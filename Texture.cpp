#include "Texture.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <limits>

#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
// PPM P6 loader
// ---------------------------------------------------------------------------
static std::string readToken(std::ifstream& f) {
    std::string token;
    int c;
    // skip whitespace/comments
    while ((c = f.get()) != EOF) {
        if (c == '#') {  // comment to end of line
            f.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        f.unget();
        break;
    }
    while ((c = f.get()) != EOF) {
        if (std::isspace(static_cast<unsigned char>(c))) break;
        token += static_cast<char>(c);
    }
    return token;
}

static bool readPPMHeader(std::ifstream& f, int& width, int& height, int& maxVal) {
    std::string magic = readToken(f);
    if (magic != "P6") return false;

    std::string wTok = readToken(f);
    std::string hTok = readToken(f);
    std::string mTok = readToken(f);
    if (wTok.empty() || hTok.empty() || mTok.empty()) return false;

    try {
        width = std::stoi(wTok);
        height = std::stoi(hTok);
        maxVal = std::stoi(mTok);
    } catch (...) {
        return false;
    }
    return width > 0 && height > 0 && maxVal == 255;
}

TextureRecord loadPPM(const std::string& path) {
    TextureRecord rec;
    rec.name = path;
    rec.filePath = path;

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "Texture: cannot open PPM file: " << path << "\n";
        return rec;
    }

    int width = 0, height = 0, maxVal = 0;
    if (!readPPMHeader(f, width, height, maxVal)) {
        std::cerr << "Texture: invalid PPM header in: " << path << "\n";
        return rec;
    }

    // PPM spec: a single whitespace character after maxVal before binary data.
    // readToken already consumed that whitespace, so no extra skip is needed.

    size_t rowBytes = static_cast<size_t>(width) * 3;
    size_t totalBytes = rowBytes * static_cast<size_t>(height);

    // Overflow guard.
    if (totalBytes / static_cast<size_t>(height) != rowBytes ||
        totalBytes > static_cast<size_t>(1) << 30) {
        std::cerr << "Texture: PPM dimensions overflow (" << width << "x" << height << ")\n";
        return rec;
    }

    rec.pixels.resize(totalBytes);
    f.read(reinterpret_cast<char*>(rec.pixels.data()),
           static_cast<std::streamsize>(totalBytes));
    if (!f) {
        std::cerr << "Texture: short read in PPM file: " << path
                  << " (expected " << totalBytes << " bytes)\n";
        rec.pixels.clear();
        return rec;
    }

    rec.width = width;
    rec.height = height;
    rec.channels = 3;
    std::cerr << "Texture: loaded PPM " << path << " (" << width << "x" << height
              << " RGB, " << totalBytes << " bytes)\n";
    return rec;
}

// ---------------------------------------------------------------------------
// TextureManager
// ---------------------------------------------------------------------------
TextureManager::~TextureManager() {
    // Note: device is not stored here; call destroy() explicitly.
}

int TextureManager::addTexture(TextureRecord tex) {
    if (!tex.valid()) {
        std::cerr << "Texture: cannot add invalid texture record: " << tex.name << "\n";
        return -1;
    }

    // Compatibility check: all array members share width/height/channels.
    if (!m_textures.empty()) {
        if (tex.width != m_width || tex.height != m_height || tex.channels != m_channels) {
            std::cerr << "Texture: incompatible dimensions for array: "
                      << tex.name << " (" << tex.width << "x" << tex.height << "x" << tex.channels
                      << ") vs existing (" << m_width << "x" << m_height << "x" << m_channels << ")\n";
            return -1;
        }
    } else {
        m_width = tex.width;
        m_height = tex.height;
        m_channels = tex.channels;
        m_format = (tex.channels == 4) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    }

    int layer = static_cast<int>(m_textures.size());
    m_textures.push_back(ArrayEntry{std::move(tex), layer});
    std::cerr << "Texture: registered layer " << layer << ": " << m_textures.back().record.name << "\n";
    return layer;
}

const TextureRecord& TextureManager::textureAt(int layer) const {
    static const TextureRecord invalid;
    if (layer < 0 || layer >= static_cast<int>(m_textures.size())) return invalid;
    return m_textures[layer].record;
}

