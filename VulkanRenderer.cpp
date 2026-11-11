#include "VulkanRenderer.hpp"
#include "Texture.hpp"
#include "Rectangle.hpp"

#include <cstring>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>

#ifndef FONT_PATH
#define FONT_PATH "/System/Library/Fonts/SFNS.ttf"
#endif

VulkanRenderer::VulkanRenderer() {
    m_textureManager = new TextureManager();
}

VulkanRenderer::~VulkanRenderer() {
    destroy();
    delete m_textureManager;
    m_textureManager = nullptr;
}

// ---------------------------------------------------------------------------
// File / shader helpers
// ---------------------------------------------------------------------------
std::vector<char> VulkanRenderer::readFile(const std::string& path) {
    std::vector<char> buffer;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return buffer;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) { std::fclose(f); return buffer; }
    buffer.resize(static_cast<size_t>(size));
    if (std::fread(buffer.data(), 1, buffer.size(), f) != buffer.size()) {
        buffer.clear();
    }
    std::fclose(f);
    return buffer;
}

VkShaderModule VulkanRenderer::createShaderModule(VkDevice device,
                                                  const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return module;
}

bool VulkanRenderer::checkValidationResult(VkResult res, const char* step) {
    if (res == VK_SUCCESS) return true;
    std::cerr << "Renderer: " << step << " failed with VkResult " << res << "\n";
    return false;
}

// ---------------------------------------------------------------------------
// init / destroy
// ---------------------------------------------------------------------------
bool VulkanRenderer::init(GLFWwindow* window, int fbWidth, int fbHeight,
                          int winWidth, int winHeight,
                          const char* shaderDir) {
    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;
    m_winWidth = winWidth;
    m_winHeight = winHeight;
    m_shaderDir = shaderDir ? shaderDir : "Shaders";

    if (!createInstance()) {
        std::cerr << "Renderer: createInstance failed\n";
        return false;
    }

    VkResult res = glfwCreateWindowSurface(m_instance, window, nullptr,
                                           &m_surface);
    if (!checkValidationResult(res, "glfwCreateWindowSurface")) return false;

    if (!pickPhysicalDevice()) {
        std::cerr << "Renderer: pickPhysicalDevice failed\n";
        return false;
    }
    if (!createLogicalDevice()) {
        std::cerr << "Renderer: createLogicalDevice failed\n";
        return false;
    }
    if (!createSwapchain()) {
        std::cerr << "Renderer: createSwapchain failed\n";
        return false;
    }
    if (!createImageViews()) {
        std::cerr << "Renderer: createImageViews failed\n";
        return false;
    }
    if (!createCommandPool()) {
        std::cerr << "Renderer: createCommandPool failed\n";
        return false;
    }
    if (!createBuffers()) {
        std::cerr << "Renderer: createBuffers failed\n";
        return false;
    }
    if (!createSyncObjects()) {
        std::cerr << "Renderer: createSyncObjects failed\n";
        return false;
    }
    // Descriptor resources must exist before the pipeline (pipeline layout
    // references the descriptor set layout).
    if (!createDescriptorResources()) {
        std::cerr << "Renderer: createDescriptorResources failed\n";
        return false;
    }
    if (!createGraphicsPipeline(m_shaderDir)) {
        std::cerr << "Renderer: createGraphicsPipeline failed\n";
        return false;
    }
    if (!createTextResources()) {
        std::cerr << "Renderer: createTextResources failed\n";
        return false;
    }

    m_lastFrameTime = std::chrono::steady_clock::now();
    m_batchesDirty = true;
    m_valid = true;
    std::cout << "Renderer: Vulkan initialized ("
              << m_fbWidth << "x" << m_fbHeight << " framebuffer)\n";
    return true;
}

void VulkanRenderer::destroy() {
    cleanup();
    if (m_instance != VK_NULL_HANDLE) {
        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::cleanup() {
    if (m_device != VK_NULL_HANDLE) vkDeviceWaitIdle(m_device);

    cleanupSwapchain();

    destroyTextResources();

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    m_descriptorSet = VK_NULL_HANDLE;

    if (m_textureManager != VK_NULL_HANDLE) {
        m_textureManager->destroy(m_device);
    }

    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }
    m_vertexMapped = nullptr;

    if (m_indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
        m_indexBufferMemory = VK_NULL_HANDLE;
    }
    m_indexMapped = nullptr;

    if (m_imageAvailableSemaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
    m_imageAvailableSemaphore = VK_NULL_HANDLE;
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
    m_renderFinishedSemaphore = VK_NULL_HANDLE;
    if (m_inFlightFence != VK_NULL_HANDLE)
        vkDestroyFence(m_device, m_inFlightFence, nullptr);
    m_inFlightFence = VK_NULL_HANDLE;

    if (m_commandBuffer != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffer);
        m_commandBuffer = VK_NULL_HANDLE;
    }
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_valid = false;
    m_texturesUploaded = false;
}

void VulkanRenderer::cleanupSwapchain() {
    for (auto iv : m_swapchainImageViews)
        if (iv != VK_NULL_HANDLE) vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// instance / physical device / logical device
// ---------------------------------------------------------------------------
bool VulkanRenderer::createInstance() {
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "X-19 Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Lyra / Cloudsdale";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
    return checkValidationResult(res, "vkCreateInstance");
}

bool VulkanRenderer::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) {
        std::cerr << "Renderer: no Vulkan physical devices found\n";
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (const auto& d : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);
        std::cout << "Renderer: physical device: " << props.deviceName << "\n";

        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qfCount, qfs.data());

        for (uint32_t i = 0; i < qfCount; ++i) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(d, i, m_surface, &present);
            if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                m_physicalDevice = d;
                m_graphicsQueueFamily = i;
                break;
            }
        }
        if (m_physicalDevice != VK_NULL_HANDLE) break;
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "Renderer: no suitable physical device (graphics+present)\n";
        return false;
    }
    return true;
}

