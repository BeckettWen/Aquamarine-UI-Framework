#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <array>
#include <chrono>

class Rectangle;
class TextureManager;
#include "Texture.hpp"
#include "Text.hpp"

// Vulkan-backed renderer for X-19 (Lyra / Cloudsdale), Phase 2.
//
// Design:
//   - One general renderer handles all rendering.
//   - Pipeline accepts arrays of vertices/indices, reducing draw calls:
//     rectangles are grouped by texture layer and submitted as one indexed
//     batch draw per compatible group.
//   - Dynamic rendering (no VkRenderPass) + dynamic viewport/scissor.
//   - Push constants (viewport, hi-DPI scale, texture layer) instead of
//     per-draw uniform buffers.
//   - Host-visible, host-coherent, persistently mapped vertex/index buffers,
//     re-uploaded by memcpy each frame (CPU-updated batch).
//
// Phase 2 texture path:
//   - Textures are registered via TextureManager, assigned stable layer
//     indices, and uploaded into a VK_IMAGE_VIEW_TYPE_2D_ARRAY image.
//   - A descriptor set (combined image sampler) binds the array to the
//     fragment shader.
//   - Rectangles carry per-vertex UVs and a textureLayer (>=0 = sample that
//     layer; -1 = solid color, using vertex color only).
//   - Text has independent CPU/GPU vertex and index arrays and its own glyph
//     image array, descriptors, shader pipeline, and draw submission.
//
// Coordinate convention: top-left origin, screen/logical pixels — same as
// GLFW cursor hit-testing, so rendering and input agree.
//
// Overlap rule: latest-added rectangle is topmost (drawn last, checked first
// for clicks).
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    // Initialize Vulkan (instance, surface, device, swapchain, pipeline, ...).
    // shaderDir is the directory containing .vert.spv / .frag.spv files.
    bool init(GLFWwindow* window, int fbWidth, int fbHeight,
              int winWidth, int winHeight, const char* shaderDir = "Shaders");
    void destroy();

    // Add / remove a rectangle (non-owning pointer). Order = draw order;
    // last added is topmost.
    int addRectangle(Rectangle* rect);
    void removeRectangle(Rectangle* rect);

    // Register a decoded texture, assigning a stable layer index.
    // Returns layer index or -1 on error.
    int addTexture(TextureRecord tex);

    // Upload all registered textures to Vulkan (must be called after init
    // and before drawing). Returns false on failure.
    bool uploadTextures();

    // Add / remove a text label (non-owning pointer). Labels use a top-left
    // logical-pixel baseline and are rendered after rectangles.
    int addText(TextLabel* text);
    void removeText(TextLabel* text);

    // Dispatch a GLFW mouse event to the topmost enabled rectangle under the
    // cursor. Only a left-button press invokes onClick.
    void dispatchMouseButton(int button, int action, int mods,
                             double cursorX, double cursorY);

    // Draw one frame: rebuild batches, record, submit, present.
    // Returns false on unrecoverable error.
    bool drawFrame();

    // Called by the window resize callback; marks swapchain outdated.
    void framebufferResized(int fbWidth, int fbHeight,
                            int winWidth, int winHeight);

    bool isValid() const { return m_valid; }
    VkDevice device() const { return m_device; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkCommandPool commandPool() const { return m_commandPool; }

    int framebufferWidth() const  { return m_fbWidth;  }
    int framebufferHeight() const { return m_fbHeight; }

private:
    // ---- creation ----
    bool createInstance();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    bool createImageViews();
    bool createGraphicsPipeline(const std::string& shaderDir);
    bool createCommandPool();
    bool createBuffers();
    bool createSyncObjects();
    bool createDescriptorResources();
    bool updateDescriptorSet();
    bool createTextResources();
    bool createTextDescriptorResources();
    bool createTextPipeline();
    bool createGlyphTextureArray();
    bool createTextBuffers();
    bool updateTextDescriptorSet();
    void destroyTextResources();

    // ---- cleanup ----
    void cleanupSwapchain();
    void cleanup();
    bool recreateSwapchain();

    // ---- frame ----
    void buildBatches();          // group rectangles by texture layer
    void buildTextBatch();
    bool detectBatchArrayModification();
    void updateAnimations(float deltaSeconds);
    void drawBatches(uint32_t imageIndex);

    // ---- helpers ----
    static std::vector<char> readFile(const std::string& path);
    static VkShaderModule createShaderModule(VkDevice device,
                                             const std::vector<char>& code);
    static bool checkValidationResult(VkResult res, const char* step);

    // ---- Vulkan handles ----
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {};

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // Dedicated text resources. These intentionally do not share the UI
    // texture array, descriptor set, or vertex/index buffers.
    VkImage m_glyphArrayImage = VK_NULL_HANDLE;
    VkDeviceMemory m_glyphArrayMemory = VK_NULL_HANDLE;
    VkImageView m_glyphArrayImageView = VK_NULL_HANDLE;
    VkSampler m_glyphSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_textDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_textDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_textDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_textPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_textPipeline = VK_NULL_HANDLE;

    VkBuffer m_textVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_textVertexBufferMemory = VK_NULL_HANDLE;
    float* m_textVertexMapped = nullptr;
    VkBuffer m_textIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_textIndexBufferMemory = VK_NULL_HANDLE;
    uint32_t* m_textIndexMapped = nullptr;
    uint32_t m_textIndexCount = 0;
    static constexpr uint32_t MAX_TEXT_GLYPHS = 4096;
    static constexpr unsigned int FONT_RASTER_SIZE = 48;
    FontAtlas m_fontAtlas;
    std::vector<TextLabel*> m_textLabels;

    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

    // synchronization (single frame in flight for simplicity)
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_inFlightFence = VK_NULL_HANDLE;

    // host-visible, host-coherent, persistently mapped vertex/index buffers
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    float* m_vertexMapped = nullptr;
    size_t m_vertexBufferSize = 0;

    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t* m_indexMapped = nullptr;
    size_t m_indexBufferSize = 0;

    // batch state: grouped draw calls
    struct DrawGroup {
        int textureLayer = -1;          // -1 = solid color
        size_t vertexOffset = 0;        // vertex count offset
        size_t indexOffset = 0;         // index count offset
        size_t indexCount = 0;          // indices in this group
    };
    std::vector<DrawGroup> m_drawGroups;
    size_t m_totalVertexCount = 0;
    size_t m_totalIndexCount = 0;

    struct RectangleBatchSnapshot {
        const Rectangle* rectangle = nullptr;
        std::array<float, 16> geometry{}; // x/y/size, color, UVs
        int textureLayer = -1;
        bool visible = true;
        bool operator==(const RectangleBatchSnapshot&) const = default;
    };
    std::vector<RectangleBatchSnapshot> m_cachedBatchSnapshot;
    bool m_batchesDirty = true;
    std::chrono::steady_clock::time_point m_lastFrameTime{};

    // state
    int m_fbWidth = 0, m_fbHeight = 0;
    int m_winWidth = 0, m_winHeight = 0;
    bool m_swapchainOutdated = false;
    bool m_valid = false;
    bool m_texturesUploaded = false;

    std::string m_shaderDir;

    // rectangles: non-owning pointers; order = draw order (last = topmost)
    std::vector<Rectangle*> m_rectangles;
    static constexpr size_t MAX_RECTANGLES = 64;

    // texture manager (renderer-owned)
    TextureManager* m_textureManager = nullptr;
};
