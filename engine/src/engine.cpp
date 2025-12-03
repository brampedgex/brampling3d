#include "engine.hpp"

extern "C" const unsigned char _binary_shader_fragment_spv_start[];
extern "C" const unsigned char _binary_shader_fragment_spv_end[];
extern "C" const unsigned char _binary_shader_vertex_spv_start[];
extern "C" const unsigned char _binary_shader_vertex_spv_end[];

struct Vertex {
    glm::vec3 pos;
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
                .format = VK_FORMAT_R32G32B32_SFLOAT,
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
    {{ -0.5, 0.5, -0.5 }, { 0, 1, 0 }},
    {{ -0.5, 0.5, 0.5 }, { 0, 1, 1 }},
    {{ -0.5, -0.5, -0.5 }, { 0, 0, 0 }},
    {{ -0.5, -0.5, 0.5 }, { 0, 0, 1 }},
    {{ 0.5, 0.5, -0.5 }, { 1, 1, 0 }},
    {{ 0.5, 0.5, 0.5 }, { 1, 1, 1 }},
    {{ 0.5, -0.5, -0.5 }, { 1, 0, 0 }},
    {{ 0.5, -0.5, 0.5 }, { 1, 0, 1 }},
});

constexpr auto INDICES = std::to_array<u16>({
    0, 1, 2, 1, 3, 2,
    1, 5, 3, 5, 7, 3,
    5, 4, 7, 4, 6, 7,
    4, 0, 6, 0, 2, 6,
    4, 5, 0, 5, 1, 0,
    6, 2, 7, 7, 2, 3
});

struct UniformBufferObject {
    glm::mat4x4 model;
    glm::mat4x4 view;
    glm::mat4x4 proj;
};

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
    // Destroy Vulkan resources before destroying the window, which will rug-pull our resources and make the destructors explode instead.
    vkDeviceWaitIdle(m_device);
    m_swapchain.reset();

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

    // create_render_pass() depends on the surface format, which is chosen by VulkanSwapchain, before creating the actual swapchain.
    m_swapchain = std::make_unique<VulkanSwapchain>(m_physical_device, m_device, m_window_surface);

    if (!create_render_pass())
        return false;

    if (!create_swapchain())
        return false;

    if (!create_descriptor_set_layout())
        return false;

    if (!create_graphics_pipeline())
        return false;

    if (!create_uniform_buffers())
        return false;

    if (!create_descriptor_pool())
        return false;

    if (!create_descriptor_sets())
        return false;

    if (!create_vertex_buffer())
        return false;

    if (!create_index_buffer())
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

bool Engine::create_render_pass() {
    VkAttachmentDescription color_attachment{
        .format = m_swapchain->surface_format().format,
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

bool Engine::create_swapchain() {
    int width, height;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    return m_swapchain->create((u32) width, (u32) height, m_render_pass);
}

bool Engine::create_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr
    }; 

    VkDescriptorSetLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &binding
    };

    if (vkCreateDescriptorSetLayout(m_device, &layout_create_info, nullptr, &m_descriptor_set_layout) != VK_SUCCESS) {
        spdlog::error("failed to create descriptor set layout");
        return false;
    }

    return true;
}

bool Engine::create_graphics_pipeline() {
    std::vector<u8> vert_shader(_binary_shader_vertex_spv_start, _binary_shader_vertex_spv_end);
    std::vector<u8> frag_shader(_binary_shader_fragment_spv_start, _binary_shader_fragment_spv_end);

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
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptor_set_layout
    };

    if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
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
        .layout = m_pipeline_layout,
        .renderPass = m_render_pass,
        .subpass = 0
    };

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS) {
        spdlog::error("failed to create graphics pipeline");
        return false;
    }

    vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
    vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
    return true;
}