bool VulkanRenderer::createLogicalDevice() {
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset",
    };

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreate{};
    queueCreate.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreate.queueFamilyIndex = m_graphicsQueueFamily;
    queueCreate.queueCount = 1;
    queueCreate.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreate;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult res = vkCreateDevice(m_physicalDevice, &createInfo, nullptr,
                                  &m_device);
    if (!checkValidationResult(res, "vkCreateDevice")) return false;

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    return true;
}

// ---------------------------------------------------------------------------
// swapchain / image views
// ---------------------------------------------------------------------------
bool VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface,
                                                  &caps) != VK_SUCCESS) {
        std::cerr << "Renderer: get surface capabilities failed\n";
        return false;
    }

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width = std::clamp(static_cast<uint32_t>(m_fbWidth),
                                  caps.minImageExtent.width,
                                  caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(m_fbHeight),
                                   caps.minImageExtent.height,
                                   caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount,
                                         nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface,
                                         &fmtCount, formats.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface,
                                              &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface,
                                              &pmCount, presentModes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& m : presentModes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = m_surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = surfaceFormat.format;
    ci.imageColorSpace = surfaceFormat.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = compositeAlpha;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain);
    if (!checkValidationResult(res, "vkCreateSwapchainKHR")) return false;

    uint32_t imageCount2 = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount2, nullptr);
    m_swapchainImages.resize(imageCount2);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount2,
                            m_swapchainImages.data());

    m_swapchainFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    std::cout << "Renderer: swapchain " << extent.width << "x" << extent.height
              << " images=" << imageCount2 << "\n";
    return true;
}

bool VulkanRenderer::createImageViews() {
    m_swapchainImageViews.clear();
    m_swapchainImageViews.reserve(m_swapchainImages.size());
    for (auto img : m_swapchainImages) {
        VkImageViewCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = m_swapchainFormat;
        ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                         VK_COMPONENT_SWIZZLE_IDENTITY,
                         VK_COMPONENT_SWIZZLE_IDENTITY,
                         VK_COMPONENT_SWIZZLE_IDENTITY};
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;
        VkImageView iv = VK_NULL_HANDLE;
        VkResult res = vkCreateImageView(m_device, &ci, nullptr, &iv);
        if (!checkValidationResult(res, "vkCreateImageView")) return false;
        m_swapchainImageViews.push_back(iv);
    }
    return true;
}

// ---------------------------------------------------------------------------
// descriptor resources (texture array bindings)
// ---------------------------------------------------------------------------
bool VulkanRenderer::createDescriptorResources() {
    // Descriptor set layout: one combined image sampler binding.
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 1;
    layoutCi.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutCi, nullptr,
                                    &m_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Renderer: vkCreateDescriptorSetLayout failed\n";
        return false;
    }

    // Descriptor pool: one set, one combined image sampler.
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolCi{};
    poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCi.maxSets = 1;
    poolCi.poolSizeCount = 1;
    poolCi.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(m_device, &poolCi, nullptr,
                               &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "Renderer: vkCreateDescriptorPool failed\n";
        return false;
    }

    // Allocate one descriptor set.
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_descriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_descriptorSetLayout;
    if (vkAllocateDescriptorSets(m_device, &ai, &m_descriptorSet) != VK_SUCCESS) {
        std::cerr << "Renderer: vkAllocateDescriptorSets failed\n";
        return false;
    }

    // Descriptor will be updated once textures are uploaded (imageView +
    // sampler from TextureManager).
    return true;
}

bool VulkanRenderer::updateDescriptorSet() {
    if (m_descriptorSet == VK_NULL_HANDLE || !m_texturesUploaded) return false;
    if (!m_textureManager->hasTextures()) return true;  // no textures, no update needed

    VkDescriptorImageInfo imgInfo{};
    imgInfo.sampler = m_textureManager->sampler();
    imgInfo.imageView = m_textureManager->imageView();
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    return true;
}

