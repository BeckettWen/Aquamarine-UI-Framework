#pragma once

#include <string>
#include <vector>
#include <array>

#if defined(LYRA_USE_VULKAN)
#include <vulkan/vulkan.h>
#endif

// A single decoded texture record.
//
// Dimensions, channel count, and raw pixel bytes are stored here so the
// renderer can upload them to Vulkan. All textures destined for the same
// array must share the same width, height, and channel count; the loader
// validates this and rejects incompatible files with a clear error.
struct TextureRecord {
    std::string name;          // debug label / asset name
    std::string filePath;      // source asset path
    int width = 0;             // pixels
    int height = 0;            // pixels
    int channels = 0;          // 3 (RGB) or 4 (RGBA)
    std::vector<unsigned char> pixels; // row-major, top-left origin

    bool valid() const { return width > 0 && height > 0 && channels > 0 && !pixels.empty(); }
};

// PPM P6 (binary) image loader, self-contained — no external dependencies.
//
// Format: "P6\nwidth height\n255\n<binary RGB bytes>"
// Supports comments after the magic line (lines starting with #).
// Pixels are stored row-major, top-left origin, matching the rectangle /
// cursor coordinate convention used elsewhere in X-19.
//
// Returns an empty (invalid) TextureRecord on any error; the caller checks
// .valid() and can inspect name/filePath for diagnostics.
TextureRecord loadPPM(const std::string& path);

#if defined(LYRA_USE_VULKAN)
// Renderer-owned Vulkan texture collection.
//
// Assigns stable layer indices to textures, validates that all array members
// share compatible dimensions/format, and owns the Vulkan image / image view
// / sampler / descriptor resources for the whole array.
//
// Thread-safety: not thread-safe; intended for single-threaded use (CPU
// batch updates + render loop).
class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager();

    // Disable copying (owns Vulkan resources).
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Register a decoded texture, assigning it the next stable layer index.
    // Returns the layer index, or -1 on error (e.g. incompatible dimensions
    // with existing array members, or too many layers for the device).
    int addTexture(TextureRecord tex);

    // Number of registered textures / array layers.
    int layerCount() const { return static_cast<int>(m_textures.size()); }

    // True if at least one texture has been registered.
    bool hasTextures() const { return !m_textures.empty(); }

    // Access a texture record by layer index (caller-owned reference).
    const TextureRecord& textureAt(int layer) const;

    // Vulkan resources (owned by this manager, valid after uploadToVulkan).
    VkImage image() const { return m_image; }
    VkImageView imageView() const { return m_imageView; }
    VkSampler sampler() const { return m_sampler; }
    VkDeviceMemory memory() const { return m_memory; }
    VkFormat format() const { return m_format; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    // Upload all registered textures to a Vulkan 2D-array image using a
    // staging buffer. Requires a valid VkDevice, physical device, and
    // graphics queue + command pool for one-time transfer commands.
    bool uploadToVulkan(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkQueue transferQueue, VkCommandPool commandPool);

    // Destroy all Vulkan resources. Safe to call multiple times / on partial
    // initialization failure.
    void destroy(VkDevice device);

private:
    struct ArrayEntry {
        TextureRecord record;
        int layerIndex = -1;
    };

    std::vector<ArrayEntry> m_textures;

    // Compatibility: all array members must match the first texture's dims.
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    VkFormat m_format = VK_FORMAT_UNDEFINED;

    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};
#endif