bool Engine::create_uniform_buffers() {
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(UniformBufferObject),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        if (vkCreateBuffer(m_device, &buffer_info, nullptr, &m_uniform_buffers[i]) != VK_SUCCESS) {
            spdlog::error("failed to create uniform buffer {}", i);
            return false;
        }

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(m_device, m_uniform_buffers[i], &mem_requirements);

        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);

        u32 memory_type_index = UINT32_MAX;
        for (u32 j = 0; j < mem_properties.memoryTypeCount; j++) {
            if ((mem_requirements.memoryTypeBits & (j << j)) &&
                (mem_properties.memoryTypes[j].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                memory_type_index = j;
                break;
            }
        }

        if (memory_type_index == UINT32_MAX) {
            spdlog::error("failed to find suitable memory type for uniform buffer");
            return false;
        }

        VkMemoryAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = mem_requirements.size,
            .memoryTypeIndex = memory_type_index,
        };

        if (vkAllocateMemory(m_device, &alloc_info, nullptr, &m_uniform_buffer_memory[i]) != VK_SUCCESS) {
            spdlog::error("failed to allocate vertex buffer memory");
            return false;
        }

        vkBindBufferMemory(m_device, m_uniform_buffers[i], m_uniform_buffer_memory[i], 0);
    }

    return true;
}

bool Engine::create_descriptor_pool() {
    VkDescriptorPoolSize pool_size{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };

    VkResult res = vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool);
    if (res != VK_SUCCESS) {
        spdlog::error("failed to create descriptor pool: {}", vulkan_error_str(res));
        return false;
    }

    return true;
}

bool Engine::create_descriptor_sets() {
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    std::ranges::fill(layouts, m_descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptor_pool,
        .descriptorSetCount = (u32) layouts.size(),
        .pSetLayouts = layouts.data()
    };

    VkResult res = vkAllocateDescriptorSets(m_device, &alloc_info, m_descriptor_sets.data());
    if (res != VK_SUCCESS) {
        spdlog::error("failed to allocate descriptor sets: {}", vulkan_error_str(res));
        return false;
    }

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = m_uniform_buffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject),
        };
        
        VkWriteDescriptorSet descriptor_write{
            .dstSet = m_descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info
        };

        vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
    }
    
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
        return false;
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
        return false;
    }

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index
    };

    if (vkAllocateMemory(m_device, &alloc_info, nullptr, &m_vertex_buffer_memory) != VK_SUCCESS) {
        spdlog::error("failed to allocate vertex buffer memory");
        return false;
    }

    vkBindBufferMemory(m_device, m_vertex_buffer, m_vertex_buffer_memory, 0);

    // Upload vertex data.
    void* data;
    vkMapMemory(m_device, m_vertex_buffer_memory, 0, buffer_info.size, 0, &data);
    memcpy(data, VERTICES.data(), static_cast<usize>(buffer_info.size));
    vkUnmapMemory(m_device, m_vertex_buffer_memory);

    return true;
}

bool Engine::create_index_buffer() {
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(u16) * INDICES.size(),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    if (vkCreateBuffer(m_device, &buffer_info, nullptr, &m_index_buffer) != VK_SUCCESS) {
        spdlog::error("failed to create index buffer");
        return false;
    }

    // Allocate memory and bind it to the buffer
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(m_device, m_index_buffer, &mem_requirements);

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
        spdlog::error("failed to find suitable memory type for index buffer");
        return false;
    }

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index
    };

    if (vkAllocateMemory(m_device, &alloc_info, nullptr, &m_index_buffer_memory) != VK_SUCCESS) {
        spdlog::error("failed to allocate index buffer memory");
        return false;
    }

    vkBindBufferMemory(m_device, m_index_buffer, m_index_buffer_memory, 0);

    // Upload index data.
    void* data;
    vkMapMemory(m_device, m_index_buffer_memory, 0, buffer_info.size, 0, &data);
    memcpy(data, INDICES.data(), static_cast<usize>(buffer_info.size));
    vkUnmapMemory(m_device, m_index_buffer_memory);

    return true;
}

bool Engine::create_command_buffers() {
    // Allocate a command buffer for each swapchain image.
    VkCommandBufferAllocateInfo alloc_info_cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };

    if (vkAllocateCommandBuffers(m_device, &alloc_info_cmd, m_command_buffers.data()) != VK_SUCCESS) {
        spdlog::error("failed to allocate command buffers");
        return false;
    }

    return true;
}