// ---------------------------------------------------------------------------
// graphics pipeline (with descriptor set layout + 9-float vertex)
// ---------------------------------------------------------------------------
bool VulkanRenderer::createGraphicsPipeline(const std::string& shaderDir) {
    auto vertCode = readFile(shaderDir + "/textured.vert.spv");
    auto fragCode = readFile(shaderDir + "/textured.frag.spv");
    if (vertCode.empty() || fragCode.empty()) {
        std::cerr << "Renderer: failed to read SPIR-V shaders from "
                  << shaderDir << "\n";
        return false;
    }

    VkShaderModule vertMod = createShaderModule(m_device, vertCode);
    VkShaderModule fragMod = createShaderModule(m_device, fragCode);
    if (vertMod == VK_NULL_HANDLE || fragMod == VK_NULL_HANDLE) {
        std::cerr << "Renderer: failed to create shader modules\n";
        if (vertMod != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, vertMod, nullptr);
        if (fragMod != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, fragMod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertMod;
    vertStage.pName = "main";
    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragMod;
    fragStage.pName = "main";
    VkPipelineShaderStageCreateInfo stages[2] = {vertStage, fragStage};

    // Vertex input: binding 0, stride 9 floats = 36 bytes.
    //   loc 0: pos  vec2  (offset 0)
    //   loc 1: color vec4 (offset 8)
    //   loc 2: uv    vec2  (offset 24)
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = 9 * sizeof(float);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc[3] = {};
    attrDesc[0].binding = 0; attrDesc[0].location = 0;
    attrDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDesc[0].offset = 0;
    attrDesc[1].binding = 0; attrDesc[1].location = 1;
    attrDesc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDesc[1].offset = 2 * sizeof(float);
    attrDesc[2].binding = 0; attrDesc[2].location = 2;
    attrDesc[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrDesc[2].offset = 6 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bindingDesc;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = attrDesc;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;

    VkPipelineDepthStencilStateCreateInfo ds{};

    VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                       VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamicStates;

    // Push constants: viewport(2) + scale(2) + textureLayer(1 int) = 20 bytes.
    VkPushConstantRange pushConst{};
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConst.offset = 0;
    pushConst.size = 20;

    VkDescriptorSetLayout layouts[1] = {m_descriptorSetLayout};
    VkPipelineLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCi.setLayoutCount = 1;
    layoutCi.pSetLayouts = layouts;
    layoutCi.pushConstantRangeCount = 1;
    layoutCi.pPushConstantRanges = &pushConst;

    if (vkCreatePipelineLayout(m_device, &layoutCi, nullptr,
                               &m_pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Renderer: vkCreatePipelineLayout failed\n";
        vkDestroyShaderModule(m_device, vertMod, nullptr);
        vkDestroyShaderModule(m_device, fragMod, nullptr);
        return false;
    }

    VkPipelineRenderingCreateInfo renderingCi{};
    renderingCi.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCi.colorAttachmentCount = 1;
    renderingCi.pColorAttachmentFormats = &m_swapchainFormat;

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.pNext = &renderingCi;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pDepthStencilState = &ds;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &dyn;
    ci.layout = m_pipelineLayout;
    ci.renderPass = VK_NULL_HANDLE;
    ci.subpass = 0;

    VkResult res = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1,
                                             &ci, nullptr, &m_pipeline);
    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
    return checkValidationResult(res, "vkCreateGraphicsPipelines");
}

bool VulkanRenderer::createCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = m_graphicsQueueFamily;
    if (vkCreateCommandPool(m_device, &ci, nullptr, &m_commandPool) != VK_SUCCESS) {
        std::cerr << "Renderer: vkCreateCommandPool failed\n";
        return false;
    }

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = m_commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_device, &ai, &m_commandBuffer) != VK_SUCCESS) {
        std::cerr << "Renderer: vkAllocateCommandBuffers failed\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// buffers / sync
// ---------------------------------------------------------------------------
static uint32_t memoryTypeIndex(VkPhysicalDevice pd, uint32_t typeBits,
                                VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return std::numeric_limits<uint32_t>::max();
}

bool VulkanRenderer::createBuffers() {
    m_vertexBufferSize = MAX_RECTANGLES * 4 * 9 * sizeof(float);
    m_indexBufferSize = MAX_RECTANGLES * 6 * sizeof(uint32_t);

    auto createBuffer = [this](VkDeviceSize size, VkBufferUsageFlags usage,
                               VkBuffer& buf, VkDeviceMemory& mem,
                               void*& mapped) -> bool {
        VkBufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size = size;
        ci.usage = usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device, &ci, nullptr, &buf) != VK_SUCCESS)
            return false;
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(m_device, buf, &req);
        uint32_t mt = memoryTypeIndex(m_physicalDevice, req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (mt == std::numeric_limits<uint32_t>::max()) {
            vkDestroyBuffer(m_device, buf, nullptr);
            buf = VK_NULL_HANDLE;
            return false;
        }
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = mt;
        if (vkAllocateMemory(m_device, &ai, nullptr, &mem) != VK_SUCCESS) {
            vkDestroyBuffer(m_device, buf, nullptr);
            buf = VK_NULL_HANDLE;
            return false;
        }
        vkBindBufferMemory(m_device, buf, mem, 0);
        void* data = nullptr;
        vkMapMemory(m_device, mem, 0, size, 0, &data);
        mapped = static_cast<float*>(data);
        return true;
    };

    if (!createBuffer(m_vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      m_vertexBuffer, m_vertexBufferMemory,
                      reinterpret_cast<void*&>(m_vertexMapped)))
        return false;
    if (!createBuffer(m_indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      m_indexBuffer, m_indexBufferMemory,
                      reinterpret_cast<void*&>(m_indexMapped)))
        return false;
    return true;
}

bool VulkanRenderer::createTextResources() {
    if (!m_fontAtlas.load(FONT_PATH, FONT_RASTER_SIZE)) return false;
    if (!createTextDescriptorResources()) return false;
    if (!createGlyphTextureArray()) return false;
    if (!updateTextDescriptorSet()) return false;
    if (!createTextBuffers()) return false;
    if (!createTextPipeline()) return false;
    return true;
}

bool VulkanRenderer::createTextDescriptorResources() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
                                    &m_textDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Text: failed to create glyph descriptor-set layout\n";
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr,
                               &m_textDescriptorPool) != VK_SUCCESS) {
        std::cerr << "Text: failed to create glyph descriptor pool\n";
        return false;
    }

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_textDescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &m_textDescriptorSetLayout;
    if (vkAllocateDescriptorSets(m_device, &allocateInfo,
                                 &m_textDescriptorSet) != VK_SUCCESS) {
        std::cerr << "Text: failed to allocate glyph descriptor set\n";
        return false;
    }
    return true;
}

