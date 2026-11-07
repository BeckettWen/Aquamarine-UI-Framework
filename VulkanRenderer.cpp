#include "VulkanRenderer.hpp"
#include "Rectangle.hpp"

#include <cstring>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <limits>

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    destroy();
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
    if (!createGraphicsPipeline(m_shaderDir)) {
        std::cerr << "Renderer: createGraphicsPipeline failed\n";
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

    for (auto s : m_imageAvailableSemaphores)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(m_device, s, nullptr);
    m_imageAvailableSemaphores.clear();
    for (auto s : m_renderFinishedSemaphores)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(m_device, s, nullptr);
    m_renderFinishedSemaphores.clear();
    for (auto f : m_inFlightFences)
        if (f != VK_NULL_HANDLE) vkDestroyFence(m_device, f, nullptr);
    m_inFlightFences.clear();

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
    m_presentQueue = VK_NULL_HANDLE;
    m_valid = false;
}

void VulkanRenderer::cleanupSwapchain() {
    for (auto iv : m_swapchainImageViews)
        if (iv != VK_NULL_HANDLE) vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    if (!m_commandBuffers.empty() && m_commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(m_device, m_commandPool,
                             static_cast<uint32_t>(m_commandBuffers.size()),
                             m_commandBuffers.data());
        m_commandBuffers.clear();
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
    appInfo.pEngineName = "X-19 Lyra";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    // MoltenVK is a portability driver: enumerate it.
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

        // Queue family supporting graphics + present on the same queue.
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
        "VK_KHR_portability_subset",  // MoltenVK portability
    };

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreate{};
    queueCreate.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreate.queueFamilyIndex = m_graphicsQueueFamily;
    queueCreate.queueCount = 1;
    queueCreate.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};  // fixed-function features, none needed

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
    m_presentQueue = m_graphicsQueue;  // same queue does present
    return true;
}

// ---------------------------------------------------------------------------
// swapchain / image views / pipeline
// ---------------------------------------------------------------------------
bool VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface,
                                                  &caps) != VK_SUCCESS) {
        std::cerr << "Renderer: get surface capabilities failed\n";
        return false;
    }

    // Extent: use current surface extent if defined, else clamp window fb size.
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

    // Image count.
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    // Surface format: prefer BGRA8 UNORM SRGB.
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount,
                                         nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount,
                                         formats.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }

    // Present mode: prefer FIFO (guaranteed, vsync). Try mailbox for low latency.
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

    // Composite alpha: opaque preferred.
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

bool VulkanRenderer::createGraphicsPipeline(const std::string& shaderDir) {
    auto vertCode = readFile(shaderDir + "/triangle.vert.spv");
    auto fragCode = readFile(shaderDir + "/triangle.frag.spv");
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

    // Vertex input: binding 0, stride 6 floats; pos at loc 0, color at loc 1.
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = 6 * sizeof(float);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc[2] = {};
    attrDesc[0].binding = 0; attrDesc[0].location = 0;
    attrDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDesc[0].offset = 0;
    attrDesc[1].binding = 0; attrDesc[1].location = 1;
    attrDesc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDesc[1].offset = 2 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bindingDesc;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = attrDesc;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    // Dynamic viewport + scissor so pipeline is independent of swapchain size.
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;        // no culling (winding is cosmetic)
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

    VkPipelineDepthStencilStateCreateInfo ds{};  // disabled

    VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                       VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamicStates;

    // Push constants: 2 vec2 (viewport, scale) = 16 bytes, vertex stage.
    VkPushConstantRange pushConst{};
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConst.offset = 0;
    pushConst.size = 16;

    VkPipelineLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCi.pushConstantRangeCount = 1;
    layoutCi.pPushConstantRanges = &pushConst;

    if (vkCreatePipelineLayout(m_device, &layoutCi, nullptr,
                               &m_pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Renderer: vkCreatePipelineLayout failed\n";
        vkDestroyShaderModule(m_device, vertMod, nullptr);
        vkDestroyShaderModule(m_device, fragMod, nullptr);
        return false;
    }

    // Dynamic rendering: pipeline references the color attachment format via
    // VkPipelineRenderingCreateInfo; no VkRenderPass is created.
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
    ci.renderPass = VK_NULL_HANDLE;  // dynamic rendering
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

    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = m_commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
    if (vkAllocateCommandBuffers(m_device, &ai, m_commandBuffers.data()) != VK_SUCCESS) {
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
    // Fixed-size batch buffer for MAX_RECTANGLES (bounded first slice).
    m_vertexBufferSize = MAX_RECTANGLES * 4 * 6 * sizeof(float);   // verts * attribs * bytes
    m_indexBufferSize = MAX_RECTANGLES * 6 * sizeof(uint32_t);     // rects * indices * bytes

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

bool VulkanRenderer::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // first frame doesn't wait

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_device, &sci, nullptr,
                              &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &sci, nullptr,
                              &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fci, nullptr,
                          &m_inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "Renderer: sync object creation failed\n";
            return false;
        }
    }
    return true;
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
    // Wait until the GPU is done with the old swapchain resources.
    vkDeviceWaitIdle(m_device);
    cleanupSwapchain();

    if (!createSwapchain()) return false;
    if (!createImageViews()) return false;
    // Pipeline is unchanged (dynamic rendering + dynamic viewport; format
    // rarely changes). Command buffers are re-allocated.
    if (!createCommandPool()) return false;

    m_swapchainOutdated = false;
    std::cout << "Renderer: swapchain recreated ("
              << m_swapchainExtent.width << "x" << m_swapchainExtent.height
              << ")\n";
    return true;
}

