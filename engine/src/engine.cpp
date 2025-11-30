#include "engine.hpp"

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

bool Engine::start() {
    if (!sdl3_init())
        return false;

    if (!init_window())
        return false;

    if (!init_graphics())
        return false;

    SDL_ShowWindow(m_window);

    spdlog::info("startup complete");
    return true;
}

void Engine::run() {
    bool should_quit = false;

    while (!should_quit) {
        // Poll window events before rendering. (why is this not bound to the window?)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                should_quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_Q) {
                    should_quit = true;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                auto& window_event = event.window;
                spdlog::info("window resized: {} {}", window_event.data1, window_event.data2);

                m_window_resized = true;
            } break;
            default:
                break;
            }
        }

        update_graphics();
    }

    spdlog::info("quitting");
    quit();
}

bool Engine::init_window() {
    if ((m_window = SDL_CreateWindow("brampling3D", 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)) == nullptr) {
        sdl3_perror("Failed to create window");
        return false;
    }

    SDL_SetWindowResizable(m_window, true);
    SDL_SetWindowMinimumSize(m_window, 640, 480);

    spdlog::info("Window initialized");
    return true;
}

void Engine::quit() {
    SDL_Quit();
}

bool Engine::init_graphics() {
    if (!create_vk_instance())
        return false;
    if (!create_window_surface())
        return false;
    if (!find_physical_device())
        return false;
    if (!create_device())
        return false;
    if (!create_swapchain())
        return false;
    if (!create_render_pass())
        return false;
    if (!create_framebuffers())
        return false;
    if (!create_graphics_pipeline())
        return false;
    if (!create_vertex_buffer())
        return false;
    if (!create_command_buffers())
        return false;
    if (!create_sync_objects())
        return false;

    spdlog::info("Vulkan initialized");
    return true;
}

bool Engine::create_vk_instance() {
    // Load Vulkan entry points
    if (volkInitialize() != VK_SUCCESS) {
        spdlog::error("Failed to load Vulkan");
        return false;
    }

    // Get instance extensions needed for vkCreateInstance
    u32 extension_count;
    auto* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (!extensions) {
        sdl3_perror("Failed to get vulkan instance extensions");
        return false;
    }

    // ApplicationInfo lets drivers enable application-specific optimizations.
    // So Intel, NVIDIA, and AMD can implement the best optimizations for brampling3D
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "brampling3D",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "brampling3D",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    // Create a vulkan instance with the required extensions
    VkInstanceCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions
    };

    if (vkCreateInstance(&create_info, nullptr, &m_vk_instance) != VK_SUCCESS) {
        spdlog::error("Failed to create vulkan instance");
        return false;
    }

    volkLoadInstance(m_vk_instance);

    spdlog::info("vulkan instance created");

    return true;
}

bool Engine::create_window_surface() {
    if (!SDL_Vulkan_CreateSurface(m_window, m_vk_instance, nullptr, &m_window_surface)) {
        sdl3_perror("Failed to create vulkan surface");
        return false;
    }

    return true;
}

bool Engine::find_physical_device() {
    u32 device_count;
    vkEnumeratePhysicalDevices(m_vk_instance, &device_count, nullptr);
    if (device_count == 0) {
        spdlog::error("Failed to enumerate vulkan devices");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_vk_instance, &device_count, devices.data());

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    u32 graphics_family = UINT32_MAX;
    u32 present_family = UINT32_MAX;

    for (const auto device : devices) {
        u32 queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        bool graphics_found = false;
        bool present_found = false;
        for (u32 i = 0; i < queue_family_count; i++) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family = i;
                graphics_found = true;
            }

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_window_surface, &present_support);
            if (present_support) {
                present_family = i;
                present_found = true;
            }
        }

        if (graphics_found && present_found) {
            physical_device = device;
            break;
        }
    }

    if (physical_device == VK_NULL_HANDLE) {
        spdlog::error("Failed to find a suitable vulkan device");
        return false;
    }

    m_physical_device = physical_device;
    m_graphics_family = graphics_family;
    m_present_family = present_family;
    return true;
}