bool VulkanRenderer::createGlyphTextureArray() {
    const uint32_t width = static_cast<uint32_t>(m_fontAtlas.width());
    const uint32_t height = static_cast<uint32_t>(m_fontAtlas.height());
    const uint32_t layers = m_fontAtlas.layerCount();
    const auto& pixels = m_fontAtlas.pixels();
    if (width == 0 || height == 0 || layers == 0 || pixels.empty()) {
        std::cerr << "Text: glyph atlas is empty\n";
        return false;
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    if (width > properties.limits.maxImageDimension2D ||
        height > properties.limits.maxImageDimension2D ||
        layers > properties.limits.maxImageArrayLayers) {
        std::cerr << "Text: glyph array exceeds device image limits\n";
        return false;
    }
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_R8_UNORM,
                                        &formatProperties);
    const VkFormatFeatureFlags requiredFeatures =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures) {
        std::cerr << "Text: device cannot sample/upload VK_FORMAT_R8_UNORM glyphs\n";
        return false;
    }

    const VkDeviceSize layerBytes = static_cast<VkDeviceSize>(width) * height;
    const VkDeviceSize uploadBytes = layerBytes * layers;
    if (uploadBytes != pixels.size()) {
        std::cerr << "Text: glyph atlas byte count does not match its dimensions\n";
        return false;
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    auto destroyStaging = [&]() {
        if (stagingBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        if (stagingMemory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, stagingMemory, nullptr);
    };

    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = uploadBytes;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        std::cerr << "Text: failed to create glyph staging buffer\n";
        return false;
    }

    VkMemoryRequirements stagingRequirements{};
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &stagingRequirements);
    const uint32_t hostMemoryType = memoryTypeIndex(
        m_physicalDevice, stagingRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (hostMemoryType == std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Text: no host-visible coherent memory for glyph upload\n";
        destroyStaging();
        return false;
    }

    VkMemoryAllocateInfo stagingAllocation{};
    stagingAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocation.allocationSize = stagingRequirements.size;
    stagingAllocation.memoryTypeIndex = hostMemoryType;
    if (vkAllocateMemory(m_device, &stagingAllocation, nullptr,
                         &stagingMemory) != VK_SUCCESS ||
        vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        std::cerr << "Text: failed to allocate glyph staging memory\n";
        destroyStaging();
        return false;
    }
    void* mappedStaging = nullptr;
    if (vkMapMemory(m_device, stagingMemory, 0, uploadBytes, 0,
                    &mappedStaging) != VK_SUCCESS) {
        std::cerr << "Text: failed to map glyph staging memory\n";
        destroyStaging();
        return false;
    }
    std::memcpy(mappedStaging, pixels.data(), pixels.size());
    vkUnmapMemory(m_device, stagingMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = layers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(m_device, &imageInfo, nullptr,
                      &m_glyphArrayImage) != VK_SUCCESS) {
        std::cerr << "Text: failed to create glyph texture array\n";
        destroyStaging();
        return false;
    }

    VkMemoryRequirements imageRequirements{};
    vkGetImageMemoryRequirements(m_device, m_glyphArrayImage, &imageRequirements);
    const uint32_t deviceMemoryType = memoryTypeIndex(
        m_physicalDevice, imageRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (deviceMemoryType == std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Text: no device-local memory for glyph texture array\n";
        destroyStaging();
        return false;
    }
    VkMemoryAllocateInfo imageAllocation{};
    imageAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAllocation.allocationSize = imageRequirements.size;
    imageAllocation.memoryTypeIndex = deviceMemoryType;
    if (vkAllocateMemory(m_device, &imageAllocation, nullptr,
                         &m_glyphArrayMemory) != VK_SUCCESS ||
        vkBindImageMemory(m_device, m_glyphArrayImage, m_glyphArrayMemory, 0) != VK_SUCCESS) {
        std::cerr << "Text: failed to allocate glyph image memory\n";
        destroyStaging();
        return false;
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo commandAllocation{};
    commandAllocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocation.commandPool = m_commandPool;
    commandAllocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAllocation.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_device, &commandAllocation,
                                 &commandBuffer) != VK_SUCCESS) {
        std::cerr << "Text: failed to allocate glyph upload command buffer\n";
        destroyStaging();
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result == VK_SUCCESS) {
        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = m_glyphArrayImage;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &toTransfer);

        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layers};
        copy.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_glyphArrayImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        VkImageMemoryBarrier toShader{};
        toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShader.image = m_glyphArrayImage;
        toShader.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toShader);
        result = vkEndCommandBuffer(commandBuffer);
    }

    VkFence uploadFence = VK_NULL_HANDLE;
    if (result == VK_SUCCESS) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        result = vkCreateFence(m_device, &fenceInfo, nullptr, &uploadFence);
    }
    if (result == VK_SUCCESS) {
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        result = vkQueueSubmit(m_graphicsQueue, 1, &submit, uploadFence);
        if (result == VK_SUCCESS)
            result = vkWaitForFences(m_device, 1, &uploadFence, VK_TRUE, UINT64_MAX);
    }
    if (uploadFence != VK_NULL_HANDLE) vkDestroyFence(m_device, uploadFence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    destroyStaging();
    if (result != VK_SUCCESS) {
        std::cerr << "Text: glyph upload command failed with VkResult " << result << '\n';
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_glyphArrayImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
    if (vkCreateImageView(m_device, &viewInfo, nullptr,
                          &m_glyphArrayImageView) != VK_SUCCESS) {
        std::cerr << "Text: failed to create glyph array image view\n";
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.maxLod = 0.0f;
    if (vkCreateSampler(m_device, &samplerInfo, nullptr,
                        &m_glyphSampler) != VK_SUCCESS) {
        std::cerr << "Text: failed to create glyph sampler\n";
        return false;
    }
    return true;
}

bool VulkanRenderer::updateTextDescriptorSet() {
    if (m_textDescriptorSet == VK_NULL_HANDLE ||
        m_glyphArrayImageView == VK_NULL_HANDLE ||
        m_glyphSampler == VK_NULL_HANDLE) return false;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_glyphSampler;
    imageInfo.imageView = m_glyphArrayImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_textDescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    return true;
}

bool VulkanRenderer::createTextBuffers() {
    constexpr VkDeviceSize floatsPerVertex = 9;
    constexpr VkDeviceSize verticesPerGlyph = 4;
    constexpr VkDeviceSize indicesPerGlyph = 6;
    const VkDeviceSize vertexBytes = MAX_TEXT_GLYPHS * verticesPerGlyph *
                                     floatsPerVertex * sizeof(float);
    const VkDeviceSize indexBytes = MAX_TEXT_GLYPHS * indicesPerGlyph * sizeof(uint32_t);

    auto createMappedBuffer = [this](VkDeviceSize size, VkBufferUsageFlags usage,
                                     VkBuffer& buffer, VkDeviceMemory& memory,
                                     void** mapped) -> bool {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            return false;
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(m_device, buffer, &requirements);
        const uint32_t type = memoryTypeIndex(
            m_physicalDevice, requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == std::numeric_limits<uint32_t>::max()) return false;
        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = type;
        if (vkAllocateMemory(m_device, &allocation, nullptr, &memory) != VK_SUCCESS ||
            vkBindBufferMemory(m_device, buffer, memory, 0) != VK_SUCCESS)
            return false;
        return vkMapMemory(m_device, memory, 0, size, 0, mapped) == VK_SUCCESS;
    };

    void* vertexData = nullptr;
    if (!createMappedBuffer(vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            m_textVertexBuffer, m_textVertexBufferMemory,
                            &vertexData)) {
        std::cerr << "Text: failed to allocate text vertex buffer\n";
        return false;
    }
    m_textVertexMapped = static_cast<float*>(vertexData);

    void* indexData = nullptr;
    if (!createMappedBuffer(indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            m_textIndexBuffer, m_textIndexBufferMemory,
                            &indexData)) {
        std::cerr << "Text: failed to allocate text index buffer\n";
        return false;
    }
    m_textIndexMapped = static_cast<uint32_t*>(indexData);
    return true;
}

bool VulkanRenderer::createTextPipeline() {
    const auto vertexCode = readFile(m_shaderDir + "/text.vert.spv");
    const auto fragmentCode = readFile(m_shaderDir + "/text.frag.spv");
    if (vertexCode.empty() || fragmentCode.empty()) {
        std::cerr << "Text: missing text.vert.spv or text.frag.spv in "
                  << m_shaderDir << '\n';
        return false;
    }
    VkShaderModule vertexModule = createShaderModule(m_device, vertexCode);
    VkShaderModule fragmentModule = createShaderModule(m_device, fragmentCode);
    if (vertexModule == VK_NULL_HANDLE || fragmentModule == VK_NULL_HANDLE) {
        if (vertexModule != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, vertexModule, nullptr);
        if (fragmentModule != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, fragmentModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertexModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragmentModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 9 * sizeof(float);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attributes[4]{};
    // VkVertexInputAttributeDescription field order is location, binding,
    // format, offset (not binding, location, format, offset).
    attributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 2 * sizeof(float)};
    attributes[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)};
    attributes[3] = {3, 0, VK_FORMAT_R32_SFLOAT, 8 * sizeof(float)};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blend;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;

    VkPushConstantRange pushConstants{};
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstants.size = 4 * sizeof(float);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_textDescriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstants;
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr,
                               &m_textPipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertexModule, nullptr);
        vkDestroyShaderModule(m_device, fragmentModule, nullptr);
        std::cerr << "Text: failed to create text pipeline layout\n";
        return false;
    }

    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &m_swapchainFormat;
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &rendering;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &assembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = m_textPipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    const VkResult result = vkCreateGraphicsPipelines(
        m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_textPipeline);
    vkDestroyShaderModule(m_device, vertexModule, nullptr);
    vkDestroyShaderModule(m_device, fragmentModule, nullptr);
    if (result != VK_SUCCESS) {
        std::cerr << "Text: failed to create text graphics pipeline (VkResult "
                  << result << ")\n";
        return false;
    }
    return true;
}