bool TextureManager::uploadToVulkan(VkDevice device, VkPhysicalDevice physicalDevice,
                                    VkQueue transferQueue, VkCommandPool commandPool) {
    if (m_textures.empty()) {
        std::cerr << "Texture: nothing to upload\n";
        return false;
    }

    // Determine required array layers and check device limits.
    uint32_t layerCount = static_cast<uint32_t>(m_textures.size());
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    if (layerCount > props.limits.maxImageArrayLayers) {
        std::cerr << "Texture: requested " << layerCount
                  << " array layers, device max is " << props.limits.maxImageArrayLayers << "\n";
        return false;
    }
    if (m_width > props.limits.maxImageDimension2D ||
        m_height > props.limits.maxImageDimension2D) {
        std::cerr << "Texture: dimensions exceed maxImageDimension2D\n";
        return false;
    }

    VkDeviceSize layerSize = static_cast<VkDeviceSize>(m_width) * m_height * 4; // RGBA8
    VkDeviceSize uploadSize = layerSize * layerCount;

    // Convert all pixels to RGBA8 (alpha = 255) and interleave into one upload buffer.
    std::vector<unsigned char> uploadData(uploadSize, 0);
    for (const auto& entry : m_textures) {
        const auto& rec = entry.record;
        int layer = entry.layerIndex;
        unsigned char* dst = uploadData.data() + static_cast<size_t>(layer) * static_cast<size_t>(layerSize);
        if (rec.channels == 3) {
            for (int y = 0; y < rec.height; ++y) {
                for (int x = 0; x < rec.width; ++x) {
                    size_t src = static_cast<size_t>(y) * rec.width * 3 + static_cast<size_t>(x) * 3;
                    size_t dstIdx = static_cast<size_t>(y) * rec.width * 4 + static_cast<size_t>(x) * 4;
                    dst[dstIdx + 0] = rec.pixels[src + 0];
                    dst[dstIdx + 1] = rec.pixels[src + 1];
                    dst[dstIdx + 2] = rec.pixels[src + 2];
                    dst[dstIdx + 3] = 255;
                }
            }
        } else { // 4 channels, already RGBA
            std::memcpy(dst, rec.pixels.data(), rec.pixels.size());
        }
    }

    // ---- staging buffer ----
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size = uploadSize;
        ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &ci, nullptr, &stagingBuffer) != VK_SUCCESS) {
            std::cerr << "Texture: vkCreateBuffer (staging) failed\n";
            return false;
        }
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &req);
        // Find host-visible coherent memory.
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        uint32_t mt = std::numeric_limits<uint32_t>::max();
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((req.memoryTypeBits & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags &
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                mt = i;
                break;
            }
        }
        if (mt == std::numeric_limits<uint32_t>::max()) {
            std::cerr << "Texture: no host-visible coherent memory type\n";
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            return false;
        }
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = mt;
        if (vkAllocateMemory(device, &ai, nullptr, &stagingMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            return false;
        }
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
        void* data = nullptr;
        vkMapMemory(device, stagingMemory, 0, uploadSize, 0, &data);
        std::memcpy(data, uploadData.data(), uploadSize);
        vkUnmapMemory(device, stagingMemory);
    }

    // ---- device image (2D array) ----
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = m_format;
    ci.extent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1};
    ci.mipLevels = 1;
    ci.arrayLayers = layerCount;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &ci, nullptr, &m_image) != VK_SUCCESS) {
        std::cerr << "Texture: vkCreateImage failed\n";
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return false;
    }

    VkMemoryRequirements imgReq;
    vkGetImageMemoryRequirements(device, m_image, &imgReq);
    VkPhysicalDeviceMemoryProperties imgMemProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &imgMemProps);
    uint32_t imgMt = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < imgMemProps.memoryTypeCount; ++i) {
        if ((imgReq.memoryTypeBits & (1u << i)) &&
            (imgMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            imgMt = i;
            break;
        }
    }
    if (imgMt == std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Texture: no device-local memory type\n";
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return false;
    }
    VkMemoryAllocateInfo imgAi{};
    imgAi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAi.allocationSize = imgReq.size;
    imgAi.memoryTypeIndex = imgMt;
    if (vkAllocateMemory(device, &imgAi, nullptr, &m_memory) != VK_SUCCESS) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return false;
    }
    vkBindImageMemory(device, m_image, m_memory, 0);

    // ---- one-time transfer command buffer ----
    VkCommandBuffer cmd;
    {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = commandPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &ai, &cmd) != VK_SUCCESS) {
            std::cerr << "Texture: vkAllocateCommandBuffers failed\n";
            destroy(device);
            return false;
        }
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    // Transition UNDEFINED -> TRANSFER_DST_OPTIMAL for all layers.
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = m_image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount};
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Copy each layer from the staging buffer.
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1};

    for (uint32_t layer = 0; layer < layerCount; ++layer) {
        region.bufferOffset = static_cast<VkDeviceSize>(layer) * layerSize;
        region.imageSubresource.baseArrayLayer = layer;
        vkCmdCopyBufferToImage(cmd, stagingBuffer, m_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    // Transition TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL for all layers.
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.image = m_image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount};
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VkFence fence;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(device, &fci, nullptr, &fence);
    vkQueueSubmit(transferQueue, 1, &submit, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);

    // ---- staging cleanup ----
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // ---- image view (2D array) ----
    {
        VkImageViewCreateInfo iv{};
        iv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv.image = m_image;
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        iv.format = m_format;
        iv.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        iv.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount};
        if (vkCreateImageView(device, &iv, nullptr, &m_imageView) != VK_SUCCESS) {
            std::cerr << "Texture: vkCreateImageView failed\n";
            destroy(device);
            return false;
        }
    }

    // ---- sampler ----
    {
        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_NEAREST;
        si.minFilter = VK_FILTER_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.anisotropyEnable = VK_FALSE;
        si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        si.compareEnable = VK_FALSE;
        si.compareOp = VK_COMPARE_OP_ALWAYS;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.mipLodBias = 0.0f;
        si.maxAnisotropy = 1.0f;
        si.minLod = 0.0f;
        si.maxLod = 0.0f;
        if (vkCreateSampler(device, &si, nullptr, &m_sampler) != VK_SUCCESS) {
            std::cerr << "Texture: vkCreateSampler failed\n";
            destroy(device);
            return false;
        }
    }

    std::cerr << "Texture: uploaded " << layerCount << " layer(s) to Vulkan 2D-array ("
              << m_width << "x" << m_height << ")\n";
    return true;
}

void TextureManager::destroy(VkDevice device) {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_format = VK_FORMAT_UNDEFINED;
    m_width = m_height = m_channels = 0;
}