bool Engine::create_device() {
    f32 queue_priority = 1;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    });

    if (m_graphics_family != m_present_family) {
        queue_create_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = m_present_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        });
    }

    VkPhysicalDeviceFeatures device_features{};

    const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = (u32) queue_create_infos.size(),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
        .pEnabledFeatures = &device_features,
    };

    if (vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device) != VK_SUCCESS) {
        spdlog::error("Failed to create vulkan device");
        return false;
    }

    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, m_graphics_family, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, m_present_family, 0, &m_present_queue);

    // Create a grapics command pool.
    VkCommandPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_graphics_family,
    };

    if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
        spdlog::critical("failed to create command pool");
        return 1;
    }

    return true;
}

bool Engine::create_swapchain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_window_surface, &capabilities);

    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_window_surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_window_surface, &format_count, surface_formats.data());
    
    // Choose a surface format.
    VkSurfaceFormatKHR surface_format = surface_formats[0];
    for (const auto& format : surface_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = format;
            break;
        }
    }
    m_surface_format = surface_format;

    VkExtent2D swapchain_extent = capabilities.currentExtent;
    if (swapchain_extent.width == UINT32_MAX) {
        int width, height;
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
        swapchain_extent.width = (u32) width;
        swapchain_extent.height = (u32) height;
    }
    m_swapchain_extent = swapchain_extent;

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_window_surface,
        .minImageCount = capabilities.minImageCount,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE
    };
    
    if (vkCreateSwapchainKHR(m_device, &swapchain_create_info, nullptr, &m_swapchain) != VK_SUCCESS) {
        spdlog::error("Failed to create swapchain");
        return false;
    }

    // Create views for each swapchain image
    u32 image_count;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
    m_swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());

    m_swapchain_image_views.resize(image_count);

    for (usize i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
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

        if (vkCreateImageView(m_device, &view_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS) {
            spdlog::error("failed to create image view {}", i);
            return false;
        }
    }

    return true;
}

bool Engine::create_render_pass() {
    VkAttachmentDescription color_attachment{
        .format = m_surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference color_attachment_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref
    };

    VkRenderPassCreateInfo render_pass_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    if (vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass) != VK_SUCCESS) {
        spdlog::error("failed to create render pass");
        return false;
    }

    return true;
}

bool Engine::create_framebuffers() {
    // Create framebuffers for the render pass for each swapchain image.
    for (const auto [i, image_view] : m_swapchain_image_views | std::views::enumerate) {
        VkImageView attachments[] = { image_view };
        VkFramebufferCreateInfo framebuffer_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = m_render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = m_swapchain_extent.width,
            .height = m_swapchain_extent.height,
            .layers = 1
        };

        VkFramebuffer framebuffer;
        if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &framebuffer) != VK_SUCCESS) {
            spdlog::error("failed to create framebuffer {}", i);
            return false;
        }

        m_swapchain_framebuffers.push_back(framebuffer);
    }

    return true;
}

bool Engine::create_graphics_pipeline() {
    std::vector<u8> vert_shader(_binary_shader_vert_spv_start, _binary_shader_vert_spv_end);
    std::vector<u8> frag_shader(_binary_shader_frag_spv_start, _binary_shader_frag_spv_end);

    VkShaderModuleCreateInfo vert_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_shader.size(),
        .pCode = reinterpret_cast<const u32*>(vert_shader.data())
    };

    VkShaderModule vert_shader_module;
    if (vkCreateShaderModule(m_device, &vert_module_info, nullptr, &vert_shader_module) != VK_SUCCESS) {
        spdlog::error("failed to create vertex shader");
        return false;
    }

    VkShaderModuleCreateInfo frag_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_shader.size(),
        .pCode = reinterpret_cast<const u32*>(frag_shader.data())
    };

    VkShaderModule frag_shader_module;
    if (vkCreateShaderModule(m_device, &frag_module_info, nullptr, &frag_shader_module) != VK_SUCCESS) {
        spdlog::error("failed to create fragment shader");
        return false;
    }

    VkPipelineShaderStageCreateInfo vert_stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader_module,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo frag_stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader_module,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_stage_info,
        frag_stage_info
    };

    auto binding_desc = Vertex::binding_description();
    auto attr_descs = Vertex::attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = (u32) attr_descs.size(),
        .pVertexAttributeDescriptions = attr_descs.data()
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = nullptr,  // Viewport is dynamic
        .scissorCount = 1,
        .pScissors = nullptr,   // Scissor is dynamic
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

    VkPipelineColorBlendAttachmentState color_blend_attachments{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                          VK_COLOR_COMPONENT_G_BIT | 
                          VK_COLOR_COMPONENT_B_BIT | 
                          VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachments
    };

    VkDynamicState dynamic_state[] = { 
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_state
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    VkPipelineLayout pipeline_layout;
    if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        spdlog::error("failed to create pipeline layout");
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state_info,
        .layout = pipeline_layout,
        .renderPass = m_render_pass,
        .subpass = 0
    };

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline) != VK_SUCCESS) {
        spdlog::error("failed to create graphics pipeline");
        return false;
    }

    vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
    vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
    return true;
}