void VulkanRenderer::destroyTextResources() {
    if (m_device == VK_NULL_HANDLE) return;

    if (m_textPipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_textPipeline, nullptr);
    m_textPipeline = VK_NULL_HANDLE;
    if (m_textPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_device, m_textPipelineLayout, nullptr);
    m_textPipelineLayout = VK_NULL_HANDLE;

    if (m_textDescriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(m_device, m_textDescriptorPool, nullptr);
    m_textDescriptorPool = VK_NULL_HANDLE;
    m_textDescriptorSet = VK_NULL_HANDLE;
    if (m_textDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(m_device, m_textDescriptorSetLayout, nullptr);
    m_textDescriptorSetLayout = VK_NULL_HANDLE;

    if (m_glyphSampler != VK_NULL_HANDLE) vkDestroySampler(m_device, m_glyphSampler, nullptr);
    m_glyphSampler = VK_NULL_HANDLE;
    if (m_glyphArrayImageView != VK_NULL_HANDLE)
        vkDestroyImageView(m_device, m_glyphArrayImageView, nullptr);
    m_glyphArrayImageView = VK_NULL_HANDLE;
    if (m_glyphArrayImage != VK_NULL_HANDLE) vkDestroyImage(m_device, m_glyphArrayImage, nullptr);
    m_glyphArrayImage = VK_NULL_HANDLE;
    if (m_glyphArrayMemory != VK_NULL_HANDLE) vkFreeMemory(m_device, m_glyphArrayMemory, nullptr);
    m_glyphArrayMemory = VK_NULL_HANDLE;

    if (m_textVertexMapped && m_textVertexBufferMemory != VK_NULL_HANDLE)
        vkUnmapMemory(m_device, m_textVertexBufferMemory);
    m_textVertexMapped = nullptr;
    if (m_textVertexBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_textVertexBuffer, nullptr);
    m_textVertexBuffer = VK_NULL_HANDLE;
    if (m_textVertexBufferMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_textVertexBufferMemory, nullptr);
    m_textVertexBufferMemory = VK_NULL_HANDLE;

    if (m_textIndexMapped && m_textIndexBufferMemory != VK_NULL_HANDLE)
        vkUnmapMemory(m_device, m_textIndexBufferMemory);
    m_textIndexMapped = nullptr;
    if (m_textIndexBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_textIndexBuffer, nullptr);
    m_textIndexBuffer = VK_NULL_HANDLE;
    if (m_textIndexBufferMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_textIndexBufferMemory, nullptr);
    m_textIndexBufferMemory = VK_NULL_HANDLE;
    m_textIndexCount = 0;
}

bool VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(m_device, &sci, nullptr,
                          &m_imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(m_device, &sci, nullptr,
                          &m_renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(m_device, &fci, nullptr,
                      &m_inFlightFence) != VK_SUCCESS) {
        std::cerr << "Renderer: sync object creation failed\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// texture management
// ---------------------------------------------------------------------------
int VulkanRenderer::addTexture(TextureRecord tex) {
    int layer = m_textureManager->addTexture(std::move(tex));
    return layer;
}

bool VulkanRenderer::uploadTextures() {
    if (!m_textureManager->hasTextures()) {
        m_texturesUploaded = true;
        return true;
    }
    if (!m_textureManager->uploadToVulkan(m_device, m_physicalDevice,
                                          m_graphicsQueue, m_commandPool)) {
        return false;
    }
    m_texturesUploaded = true;
    return updateDescriptorSet();
}

// ---------------------------------------------------------------------------
// resize / recreate swapchain
// ---------------------------------------------------------------------------
void VulkanRenderer::framebufferResized(int fbWidth, int fbHeight,
                                        int winWidth, int winHeight) {
    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;
    m_winWidth = winWidth;
    m_winHeight = winHeight;
    m_swapchainOutdated = true;
}

bool VulkanRenderer::recreateSwapchain() {
    vkDeviceWaitIdle(m_device);
    cleanupSwapchain();

    if (!createSwapchain()) return false;
    if (!createImageViews()) return false;

    m_swapchainOutdated = false;
    std::cout << "Renderer: swapchain recreated ("
              << m_swapchainExtent.width << "x" << m_swapchainExtent.height
              << ")\n";
    return true;
}

// ---------------------------------------------------------------------------
// batch build + draw
// ---------------------------------------------------------------------------
void VulkanRenderer::buildBatches() {
    m_drawGroups.clear();
    m_totalVertexCount = 0;
    m_totalIndexCount = 0;

    if (!m_vertexMapped || !m_indexMapped) return;

    // Keep painter's order: merge only adjacent rectangles using the same
    // texture layer. Each draw group's vertices and indices are contiguous.
    DrawGroup* currentGroup = nullptr;
    uint32_t baseVertex = 0;
    size_t rectangleCount = 0;
    for (Rectangle* rectangle : m_rectangles) {
        if (!rectangle || !rectangle->visible) continue;
        if (rectangleCount >= MAX_RECTANGLES) {
            std::cerr << "Renderer: rectangle batch limit of " << MAX_RECTANGLES
                      << " reached; remaining rectangles are not drawn\n";
            break;
        }

        if (!currentGroup || currentGroup->textureLayer != rectangle->textureLayer) {
            m_drawGroups.push_back(DrawGroup{
                rectangle->textureLayer, m_totalVertexCount, m_totalIndexCount, 0});
            currentGroup = &m_drawGroups.back();
        }

        float* vertices = m_vertexMapped + m_totalVertexCount * 9;
        uint32_t* indices = m_indexMapped + m_totalIndexCount;
        rectangle->writeVertices(vertices);
        rectangle->writeIndices(indices, baseVertex);

        m_totalVertexCount += 4;
        m_totalIndexCount += 6;
        currentGroup->indexCount += 6;
        baseVertex += 4;
        ++rectangleCount;
    }
}

bool VulkanRenderer::detectBatchArrayModification() {
    std::vector<RectangleBatchSnapshot> current;
    current.reserve(m_rectangles.size());
    for (const Rectangle* rectangle : m_rectangles) {
        if (!rectangle) continue;
        RectangleBatchSnapshot snapshot;
        snapshot.rectangle = rectangle;
        snapshot.geometry = {
            rectangle->x, rectangle->y, rectangle->width, rectangle->height,
            rectangle->color[0], rectangle->color[1], rectangle->color[2], rectangle->color[3],
            rectangle->uv[0], rectangle->uv[1], rectangle->uv[2], rectangle->uv[3],
            rectangle->uv[4], rectangle->uv[5], rectangle->uv[6], rectangle->uv[7],
        };
        snapshot.textureLayer = rectangle->textureLayer;
        snapshot.visible = rectangle->visible;
        current.push_back(snapshot);
    }
    if (current != m_cachedBatchSnapshot) {
        m_cachedBatchSnapshot = std::move(current);
        m_batchesDirty = true;
    }
    return m_batchesDirty;
}

void VulkanRenderer::updateAnimations(float deltaSeconds) {
    for (Rectangle* rectangle : m_rectangles) {
        if (rectangle && rectangle->advanceAnimation(deltaSeconds))
            m_batchesDirty = true;
    }
}

void VulkanRenderer::buildTextBatch() {
    m_textIndexCount = 0;
    if (!m_textVertexMapped || !m_textIndexMapped || !m_fontAtlas.valid()) return;

    uint32_t glyphCount = 0;
    bool capacityWarningPrinted = false;
    const float atlasWidth = static_cast<float>(m_fontAtlas.width());
    const float atlasHeight = static_cast<float>(m_fontAtlas.height());

    auto writeGlyphQuad = [&](float left, float top, float width, float height,
                              const FontGlyph& glyph, const TextLabel& label) {
        if (glyphCount >= MAX_TEXT_GLYPHS) {
            if (!capacityWarningPrinted) {
                std::cerr << "Text: batch limit of " << MAX_TEXT_GLYPHS
                          << " glyphs reached; remaining text was truncated\n";
                capacityWarningPrinted = true;
            }
            return false;
        }

        const float right = left + width;
        const float bottom = top + height;
        const float uRight = static_cast<float>(glyph.width) / atlasWidth;
        const float vBottom = static_cast<float>(glyph.height) / atlasHeight;
        const float corners[4][9] = {
            {left,  top,    label.color[0], label.color[1], label.color[2], label.color[3], 0.0f, 0.0f, static_cast<float>(glyph.layer)},
            {right, top,    label.color[0], label.color[1], label.color[2], label.color[3], uRight, 0.0f, static_cast<float>(glyph.layer)},
            {right, bottom, label.color[0], label.color[1], label.color[2], label.color[3], uRight, vBottom, static_cast<float>(glyph.layer)},
            {left,  bottom, label.color[0], label.color[1], label.color[2], label.color[3], 0.0f, vBottom, static_cast<float>(glyph.layer)},
        };
        const uint32_t baseVertex = glyphCount * 4;
        float* vertex = m_textVertexMapped + static_cast<size_t>(baseVertex) * 9;
        for (size_t corner = 0; corner < 4; ++corner)
            std::copy_n(corners[corner], 9, vertex + corner * 9);

        uint32_t* indices = m_textIndexMapped + static_cast<size_t>(glyphCount) * 6;
        indices[0] = baseVertex;
        indices[1] = baseVertex + 1;
        indices[2] = baseVertex + 2;
        indices[3] = baseVertex;
        indices[4] = baseVertex + 2;
        indices[5] = baseVertex + 3;
        ++glyphCount;
        return true;
    };

    for (const TextLabel* label : m_textLabels) {
        if (!label || !label->visible || label->text.empty() ||
            !std::isfinite(label->scale) || label->scale <= 0.0f) continue;

        float cursorX = label->x;
        float baselineY = label->baselineY;
        for (unsigned char character : label->text) {
            if (character == '\n') {
                cursorX = label->x;
                baselineY += static_cast<float>(m_fontAtlas.lineHeight()) * label->scale;
                continue;
            }
            if (character == '\r') continue;

            const FontGlyph* glyph = m_fontAtlas.glyph(character);
            if (!glyph) continue;
            if (character == '\t') {
                const FontGlyph* space = m_fontAtlas.glyph(' ');
                cursorX += (space ? space->advance : 12.0f) * 4.0f * label->scale;
                continue;
            }

            if (glyph->width > 0 && glyph->height > 0) {
                const float left = cursorX + glyph->bearingX * label->scale;
                const float top = baselineY - glyph->bearingY * label->scale;
                const bool written = writeGlyphQuad(
                    left, top, glyph->width * label->scale,
                    glyph->height * label->scale, *glyph, *label);
                if (!written) break;
            }
            cursorX += glyph->advance * label->scale;
        }
        if (glyphCount >= MAX_TEXT_GLYPHS) break;
    }
    m_textIndexCount = glyphCount * 6;
}

void VulkanRenderer::drawBatches(uint32_t imageIndex) {
    if (m_drawGroups.empty() && m_textIndexCount == 0) return;

    VkCommandBuffer cmd = m_commandBuffer;

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
        std::cerr << "Renderer: vkBeginCommandBuffer failed\n";
        return;
    }

    // Dynamic rendering begin.
    VkRenderingAttachmentInfo colorAttach{};
    colorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttach.imageView = m_swapchainImageViews[imageIndex];
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea = {{0, 0}, m_swapchainExtent};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &colorAttach;
    vkCmdBeginRendering(cmd, &rendering);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, m_swapchainExtent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (!m_drawGroups.empty()) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        if (m_texturesUploaded && m_textureManager->hasTextures() &&
            m_descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_pipelineLayout, 0, 1, &m_descriptorSet,
                                    0, nullptr);
        }

        VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        for (const auto& group : m_drawGroups) {
            if (group.indexCount == 0) continue;

            float pc[5] = {
                static_cast<float>(m_fbWidth),
                static_cast<float>(m_fbHeight),
                (m_winWidth > 0) ? static_cast<float>(m_fbWidth) / m_winWidth : 1.0f,
                (m_winHeight > 0) ? static_cast<float>(m_fbHeight) / m_winHeight : 1.0f,
                static_cast<float>(group.textureLayer),
            };
            vkCmdPushConstants(cmd, m_pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, 20, pc);

            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(group.indexCount), 1,
                             static_cast<uint32_t>(group.indexOffset),
                             0, 0);
        }
    }

    // Text is rendered after UI rectangles from a completely separate batch,
    // descriptor set, glyph array, and graphics pipeline.
    if (m_textIndexCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_textPipelineLayout, 0, 1,
                                &m_textDescriptorSet, 0, nullptr);
        VkDeviceSize textVertexOffset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_textVertexBuffer,
                               &textVertexOffset);
        vkCmdBindIndexBuffer(cmd, m_textIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        const float textPushConstants[4] = {
            static_cast<float>(m_fbWidth),
            static_cast<float>(m_fbHeight),
            (m_winWidth > 0) ? static_cast<float>(m_fbWidth) / m_winWidth : 1.0f,
            (m_winHeight > 0) ? static_cast<float>(m_fbHeight) / m_winHeight : 1.0f,
        };
        vkCmdPushConstants(cmd, m_textPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(textPushConstants), textPushConstants);
        vkCmdDrawIndexed(cmd, m_textIndexCount, 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        std::cerr << "Renderer: vkEndCommandBuffer failed\n";
    }
}