bool Engine::create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS) {
            spdlog::error("failed to create image available semaphore");
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

    m_swapchain->reset();
    // TODO: recreate render pass if the surface format changed
    // TODO: handle errors. just nuke everything if resize fails ig
    create_swapchain();

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
    VkResult acquire_result = m_swapchain->acquire(image_available_semaphore, image_index);

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_need_swapchain_recreate = true;
        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
        return;
    } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
        // Suboptimal means we can render this frame, but we should still recreate the swapchain after.
        m_need_swapchain_recreate = true;
    } else if (acquire_result != VK_SUCCESS) {
        vulkan_check_res(acquire_result, "failed to acquire swapchain image");
    }

    VkCommandBuffer command_buffer = m_command_buffers[m_current_frame];

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };

    vulkan_check_res(
        vkBeginCommandBuffer(command_buffer, &begin_info),
        "failed to begin recording command buffer"
    );

    {
        auto now = std::chrono::steady_clock::now();
        static auto start = now;

        f32 seconds = std::chrono::duration_cast<std::chrono::duration<float>>(now - start).count();

        auto extent = m_swapchain->extent();
        auto aspect = (f32) extent.width / extent.height;

        UniformBufferObject ubo;
        ubo.model = glm::rotate(glm::identity<glm::mat4x4>(), glm::radians(seconds * 180), glm::vec3(0, 1, 0));
        ubo.view = glm::lookAt(glm::vec3(3, 3, 3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        ubo.proj = glm::perspective(glm::radians(90.f), aspect, 0.1f, 100.f);

        auto mem = m_uniform_buffer_memory[m_current_frame];

        void* data;
        vkMapMemory(m_device, mem, 0, sizeof(UniformBufferObject), 0, &data);
        memcpy(data, &ubo, sizeof(UniformBufferObject));
        vkUnmapMemory(m_device, mem);
    }

    VkClearValue clear_col = {{{0.2, 0.2, 0.2, 1}}};

    // Begin the render pass by clearing the image.
    VkRenderPassBeginInfo rp_begin_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_render_pass,
        .framebuffer = m_swapchain->framebuffer(image_index),
        .renderArea = {
            .offset = { 0, 0 },
            .extent = m_swapchain->extent()
        },
        .clearValueCount = 1,
        .pClearValues = &clear_col
    };

    // Set viewport and scissor, which are dynamic.
    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = (f32) m_swapchain->extent().width,
        .height = (f32) m_swapchain->extent().height,
        .minDepth = 0,
        .maxDepth = 0
    };

    VkRect2D scissor{
        .offset = { 0, 0 },
        .extent = m_swapchain->extent()
    };

    // Then draw.
    vkCmdBeginRenderPass(command_buffer, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    VkDescriptorSet descriptor_sets[] = { m_descriptor_sets[m_current_frame] };
    VkBuffer vertex_bufs[] = { m_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1, descriptor_sets, 0, nullptr);
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_bufs, offsets);
    vkCmdBindIndexBuffer(command_buffer, m_index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer, (u32) INDICES.size(), 1, 0, 0, 0);
    vkCmdEndRenderPass(command_buffer);

    vulkan_check_res(
        vkEndCommandBuffer(command_buffer),
        "failed to end command buffer"
    );

    VkSemaphore wait_semaphores[] = { image_available_semaphore };
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = { m_swapchain->submit_semaphore(image_index) };

    // Submit the command buffer.
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores
    };

    VkResult submit_result = vkQueueSubmit(m_graphics_queue, 1, &submit_info, in_flight_fence);
    vulkan_check_res(submit_result, "failed to submit draw command buffer for {}", image_index);

    VkResult present_result = m_swapchain->present(m_present_queue, signal_semaphores[0], image_index);

    if (present_result == VK_SUBOPTIMAL_KHR || present_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Recreate swapchain next frame. We usually get this right before SDL sends a resize event anyway
        m_need_swapchain_recreate = true;
    } else if (present_result != VK_SUCCESS) {
        vulkan_check_res(present_result, "failed to present image {}", image_index);
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}