bool Engine::create_vertex_buffer() {
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * VERTICES.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    if (vkCreateBuffer(m_device, &buffer_info, nullptr, &m_vertex_buffer) != VK_SUCCESS) {
        spdlog::error("failed to create vertex buffer");
        return true;
    }

    // Allocate memory and bind it to the buffer
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(m_device, m_vertex_buffer, &mem_requirements);

    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);

    // ?????
    u32 memory_type_index = UINT32_MAX;
    for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((mem_requirements.memoryTypeBits & (i << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            memory_type_index = i;
            break;
        }
    }
    if (memory_type_index == UINT32_MAX) {
        spdlog::error("failed to find suitable memory type for vertex buffer");
        return true;
    }

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index
    };

    if (vkAllocateMemory(m_device, &alloc_info, nullptr, &m_vertex_buffer_memory) != VK_SUCCESS) {
        spdlog::error("failed to allocate vertex buffer memory");
        return true;
    }

    vkBindBufferMemory(m_device, m_vertex_buffer, m_vertex_buffer_memory, 0);

    // Upload vertex data.
    void* data;
    vkMapMemory(m_device, m_vertex_buffer_memory, 0, buffer_info.size, 0, &data);
    memcpy(data, VERTICES.data(), static_cast<usize>(buffer_info.size));
    vkUnmapMemory(m_device, m_vertex_buffer_memory);

    return true;
}

bool Engine::create_command_buffers() {
    // Allocate a command buffer for each swapchain image.
    m_command_buffers.resize(m_swapchain_images.size());
    
    VkCommandBufferAllocateInfo alloc_info_cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = (u32) m_swapchain_images.size()
    };

    if (vkAllocateCommandBuffers(m_device, &alloc_info_cmd, m_command_buffers.data()) != VK_SUCCESS) {
        spdlog::error("failed to allocate command buffers");
        return false;
    }

    for (usize i = 0; i < m_swapchain_images.size(); i++) {
        VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };

        if (vkBeginCommandBuffer(m_command_buffers[i], &begin_info) != VK_SUCCESS) {
            spdlog::error("failed to begin recording command buffer {}", i);
            return false;
        }

        VkClearValue clear_col = {{{0.2, 0.2, 0.2, 1}}};

        // Begin the render pass by clearing the image.
        VkRenderPassBeginInfo rp_begin_info{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_render_pass,
            .framebuffer = m_swapchain_framebuffers[i],
            .renderArea = {
                .offset = { 0, 0 },
                .extent = m_swapchain_extent
            },
            .clearValueCount = 1,
            .pClearValues = &clear_col
        };

        // Set viewport and scissor, which are dynamic.
        VkViewport viewport{
            .x = 0,
            .y = 0,
            .width = (f32) m_swapchain_extent.width,
            .height = (f32) m_swapchain_extent.height,
            .minDepth = 0,
            .maxDepth = 0
        };

        VkRect2D scissor{
            .offset = { 0, 0 },
            .extent = m_swapchain_extent
        };

        // Then draw.
        vkCmdBeginRenderPass(m_command_buffers[i], &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
        vkCmdSetViewport(m_command_buffers[i], 0, 1, &viewport);
        vkCmdSetScissor(m_command_buffers[i], 0, 1, &scissor);
        VkBuffer vertex_bufs[] = { m_vertex_buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_command_buffers[i], 0, 1, vertex_bufs, offsets);
        vkCmdDraw(m_command_buffers[i], (u32) VERTICES.size(), 1, 0, 0);
        vkCmdEndRenderPass(m_command_buffers[i]);

        if (vkEndCommandBuffer(m_command_buffers[i]) != VK_SUCCESS) {
            spdlog::error("failed to end command buffer {}", i);
            return false;
        }
    }

    return true;
}