// ---------------------------------------------------------------------------
// drawFrame
// ---------------------------------------------------------------------------
bool VulkanRenderer::drawFrame() {
    if (!m_valid) return false;

    if (m_swapchainOutdated) {
        if (!recreateSwapchain()) return false;
    }

    VkResult res = vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE,
                                   UINT64_MAX);
    if (!checkValidationResult(res, "vkWaitForFences")) return false;

    uint32_t imageIndex = 0;
    res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                m_imageAvailableSemaphore, VK_NULL_HANDLE,
                                &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchainOutdated = true;
        return true;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return checkValidationResult(res, "vkAcquireNextImageKHR");
    }

    // Reset only once an image was acquired and a submission is imminent. If
    // acquisition reports OUT_OF_DATE, the still-signaled fence remains safe
    // to wait on during swapchain recreation.
    res = vkResetFences(m_device, 1, &m_inFlightFence);
    if (!checkValidationResult(res, "vkResetFences")) return false;

    // Update animated positions, detect any public geometry changes, and only
    // rebuild the mapped rectangle batch when its source array changed.
    const auto frameTime = std::chrono::steady_clock::now();
    float deltaSeconds = std::chrono::duration<float>(
        frameTime - m_lastFrameTime).count();
    m_lastFrameTime = frameTime;
    deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.1f);
    updateAnimations(deltaSeconds);
    if (detectBatchArrayModification()) {
        buildBatches();
        m_batchesDirty = false;
    }
    buildTextBatch();
    drawBatches(imageIndex);

    VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_imageAvailableSemaphore;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_renderFinishedSemaphore;

    res = vkQueueSubmit(m_graphicsQueue, 1, &submit, m_inFlightFence);
    if (!checkValidationResult(res, "vkQueueSubmit")) return false;

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &m_renderFinishedSemaphore;
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &imageIndex;

    res = vkQueuePresentKHR(m_graphicsQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        m_swapchainOutdated = true;
    } else if (!checkValidationResult(res, "vkQueuePresentKHR")) {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// rectangle management (non-owning)
// ---------------------------------------------------------------------------
int VulkanRenderer::addRectangle(Rectangle* rect) {
    if (!rect) return -1;
    m_rectangles.push_back(rect);
    m_batchesDirty = true;
    return static_cast<int>(m_rectangles.size()) - 1;
}

void VulkanRenderer::removeRectangle(Rectangle* rect) {
    m_rectangles.erase(
        std::remove(m_rectangles.begin(), m_rectangles.end(), rect),
        m_rectangles.end());
    m_batchesDirty = true;
}

int VulkanRenderer::addText(TextLabel* text) {
    if (!text) return -1;
    m_textLabels.push_back(text);
    return static_cast<int>(m_textLabels.size()) - 1;
}

void VulkanRenderer::removeText(TextLabel* text) {
    m_textLabels.erase(
        std::remove(m_textLabels.begin(), m_textLabels.end(), text),
        m_textLabels.end());
}

void VulkanRenderer::dispatchMouseButton(int button, int action, int mods,
                                         double cursorX, double cursorY) {
    (void)mods;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    std::function<void()> response;
    for (auto it = m_rectangles.rbegin(); it != m_rectangles.rend(); ++it) {
        Rectangle* rectangle = *it;
        if (rectangle && rectangle->contains(cursorX, cursorY)) {
            response = rectangle->onClick;
            break;
        }
    }
    if (response) response();
}
