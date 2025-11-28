#include "sdl3.hpp"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_vulkan.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>

#include <glm/glm.hpp>

using namespace std::chrono_literals;

extern "C" const unsigned char _binary_shader_frag_spv_start[];
extern "C" const unsigned char _binary_shader_frag_spv_end[];
extern "C" const unsigned char _binary_shader_vert_spv_start[];
extern "C" const unsigned char _binary_shader_vert_spv_end[];

struct Vertex {
    glm::vec2 pos;
    glm::vec3 col;

    static VkVertexInputBindingDescription binding_description() {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
    }

    static std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions() {
        return std::to_array<VkVertexInputAttributeDescription>({
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, pos),
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, col)
            }
        });
    }
};

constexpr auto VERTICES = std::to_array<Vertex>({
    {{ 0, -0.5 }, { 1, 0, 0 }},
    {{ 0.5, 0.5 }, { 0, 1, 0 }},
    {{ -0.5, 0.5 }, { 0, 0, 1 }}
});

int main(int argc, char** argv) {
    if (!sdl3_init()) {
        return 1;
    }

    SDL_Window* window;
    if ((window = SDL_CreateWindow("brampling3D", 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)) == nullptr) {
        sdl3_perror("Failed to create window");
        return 1;
    }

    // Load Vulkan APIs
    if (volkInitialize() != VK_SUCCESS) {
        spdlog::critical("Failed to initialize Vulkan!");
        return 1;
    }

    // Get instance extensions needed for vkCreateInstance
    u32 extensionCount;
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensions) {
        sdl3_perror("failed to get vulkan instance extensions");
        return 1;
    }

    // ApplicationInfo lets drivers enable application-specific optimizations. So Intel, NVIDIA, and AMD can implement the best optimizations for brampling3D.
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "brampling3D",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "brampling3D",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    // Create a vulkan instance with the required extensions
    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions
    };

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        spdlog::error("Failed to create vulkan instance");
        return 1;
    }

    volkLoadInstance(instance);

    spdlog::info("vulkan instance created");

    // Create the window surface. This is our target for presenting window contents.
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        sdl3_perror("Failed to create vulkan surface");
        return 1;
    }

    // Find a suitable physical device
    u32 deviceCount;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        spdlog::error("Failed to enumerate vulkan devices");
        return 1;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    u32 graphicsFamily = UINT32_MAX;
    u32 presentFamily = UINT32_MAX;

    for (const auto& device : devices) {
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        bool graphicsFound = false;
        bool presentFound = false;
        for (u32 i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamily = i;
                graphicsFound = true;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                presentFamily = i;
                presentFound = true;
            }
        }

        if (graphicsFound && presentFound) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        spdlog::critical("Failed to find suitable vulkan device");
        return 1;
    }

    spdlog::info("presentFamily: {}, graphicsFamily: {}", presentFamily, graphicsFamily);

    // Create a logical device.
    float queuePriority = 1;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphicsFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    });

    if (graphicsFamily != presentFamily) {
        queueCreateInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = presentFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = (u32) queueCreateInfos.size(),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures,
    };

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        spdlog::critical("Failed to create vulkan device");
        return 1;
    }

    volkLoadDevice(device);

    VkQueue graphicsQueue, presentQueue;
    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);

    // Create swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data());
    
    VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
    for (const auto& format : surfaceFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    VkExtent2D swapchainExtent = capabilities.currentExtent;
    if (swapchainExtent.width == UINT32_MAX) {
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        swapchainExtent.width = (u32) width;
        swapchainExtent.height = (u32) height;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = capabilities.minImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE
    };
    
    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS) {
        spdlog::critical("Failed to create swapchain");
        return 1;
    }

    // Create views for each swapchain image
    u32 imageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    std::vector<VkImage> swapchainImages(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    std::vector<VkImageView> swapchainImageViews(imageCount);
    for (usize i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create image view {}", i);
            return 1;
        }
    }

    // Create a VkRenderPass, which (seemingly?) represents the bound render targets / attachments
    VkAttachmentDescription colorAttachment{
        .format = surfaceFormat.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        spdlog::critical("failed to create render pass");
        return 1;
    }

    // Compile shaders and create a graphics pipeline.

    std::vector<u8> vert_shader(_binary_shader_vert_spv_start, _binary_shader_vert_spv_end);
    std::vector<u8> frag_shader(_binary_shader_frag_spv_start, _binary_shader_frag_spv_end);

    VkShaderModuleCreateInfo vertModuleInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_shader.size(),
        .pCode = reinterpret_cast<const u32*>(vert_shader.data())
    };

    VkShaderModule vertShaderModule;
    if (vkCreateShaderModule(device, &vertModuleInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
        spdlog::critical("failed to create vertex shader");
        return 1;
    }

    VkShaderModuleCreateInfo fragModuleInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_shader.size(),
        .pCode = reinterpret_cast<const u32*>(frag_shader.data())
    };

    VkShaderModule fragShaderModule;
    if (vkCreateShaderModule(device, &fragModuleInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
        spdlog::critical("failed to create fragment shader");
        return 1;
    }

    VkPipelineShaderStageCreateInfo vertStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo fragStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertStageInfo,
        fragStageInfo
    };

    auto bindingDesc = Vertex::binding_description();
    auto attrDescs = Vertex::attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount = (u32) attrDescs.size(),
        .pVertexAttributeDescriptions = attrDescs.data()
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = (float) swapchainExtent.width,
        .height = (float) swapchainExtent.height,
        .minDepth = 0,
        .maxDepth = 0
    };

    VkRect2D scissor{
        .offset = { 0, 0 },
        .extent = swapchainExtent
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1,
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                          VK_COLOR_COMPONENT_G_BIT | 
                          VK_COLOR_COMPONENT_B_BIT | 
                          VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        spdlog::critical("failed to create pipeline layout");
        return 1;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0
    };

    VkPipeline graphicsPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        spdlog::critical("failed to create graphics pipeline");
        return 1;
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    // Create frame buffers attached to each swapchain image
    std::vector<VkFramebuffer> swapchainFramebuffers(imageCount);
    for (usize i = 0; i < imageCount; i++) {
        VkImageView attachments[] = {swapchainImageViews[i]};
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create framebuffer {}", i);
            return 1;
        }
    }

    // Create a grapics command pool.
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsFamily,
    };

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        spdlog::critical("failed to create command pool");
        return 1;
    }

    // Create the vertex buffer.
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * VERTICES.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        spdlog::critical("failed to create vertex buffer");
        return 1;
    }

    // Allocate memory and bind it to the buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    // ?????
    u32 memoryTypeIndex = UINT32_MAX;
    for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (i << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == UINT32_MAX) {
        spdlog::critical("failed to find suitable memory type for vertex buffer");
        return 1;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        spdlog::critical("failed to allocate vertex buffer memory");
        return 1;
    }

    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

    // Upload vertex data.
    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);
    memcpy(data, VERTICES.data(), static_cast<usize>(bufferInfo.size));
    vkUnmapMemory(device, vertexBufferMemory);

    // Allocate a command buffer for each swapchain image.
    std::vector<VkCommandBuffer> commandBuffers(imageCount);
    VkCommandBufferAllocateInfo allocInfoCmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = imageCount
    };

    if (vkAllocateCommandBuffers(device, &allocInfoCmd, commandBuffers.data()) != VK_SUCCESS) {
        spdlog::critical("failed to allocate command buffers");
        return 1;
    }

    spdlog::info("recording {} command buffers", imageCount);
    for (usize i = 0; i < imageCount; i++) {
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            spdlog::critical("failed to begin recording command buffer {}", i);
            return 1;
        }

        VkClearValue clearColor = {{{0.2, 0.2, 0.2, 1}}};

        // Begin the render pass by clearing the image.
        VkRenderPassBeginInfo rpBeginInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = swapchainFramebuffers[i],
            .renderArea = {
                .offset = { 0, 0 },
                .extent = swapchainExtent
            },
            .clearValueCount = 1,
            .pClearValues = &clearColor
        };

        // Then draw.
        vkCmdBeginRenderPass(commandBuffers[i], &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
        vkCmdDraw(commandBuffers[i], (u32) VERTICES.size(), 1, 0, 0);
        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            spdlog::critical("failed to end command buffer {}", i);
            return 1;
        }
    }

    std::vector<VkSemaphore> imageAvailableSemaphores(imageCount);
    std::vector<VkSemaphore> renderFinishedSemaphores(imageCount);
    std::vector<VkFence> inFlightFences(imageCount);
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (usize i = 0; i < imageCount; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create image available semaphore");
            return 1;
        }

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create render finished semaphore");
            return 1;
        }

        if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create in flight fence");
            return 1;
        }
    }

    // Finally Vulkan initialization is done.
    // Present our SDL window and start the loop.

    // SDL_SetWindowResizable(window, true);
    SDL_SetWindowMinimumSize(window, 640, 480);
    SDL_ShowWindow(window);

    spdlog::info("Showing window");

    u32 currentFrame = 0;

    bool quit = false;

    while (!quit) {
        // Poll window events before rendering. (why is this not bound to the window?)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_Q) {
                    quit = true;
                }
                break;
            default:
                break;
            }
        }

        VkSemaphore imageAvailableSemaphore = imageAvailableSemaphores[currentFrame];
        VkFence inFlightFence = inFlightFences[currentFrame];

        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFence);

        u32 imageIndex;
        VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            spdlog::error("failed to acquire next image: {}", (int)acquireResult);
            quit = true;
            continue;
        }

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};

        // Submit the command buffer that we already recorded.
        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[imageIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores
        };

        VkResult submitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
        if (submitResult != VK_SUCCESS) {
            spdlog::error("failed to submit draw command buffer for {}: {}", imageIndex, (int)submitResult);
            quit = true;
            continue;
        }

        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex
        };

        VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
            spdlog::error("failed to present image {}: {}", imageIndex, (int)presentResult);
            quit = true;
            continue;
        }

        currentFrame = (currentFrame + 1) % imageCount;

        // 60 fps limit so as to not nuke the CPU (TODO: vsync?)
        std::this_thread::sleep_for(16ms);
    }

    spdlog::info("quitting");

    return 0;
}