// ---------------------------------------------------------------------------
// batch build + draw
// ---------------------------------------------------------------------------
void VulkanRenderer::buildBatch() {
    m_vertexCount = 0;
    m_indexCount = 0;
    if (m_rectangles.empty()) return;

    if (m_rectangles.size() > MAX_RECTANGLES) {
        std::cerr << "Renderer: too many rectangles (" << m_rectangles.size()
                  << " > " << MAX_RECTANGLES << "), truncating\n";
    }

    float* v = m_vertexMapped;
    uint32_t* ib = m_indexMapped;
    unsigned int base = 0;
    size_t drawn = 0;
    for (auto* r : m_rectangles) {
        if (drawn >= MAX_RECTANGLES) break;
        r->writeVertices(v);
        v += 24;  // 4 verts * 6 floats
        r->writeIndices(ib, base);
        ib += 6;
        base += 4;
        ++drawn;
    }
    m_vertexCount = drawn * 4;
    m_indexCount = drawn * 6;
}

void VulkanRenderer::drawBatch(uint32_t imageIndex) {
    if (m_indexCount == 0) return;  // nothing to draw

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

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
    colorAttach.clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};  // dark gray

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea = {{0, 0}, m_swapchainExtent};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &colorAttach;
    vkCmdBeginRendering(cmd, &rendering);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

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

    // Push constants: viewport (fb size) + scale (fb / window ratio) for
    // screen->NDC conversion and high-DPI correctness.
    float pc[4] = {
        static_cast<float>(m_fbWidth),
        static_cast<float>(m_fbHeight),
        (m_winWidth > 0) ? static_cast<float>(m_fbWidth) / m_winWidth : 1.0f,
        (m_winHeight > 0) ? static_cast<float>(m_fbHeight) / m_winHeight : 1.0f,
    };
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       16, pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(m_indexCount), 1, 0, 0, 0);

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

    // Recreate swapchain-dependent resources if the window was resized or the
    // swapchain went out of date.
    if (m_swapchainOutdated) {
        if (!recreateSwapchain()) return false;
    }

    // Wait for the previous frame to finish using this frame's resources.
    VkResult res = vkWaitForFences(m_device, 1,
                                   &m_inFlightFences[m_currentFrame], VK_TRUE,
                                   UINT64_MAX);
    if (!checkValidationResult(res, "vkWaitForFences")) return false;
    res = vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    if (!checkValidationResult(res, "vkResetFences")) return false;

    // Acquire the next swapchain image.
    uint32_t imageIndex = 0;
    res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                m_imageAvailableSemaphores[m_currentFrame],
                                VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchainOutdated = true;
        return true;  // will recreate on the next frame
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return checkValidationResult(res, "vkAcquireNextImageKHR");
    }

    // Rebuild the CPU-updated vertex/index batch and record the command buffer.
    buildBatch();
    drawBatch(imageIndex);

    // Submit to the graphics queue.
    VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffers[m_currentFrame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

    res = vkQueueSubmit(m_graphicsQueue, 1, &submit,
                        m_inFlightFences[m_currentFrame]);
    if (!checkValidationResult(res, "vkQueueSubmit")) return false;

    // Present.
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &imageIndex;

    res = vkQueuePresentKHR(m_presentQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        m_swapchainOutdated = true;
    } else if (!checkValidationResult(res, "vkQueuePresentKHR")) {
        return false;
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

// ---------------------------------------------------------------------------
// rectangle management (non-owning)
// ---------------------------------------------------------------------------
int VulkanRenderer::addRectangle(Rectangle* rect) {
    if (!rect) return -1;
    m_rectangles.push_back(rect);
    return static_cast<int>(m_rectangles.size()) - 1;
}

void VulkanRenderer::removeRectangle(Rectangle* rect) {
    m_rectangles.erase(
        std::remove(m_rectangles.begin(), m_rectangles.end(), rect),
        m_rectangles.end());
}
