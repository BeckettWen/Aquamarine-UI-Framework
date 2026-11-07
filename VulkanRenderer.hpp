#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <string>

class Rectangle;

// Vulkan-backed renderer for X-19.
//
// Design (first vertical slice):
//   - One general renderer handles all rendering.
//   - Pipeline accepts arrays of vertices / indices and reduces draw calls:
//     ALL rectangles are submitted as ONE indexed batch draw per frame.
//   - Dynamic rendering (no VkRenderPass) + dynamic viewport/scissor, so the
//     pipeline does not depend on swapchain size and only swapchain-dependent
//     resources are recreated on resize.
//   - Push constants (viewport + hi-DPI scale) instead of descriptor sets,
//     keeping the first slice minimal.
//   - Host-visible, host-coherent vertex/index buffers mapped persistently and
//     re-uploaded by memcpy each frame (CPU-updated batch).
//
// Coordinate convention: top-left origin, screen/logical pixels — the same
// convention used for GLFW cursor hit-testing, so rendering and input agree.
//
// Overlap rule: latest-added rectangle is topmost (drawn last, checked first
// for clicks).
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    // Initialize Vulkan (instance, surface, device, swapchain, pipeline, ...).
    // shaderDir is the directory containing triangle.vert.spv / triangle.frag.spv.
    bool init(GLFWwindow* window, int fbWidth, int fbHeight,
              int winWidth, int winHeight, const char* shaderDir = "Shaders");
    void destroy();

    // Add / remove a rectangle (non-owning pointer). Order = draw order;
    // last added is topmost.
    int addRectangle(Rectangle* rect);
    void removeRectangle(Rectangle* rect);

    // Draw one frame: rebuild the vertex/index batch, record, submit, present.
    // Returns false on unrecoverable error.
    bool drawFrame();

    // Called by the window resize callback; marks swapchain outdated and
    // records the new logical/framebuffer size.
    void framebufferResized(int fbWidth, int fbHeight,
                            int winWidth, int winHeight);

    bool isValid() const { return m_valid; }
    VkDevice device() const { return m_device; }

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

    // ---- cleanup ----
    void cleanupSwapchain();
    void cleanup();
    bool recreateSwapchain();

    // ---- frame ----
    void buildBatch();
    void drawBatch(uint32_t imageIndex);

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
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {};

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // synchronization (one set per frame in flight)
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    size_t m_currentFrame = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // host-visible, host-coherent, persistently mapped vertex/index buffers
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    float* m_vertexMapped = nullptr;
    size_t m_vertexBufferSize = 0;

    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t* m_indexMapped = nullptr;
    size_t m_indexBufferSize = 0;

    size_t m_vertexCount = 0;   // current batch vertex count
    size_t m_indexCount = 0;    // current batch index count

    // state
    int m_fbWidth = 0, m_fbHeight = 0;
    int m_winWidth = 0, m_winHeight = 0;
    bool m_swapchainOutdated = false;
    bool m_valid = false;

    std::string m_shaderDir;

    // rectangles: non-owning pointers; order = draw order (last = topmost)
    std::vector<Rectangle*> m_rectangles;
    static constexpr size_t MAX_RECTANGLES = 64;
};