bool Engine::create_sync_objects() {
    usize image_count = m_swapchain_images.size();
    m_image_available_semaphores.resize(image_count);
    m_render_finished_semaphores.resize(image_count);
    m_in_flight_fences.resize(image_count);

    VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fence_info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

    for (usize i = 0; i < image_count; i++) {
        if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS) {
            spdlog::error("failed to create image available semaphore");
            return false;
        }

        if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS) {
            spdlog::error("failed to create render finished semaphore");
            return false;
        }

        if (vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS) {
            spdlog::error("failed to create in flight fence");
            return false;
        }
    }

    return true;
}

void Engine::recreate_swapchain() {
    // Wait for the device to be idle before recreating the swapchain
    vkDeviceWaitIdle(m_device);

    // Cleanup...
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    for (const auto view : m_swapchain_image_views) {
        vkDestroyImageView(m_device, view, nullptr);
    }
    for (const auto framebuffer : m_swapchain_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    vkFreeCommandBuffers(m_device, m_command_pool, (u32) m_command_buffers.size(), m_command_buffers.data()); 
    // TODO: Do we need to destroy sync objects?
    for (const auto semaphore : m_image_available_semaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }
    for (const auto semaphore : m_render_finished_semaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }
    for (const auto fence : m_in_flight_fences) {
        vkDestroyFence(m_device, fence, nullptr);
    }

    m_swapchain_images.clear();
    m_swapchain_image_views.clear();
    m_swapchain_framebuffers.clear();
    m_command_buffers.clear();
    m_image_available_semaphores.clear();
    m_render_finished_semaphores.clear();
    m_in_flight_fences.clear();

    // Now recreate everything.
    
    // TODO: handle errors somehow. just nuke everything if resize fails ig
    create_swapchain();
    create_framebuffers();
    create_command_buffers();
    create_sync_objects();
    
    // This fixes issues with synchronization. I have no idea why. Thank you random stackoverflow commenter!
    m_current_frame = 0;

    spdlog::info("finished swapchain recreate");
}

void Engine::update_graphics() {
    // Handle swapchain recreation before rendering a frame.
    if (m_window_resized || m_need_swapchain_recreate) {
        m_window_resized = m_need_swapchain_recreate = false;

        recreate_swapchain();
    }

    render_frame();
}

void Engine::render_frame() {
    VkSemaphore image_available_semaphore = m_image_available_semaphores[m_current_frame];
    VkFence in_flight_fence = m_in_flight_fences[m_current_frame];

    vkWaitForFences(m_device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &in_flight_fence);

    u32 image_index;
    VkResult acquire_result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_need_swapchain_recreate = true;
        m_current_frame = (m_current_frame + 1) % m_swapchain_images.size();
        return;
    } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
        // Suboptimal means we can render this frame, but we should still recreate the swapchain after.
        m_need_swapchain_recreate = true;
    } else if (acquire_result != VK_SUCCESS) {
        spdlog::error("failed to acquire next image: {}", string_VkResult(acquire_result));
        return;
    }

    VkSemaphore wait_semaphores[] = {image_available_semaphore};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {m_render_finished_semaphores[image_index]};

    // Submit the command buffer that we already recorded.
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_command_buffers[m_current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores
    };

    VkResult submit_result = vkQueueSubmit(m_graphics_queue, 1, &submit_info, in_flight_fence);
    if (submit_result != VK_SUCCESS) {
        spdlog::error("failed to submit draw command buffer for {}: {}", image_index, (int)submit_result);
        return;
    }

    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index
    };

    VkResult present_result = vkQueuePresentKHR(m_present_queue, &present_info);

    if (present_result == VK_SUBOPTIMAL_KHR || present_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Recreate swapchain next frame. We usually get this right before SDL sends a resize event anyway
        m_need_swapchain_recreate = true;
    } else if (present_result != VK_SUCCESS) {
        spdlog::error("failed to present image {}: {}", image_index, (int)present_result);
        return;
    }

    m_current_frame = (m_current_frame + 1) % m_swapchain_images.size();
}
