#include "engine.hpp"

#include "assets.hpp"

#include "util/imgui.hpp"
#include "util/stb.hpp"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 col;
    glm::vec2 tex_coord;

    static VkVertexInputBindingDescription binding_description() {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
    }

    static std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions() {
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
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, tex_coord)
            }
        });
    }
};

// Textured cube vertices
constexpr auto VERTICES = std::to_array<Vertex>({
    // -x
    {{ -0.5,  0.5, -0.5 }, { 0, 1, 0 }, { 0, 0 }},
    {{ -0.5,  0.5,  0.5 }, { 0, 1, 1 }, { 1, 0 }},
    {{ -0.5, -0.5, -0.5 }, { 0, 0, 0 }, { 0, 1 }},
    {{ -0.5, -0.5,  0.5 }, { 0, 0, 1 }, { 1, 1 }},
    // +z
    {{ -0.5,  0.5,  0.5 }, { 0, 1, 1 }, { 0, 0 }},
    {{  0.5,  0.5,  0.5 }, { 1, 1, 1 }, { 1, 0 }},
    {{ -0.5, -0.5,  0.5 }, { 0, 0, 1 }, { 0, 1 }},
    {{  0.5, -0.5,  0.5 }, { 1, 0, 1 }, { 1, 1 }},
    // +x
    {{  0.5,  0.5,  0.5 }, { 1, 1, 1 }, { 0, 0 }},
    {{  0.5,  0.5, -0.5 }, { 1, 1, 0 }, { 1, 0 }},
    {{  0.5, -0.5,  0.5 }, { 1, 0, 1 }, { 0, 1 }},
    {{  0.5, -0.5, -0.5 }, { 1, 0, 0 }, { 1, 1 }},
    // -z
    {{  0.5,  0.5, -0.5 }, { 1, 1, 0 }, { 0, 0 }},
    {{ -0.5,  0.5, -0.5 }, { 0, 1, 0 }, { 1, 0 }},
    {{  0.5, -0.5, -0.5 }, { 1, 0, 0 }, { 0, 1 }},
    {{ -0.5, -0.5, -0.5 }, { 0, 0, 0 }, { 1, 1 }},
    // +y
    {{  0.5,  0.5, -0.5 }, { 1, 1, 0 }, { 0, 0 }},
    {{  0.5,  0.5,  0.5 }, { 1, 1, 1 }, { 1, 0 }},
    {{ -0.5,  0.5, -0.5 }, { 0, 1, 0 }, { 0, 1 }},
    {{ -0.5,  0.5,  0.5 }, { 0, 1, 1 }, { 1, 1 }},
    // -y
    {{ -0.5, -0.5, -0.5 }, { 0, 0, 0 }, { 0, 0 }},
    {{ -0.5, -0.5,  0.5 }, { 0, 0, 1 }, { 1, 0 }},
    {{  0.5, -0.5, -0.5 }, { 1, 0, 0 }, { 0, 1 }},
    {{  0.5, -0.5,  0.5 }, { 1, 0, 1 }, { 1, 1 }}
});

// Textured cube indices
constexpr auto INDICES = std::to_array<u16>({
     0,  1,  2,  1,  3,  2,
     4,  5,  6,  5,  7,  6,
     8,  9, 10,  9, 11, 10,
    12, 13, 14, 13, 15, 14,
    16, 17, 18, 17, 19, 18,
    20, 21, 22, 21, 23, 22,
});

struct CameraUBO {
    glm::mat4x4 view;
    glm::mat4x4 proj;
};

struct CubeUBO {
    glm::mat4x4 model;
};

#ifdef DEBUG_BUILD
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#else
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#endif

void Engine::start() {
    spdlog::info("starting engine");

    const auto start = std::chrono::steady_clock::now();

    if (!sdl3_init())
        throw std::runtime_error("Failed to initialize SDL");

    init_window();
    init_graphics();
    init_imgui();

    SDL_ShowWindow(m_window);

    // Calculate initialization time.
    const auto end = std::chrono::steady_clock::now();
    f32 ms = std::chrono::duration_cast<std::chrono::duration<f32, std::milli>>(end - start).count();
    spdlog::info("startup complete ({:.3f} ms)", ms);
}

void Engine::run() {
    bool should_quit = false;

    while (!should_quit) {
        // Poll window events before rendering. (why is this not bound to the window?)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Pass the event to imgui to handle IO
            ImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                should_quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_Q) {
                    should_quit = true;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                auto& window_event = event.window;

                m_window_width = (u32) window_event.data1;
                m_window_height = (u32) window_event.data2;
                spdlog::info("window resized: {} {}", m_window_width, m_window_height);

                m_window_resized = true;
            } break;
            default:
                break;
            }
        }

        update_graphics();
    }

    quit();
}

void Engine::init_window() {
    constexpr u32 DEFAULT_WIDTH = 960;
    constexpr u32 DEFAULT_HEIGHT = 640;

    // Hide the window until we are done initializing GPU resources.
    // Maybe in the future we want to show some kind of splash screen when the loading process takes longer,
    // but for now this is fine.
    if ((m_window = SDL_CreateWindow("brampling3D", DEFAULT_WIDTH, DEFAULT_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)) == nullptr) {
        sdl3_perror("Failed to create window");
        throw std::runtime_error("Window initialization failed");
    }

    int width, height;
    SDL_GetWindowSize(m_window, &width, &height);
    m_window_width = (u32) width;
    m_window_height = (u32) height;

    SDL_SetWindowResizable(m_window, true);
    SDL_SetWindowMinimumSize(m_window, 640, 480);

    spdlog::info("Window initialized");
}

void Engine::quit() {
    spdlog::info("quitting");

    // Make sure the GPU isn't doing anything with the resources we are about to destroy.
    vkDeviceWaitIdle(device());

    ImGui_ImplVulkan_Shutdown();

    for (const auto fence : m_in_flight_fences) {
        vkDestroyFence(device(), fence, nullptr);
    }
    for (const auto semaphore : m_image_available_semaphores) {
        vkDestroySemaphore(device(), semaphore, nullptr);
    }
    
    vkFreeCommandBuffers(device(), m_command_pool, m_command_buffers.size(), m_command_buffers.data());
    vkDestroyCommandPool(device(), m_command_pool, nullptr);
    vkDestroyCommandPool(device(), m_transient_command_pool, nullptr);

    for (const auto& object : m_scene_objects) {
        for (const auto ubo : object.m_ubos) {
            vkDestroyBuffer(device(), ubo, nullptr);
        }
        for (const auto mem : object.m_ubo_memory) {
            vkUnmapMemory(device(), mem);
            vkFreeMemory(device(), mem, nullptr);
        }
        vkFreeDescriptorSets(device(), m_descriptor_pool, object.m_descriptor_sets.size(), object.m_descriptor_sets.data());
    }

    vkFreeDescriptorSets(device(), m_descriptor_pool, m_descriptor_sets.size(), m_descriptor_sets.data());
    vkDestroyDescriptorPool(device(), m_descriptor_pool, nullptr);

    for (const auto uniform_buffer_memory : m_camera_ubo_memory) {
        vkFreeMemory(device(), uniform_buffer_memory, nullptr);
    }
    for (const auto uniform_buffer : m_camera_ubos) {
        vkDestroyBuffer(device(), uniform_buffer, nullptr);
    }

    vkDestroySampler(device(), m_texture_sampler, nullptr);
    vkDestroyImageView(device(), m_texture_image_view, nullptr);
    vkFreeMemory(device(), m_texture_image_memory, nullptr);
    vkDestroyImage(device(), m_texture_image, nullptr);

    vkDestroyImageView(device(), m_depth_image_view, nullptr);
    vkDestroyImage(device(), m_depth_image, nullptr);
    vkFreeMemory(device(), m_depth_image_memory, nullptr);

    vkFreeMemory(device(), m_index_buffer_memory, nullptr);
    vkDestroyBuffer(device(), m_index_buffer, nullptr);

    vkFreeMemory(device(), m_vertex_buffer_memory, nullptr);
    vkDestroyBuffer(device(), m_vertex_buffer, nullptr);

    vkDestroyPipeline(device(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(device(), m_pipeline_layout, nullptr);

    vkDestroyDescriptorSetLayout(device(), m_descriptor_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device(), m_scene_object_descriptor_set_layout, nullptr);

    vkDeviceWaitIdle(device());

    m_swapchain.reset();
    m_device.reset();

    SDL_Vulkan_DestroySurface(m_instance, m_window_surface, nullptr);

    vkDestroyInstance(m_instance, nullptr);

    SDL_DestroyWindow(m_window);
    SDL_Quit();

    spdlog::info("Goodbye!");
}

void Engine::init_graphics() {
    create_instance();

    create_window_surface();
    
    m_device = std::make_unique<VulkanDevice>(m_instance, m_window_surface);
    m_swapchain = std::make_unique<VulkanSwapchain>(physical_device(), device(), m_window_surface);

    // Do things that depend on surface_format...

    m_swapchain->create(m_window_width, m_window_height);

    create_command_pools();

    create_depth_image();

    create_texture_image();
    create_texture_image_view();
    create_texture_sampler();

    create_descriptor_set_layouts();

    create_graphics_pipeline();

    create_camera_ubos();

    create_descriptor_pool();

    create_descriptor_sets();

    create_vertex_buffer();
    create_index_buffer();

    create_scene_objects();

    create_command_buffers();

    create_sync_objects();

    spdlog::info("Vulkan initialized");
}

void Engine::init_imgui() {
    // Make sure imgui was linked correctly
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard

    // Setup style and scaling
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    // Setup backends
    ImGui_ImplSDL3_InitForVulkan(m_window);
    
    const auto color_attachment_formats = std::to_array({ m_swapchain->surface_format().format });
    ImGui_ImplVulkan_InitInfo init_info{
        .ApiVersion = ENGINE_VULKAN_API_VERSION,
        .Instance = m_instance,
        .PhysicalDevice = physical_device(),
        .Device = device(),
        .QueueFamily = m_device->graphics_family(),
        .Queue = graphics_queue(),
        .DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
        .MinImageCount = m_swapchain->min_image_count(),
        .ImageCount = (u32) m_swapchain->image_count(),
        .PipelineInfoMain = {
            .Subpass = 0,
            .PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = color_attachment_formats.size(),
                .pColorAttachmentFormats = color_attachment_formats.data()
            }
        },
        .UseDynamicRendering = true
    };
    ImGui_ImplVulkan_Init(&init_info);

    spdlog::info("ImGui initialized");
}

void Engine::create_instance() {
    // Get instance extensions needed for vkCreateInstance
    u32 extension_count;
    auto* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (!extensions) {
        sdl3_perror("Failed to get vulkan instance extensions");
        throw std::runtime_error("Vulkan initialization failed");
    }

    std::vector enable_extensions(extensions, extensions + extension_count);

    u32 supported_extension_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, nullptr);
    std::vector<VkExtensionProperties> supported_extensions;
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, supported_extensions.data());

    // Try to enable VK_KHR_portability_enumeration for MoltenVK support.
    constexpr auto DESIRED_EXTENSIONS = std::to_array({ VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME });

    for (auto desired_extension : DESIRED_EXTENSIONS) {
        for (const auto& extension : supported_extensions) {
            if (extension.extensionName == std::string_view{desired_extension}) {
                enable_extensions.push_back(desired_extension);
                break;
            }
        }
    }

    u32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    std::vector<const char*> enabled_layers;

    if constexpr (ENABLE_VALIDATION_LAYERS) {
        constexpr auto DESIRED_LAYERS = std::to_array({ "VK_LAYER_KHRONOS_validation" });

        for (auto desired_layer : DESIRED_LAYERS) {
            bool available = false;

            for (const auto& layer : available_layers) {
                if (layer.layerName == std::string_view{desired_layer}) {
                    enabled_layers.push_back(desired_layer);
                    available = true;
                    break;
                }
            }

            if (!available)
                spdlog::warn("validation layer unavailable: {}", desired_layer);
        }
    }

    // ApplicationInfo lets drivers enable application-specific optimizations.
    // So Intel, NVIDIA, and AMD can implement the best optimizations for brampling3D (future GOTY. thanks guys)
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "brampling3D",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "brampling3D",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = ENGINE_VULKAN_API_VERSION 
    };

    VkInstanceCreateFlags create_flags = 0;

    if (std::ranges::contains(enable_extensions, std::string_view{ VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME })) {
        create_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    // Create a vulkan instance with the required extensions
    VkInstanceCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags = create_flags,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = (u32) enabled_layers.size(),
        .ppEnabledLayerNames = enabled_layers.data(),
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
    };

    vulkan_check_res(
        vkCreateInstance(&create_info, nullptr, &m_instance),
        "Failed to create vulkan instance"
    );
}

void Engine::create_window_surface() {
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_window_surface)) {
        sdl3_perror("Failed to create vulkan surface");
        throw std::runtime_error("Vulkan initialization failed");
    }
}

void Engine::create_command_pools() {
    VkCommandPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_device->graphics_family(),
    };
    vulkan_check_res(
        vkCreateCommandPool(device(), &pool_info, nullptr, &m_command_pool),
        "failed to create command pool"
    );

    // Create a separate command pool for transient (single time) command buffers.
    // Per the Vulkan spec this can allow the driver to optimize for this use case.
    // (In practice, all the open source vulkan drivers I've seen don't do anything with that flag :()
    VkCommandPoolCreateInfo transient_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = m_device->graphics_family()
    };
    vulkan_check_res(
        vkCreateCommandPool(device(), &transient_pool_info, nullptr, &m_transient_command_pool),
        "failed to create command pool"
    );
}

void Engine::create_descriptor_set_layouts() {
    {
        VkDescriptorSetLayoutBinding ubo_binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        }; 

        VkDescriptorSetLayoutBinding sampler_layout_binding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        };

        const auto bindings = std::to_array({ ubo_binding, sampler_layout_binding });

        VkDescriptorSetLayoutCreateInfo layout_create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = 0,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data()
        };
        vulkan_check_res(
            vkCreateDescriptorSetLayout(device(), &layout_create_info, nullptr, &m_descriptor_set_layout),
            "failed to create descriptor set layout"
        );
    }

    {
        VkDescriptorSetLayoutBinding ubo_binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        }; 

        const auto bindings = std::to_array({ ubo_binding });

        VkDescriptorSetLayoutCreateInfo layout_create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = 0,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data()
        };
        vulkan_check_res(
            vkCreateDescriptorSetLayout(device(), &layout_create_info, nullptr, &m_scene_object_descriptor_set_layout),
            "failed to create scene object descriptor set layout"
        );
    }
}

void Engine::create_graphics_pipeline() {
    std::vector<u8> vert_shader(std::from_range, get_asset<"shaders/color3d.vertex.spv">());
    std::vector<u8> frag_shader(std::from_range, get_asset<"shaders/color3d.fragment.spv">());

    VkShaderModuleCreateInfo vert_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_shader.size(),
        .pCode = reinterpret_cast<const u32*>(vert_shader.data())
    };

    VkShaderModule vert_shader_module;
    vulkan_check_res(
        vkCreateShaderModule(device(), &vert_module_info, nullptr, &vert_shader_module),
        "failed to create vertex shader"
    );

    VkShaderModuleCreateInfo frag_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_shader.size(),
        .pCode = reinterpret_cast<const u32*>(frag_shader.data())
    };

    VkShaderModule frag_shader_module;
    vulkan_check_res(
        vkCreateShaderModule(device(), &frag_module_info, nullptr, &frag_shader_module),
        "failed to create fragment shader"
    );

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

    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
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

    const auto set_layouts = std::to_array({ m_descriptor_set_layout, m_scene_object_descriptor_set_layout });
    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (u32) set_layouts.size(),
        .pSetLayouts = set_layouts.data()
    };

    vulkan_check_res(
        vkCreatePipelineLayout(device(), &pipeline_layout_info, nullptr, &m_pipeline_layout),
        "failed to create pipeline layout"
    );

    const auto color_attachment_formats = std::to_array({ m_swapchain->surface_format().format });

    VkPipelineRenderingCreateInfo pipeline_rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = color_attachment_formats.size(),
        .pColorAttachmentFormats = color_attachment_formats.data(),
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
    };

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_info,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state_info,
        .layout = m_pipeline_layout,
        // We use dynamic rendering instead of a render pass.
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0
    };

    vulkan_check_res(
        vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline),
        "failed to create graphics pipeline"
    );

    vkDestroyShaderModule(device(), frag_shader_module, nullptr);
    vkDestroyShaderModule(device(), vert_shader_module, nullptr);
}

void Engine::create_depth_image() {
    // TODO: Test for allowed formats
    const auto depth_format = VK_FORMAT_D32_SFLOAT;
    u32 width = m_swapchain->extent().width;
    u32 height = m_swapchain->extent().height;

    // Create the texture image.
    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depth_format,
        .extent = { .width = width, .height = height, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    vulkan_check_res(
        vkCreateImage(device(), &image_info, nullptr, &m_depth_image),
        "failed to create depth image"
    );

    // Allocate memory for the image.
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device(), m_depth_image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = choose_memory_type(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    vulkan_check_res(
        vkAllocateMemory(device(), &alloc_info, nullptr, &m_depth_image_memory),
        "failed to allocate depth image memory"
    );

    vkBindImageMemory(device(), m_depth_image, m_depth_image_memory, 0);

    VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
    vulkan_check_res(
        vkCreateImageView(device(), &view_info, nullptr, &m_depth_image_view),
        "failed to create depth image view"
    );
}


void Engine::create_texture_image() {
    auto image = stb::Image::from_bytes(get_asset<"images/soggy.png">(), 4);
    if (!image)
        throw std::runtime_error("failed to load soggy.png");

    VkDeviceSize image_size = image->width() * image->height() * 4;

    // Create a staging buffer to upload our texture to.
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

    // Upload the texture.
    void* data;
    vkMapMemory(device(), staging_buffer_memory, 0, image_size, 0, &data);
    memcpy(data, image->data(), (usize) image_size);
    vkUnmapMemory(device(), staging_buffer_memory);

    // Create the texture image.
    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = { .width = image->width(), .height = image->height(), .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    vulkan_check_res(
        vkCreateImage(device(), &image_info, nullptr, &m_texture_image),
        "failed to create texture image"
    );

    // Allocate memory for the image.
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device(), m_texture_image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = choose_memory_type(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    vulkan_check_res(
        vkAllocateMemory(device(), &alloc_info, nullptr, &m_texture_image_memory),
        "failed to allocate texture image memory"
    );

    vkBindImageMemory(device(), m_texture_image, m_texture_image_memory, 0);

    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        // We can transfer an image between queues this way. We could hypothetically use a separate queue to upload images, allowing async texture uploads.
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_texture_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
    };

    // TODO: Use vkCmdPipelineBarrier2 provided by Vulkan 1.3
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // Copy the staging buffer.

    VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = { .x = 0, .y = 0, .z = 0 },
        .imageExtent = { .width = image->width(), .height = image->height(), .depth =  1 }
    };

    vkCmdCopyBufferToImage(
        command_buffer,
        staging_buffer,
        m_texture_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    // Prepare the texture for shader use.

    VkImageMemoryBarrier barrier2{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        // We can transfer an image between queues this way. We could hypothetically use a separate queue to upload images, allowing async texture uploads.
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_texture_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
    };

    // TODO: Use vkCmdPipelineBarrier2 provided by Vulkan 1.3
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier2
    );

    end_single_time_commands(command_buffer);

    // Cleanup staging buffer.
    vkDestroyBuffer(device(), staging_buffer, nullptr);
    vkFreeMemory(device(), staging_buffer_memory, nullptr);
}

void Engine::create_texture_image_view() {
    VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_texture_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
    vulkan_check_res(
        vkCreateImageView(device(), &view_info, nullptr, &m_texture_image_view),
        "failed to create texture image view"
    );
}

void Engine::create_texture_sampler() {
    VkSamplerCreateInfo sampler_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = m_device->physical_device_properties().limits.maxSamplerAnisotropy,
        // TODO: what's this?
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0,
        .maxLod = 0,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    vulkan_check_res(
        vkCreateSampler(device(), &sampler_info, nullptr, &m_texture_sampler),
        "failed to create texture sampler"
    );
}

void Engine::create_camera_ubos() {
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        create_buffer(
            sizeof(CameraUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_camera_ubos[i],
            m_camera_ubo_memory[i]
        );
    }
}

void Engine::create_descriptor_pool() {
    VkDescriptorPoolSize ubo_pool_size{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT + MAX_FRAMES_IN_FLIGHT * 2
    };

    VkDescriptorPoolSize sampler_pool_size{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };

    const auto pool_sizes = std::to_array({ ubo_pool_size, sampler_pool_size });

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = MAX_FRAMES_IN_FLIGHT + MAX_FRAMES_IN_FLIGHT * 2,
        .poolSizeCount = pool_sizes.size(),
        .pPoolSizes = pool_sizes.data()
    };
    VkResult res = vkCreateDescriptorPool(device(), &pool_info, nullptr, &m_descriptor_pool);
    vulkan_check_res(res, "failed to create descriptor pool");
}

void Engine::create_descriptor_sets() {
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    std::ranges::fill(layouts, m_descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptor_pool,
        .descriptorSetCount = (u32) layouts.size(),
        .pSetLayouts = layouts.data()
    };

    VkResult res = vkAllocateDescriptorSets(device(), &alloc_info, m_descriptor_sets.data());
    vulkan_check_res(res, "failed to allocate descriptor sets");

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = m_camera_ubos[i],
            .offset = 0,
            .range = sizeof(CameraUBO),
        };

        VkDescriptorImageInfo image_info{
            .sampler = m_texture_sampler,
            .imageView = m_texture_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        
        auto descriptor_writes = std::to_array<VkWriteDescriptorSet>({
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptor_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptor_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info
            }
        });

        vkUpdateDescriptorSets(device(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
}

void Engine::create_vertex_buffer() {
    usize size = sizeof(Vertex) * VERTICES.size();

    create_buffer(
        size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_vertex_buffer,
        m_vertex_buffer_memory
    );

    // Upload vertex data.
    void* data;
    vkMapMemory(device(), m_vertex_buffer_memory, 0, size, 0, &data);
    memcpy(data, VERTICES.data(), size);
    vkUnmapMemory(device(), m_vertex_buffer_memory);
}

void Engine::create_index_buffer() {
    usize size = sizeof(u16) * INDICES.size();

    create_buffer(
        size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_index_buffer,
        m_index_buffer_memory
    );

    // Upload index data.
    void* data;
    vkMapMemory(device(), m_index_buffer_memory, 0, size, 0, &data);
    memcpy(data, INDICES.data(), size);
    vkUnmapMemory(device(), m_index_buffer_memory);
}

void Engine::create_scene_objects() {
    const auto poses = std::to_array<glm::vec3>({
        { 0, 0, 0 },
        { 1.5, 0, 1.5 },
    });

    const auto rotate_axes = std::to_array<glm::vec3>({
        { 0, 1, 0 },
        { 0, 0, 1 },
    });

    for (usize i = 0; i < poses.size(); i++) {
        CubeObject cube{
            .m_pos = poses[i],
            .m_rotate_axis = rotate_axes[i],
        };

        for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            create_buffer(
                sizeof(CubeUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                cube.m_ubos[i],
                cube.m_ubo_memory[i]
            );

            vkMapMemory(device(), cube.m_ubo_memory[i], 0, sizeof(CubeUBO), 0, &cube.m_ubo_data[i]);
        }

        std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
        std::ranges::fill(layouts, m_scene_object_descriptor_set_layout);

        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_descriptor_pool,
            .descriptorSetCount = (u32) layouts.size(),
            .pSetLayouts = layouts.data()
        };

        vulkan_check_res(
            vkAllocateDescriptorSets(device(), &alloc_info, cube.m_descriptor_sets.data()),
            "failed to allocate CubeObject descriptor sets"
        );

        for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo buffer_info{
                .buffer = cube.m_ubos[i],
                .offset = 0,
                .range = sizeof(CubeUBO),
            };

            auto descriptor_writes = std::to_array<VkWriteDescriptorSet>({
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = cube.m_descriptor_sets[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &buffer_info,
                }
            });
            vkUpdateDescriptorSets(device(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
        }

        m_scene_objects.push_back(cube);
    }
}

void Engine::create_command_buffers() {
    // Allocate a command buffer for each swapchain image.
    VkCommandBufferAllocateInfo alloc_info_cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };

    vulkan_check_res(
        vkAllocateCommandBuffers(device(), &alloc_info_cmd, m_command_buffers.data()),
        "failed to allocate command buffers"
    );
}

void Engine::create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vulkan_check_res(
            vkCreateSemaphore(device(), &semaphore_info, nullptr, &m_image_available_semaphores[i]),
            "failed to create image available semaphore {}", i
        );

        vulkan_check_res(
            vkCreateFence(device(), &fence_info, nullptr, &m_in_flight_fences[i]),
            "failed to create in-flight fence {}", i
        );
    }
}

u32 Engine::choose_memory_type(u32 memory_type_bits, VkMemoryPropertyFlags mem_flags) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device(), &mem_properties);

    u32 memory_type_index = UINT32_MAX;
    for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((memory_type_bits & (i << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & mem_flags)) {
            memory_type_index = i;
            break;
        }
    }
    if (memory_type_index == UINT32_MAX) {
        throw std::runtime_error("failed to find suitable memory type for buffer");
    }

    return memory_type_index;
}

void Engine::create_buffer(usize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_flags, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    vulkan_check_res(
        vkCreateBuffer(device(), &buffer_info, nullptr, &buf),
        "failed to create buffer"
    );

    // Find suitable memory type.
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device(), buf, &mem_requirements);

    u32 memory_type_index = choose_memory_type(mem_requirements.memoryTypeBits, mem_flags);

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index
    };

    vulkan_check_res(
        vkAllocateMemory(device(), &alloc_info, nullptr, &mem),
        "failed to allocate buffer memory"
    );

    vkBindBufferMemory(device(), buf, mem, 0);
}

VkCommandBuffer Engine::begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info_cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_transient_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1 
    };
    VkCommandBuffer command_buffer;
    vulkan_check_res(
        vkAllocateCommandBuffers(device(), &alloc_info_cmd, &command_buffer),
        "failed to allocate transfer command buffer"
    );

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vulkan_check_res(
        vkBeginCommandBuffer(command_buffer, &begin_info),
        "failed to begin single time command buffer"
    );

    return command_buffer;
}

void Engine::end_single_time_commands(VkCommandBuffer command_buffer) {
    vulkan_check_res(
        vkEndCommandBuffer(command_buffer),
        "failed to end single time command buffer"
    );

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer
    };

    // TODO: Return a fence for the caller to wait on to allow less stalling 
    // (let's us start single time tasks while others are already in flight and wait on all of them in one go... or just one since queues execute command buffers sequentially)
    vkQueueSubmit(graphics_queue(), 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue());

    vkFreeCommandBuffers(device(), m_transient_command_pool, 1, &command_buffer);
}

void Engine::recreate_swapchain() {
    // Wait for the device to be idle before recreating the swapchain
    vkDeviceWaitIdle(device());

    m_swapchain->reset();
    // TODO: recreate render pass if the surface format changed
    m_swapchain->create(m_window_width, m_window_height);

    // Update the ImGui backend
    ImGui_ImplVulkan_SetMinImageCount(m_swapchain->min_image_count());

    spdlog::info("finished swapchain recreate");
}

void Engine::update_graphics() {
    // Handle swapchain recreation before rendering a frame.
    if (m_window_resized || m_need_swapchain_recreate) {
        m_window_resized = m_need_swapchain_recreate = false;

        // We update window size in the SDL resize event, but let's double check 
        // that it's correct in case the state somehow gets out of sync
        int width, height;
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
        m_window_width = (u32) width;
        m_window_height = (u32) height;

        recreate_swapchain();
    }

    render_frame();
}

void Engine::render_frame() {
    VkSemaphore image_available_semaphore = m_image_available_semaphores[m_current_frame];
    VkFence in_flight_fence = m_in_flight_fences[m_current_frame];

    vkWaitForFences(device(), 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device(), 1, &in_flight_fence);

    VkResult acquire_result = m_swapchain->acquire(image_available_semaphore, m_image_index);

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

        f32 seconds = std::chrono::duration_cast<std::chrono::duration<f32>>(now - start).count();

        auto extent = m_swapchain->extent();
        auto aspect = (f32) extent.width / extent.height;

        CameraUBO camera_ubo;
        camera_ubo.view = glm::lookAt(glm::vec3(6, 5, 6), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        camera_ubo.proj = glm::perspective(glm::radians(45.f), aspect, 0.1f, 100.f);
        camera_ubo.proj[1][1] *= -1; // Flip vertically to correct the screen space coordinates

        // Write Camera UBO.
        auto mem = m_camera_ubo_memory[m_current_frame];
        void* data;
        vkMapMemory(device(), mem, 0, sizeof(CameraUBO), 0, &data);
        memcpy(data, &camera_ubo, sizeof(CameraUBO));
        vkUnmapMemory(device(), mem);

        for (const auto& object : m_scene_objects) {
            auto id = glm::identity<glm::mat4x4>();
            auto rotate = glm::rotate(id, glm::radians(seconds * 180), object.m_rotate_axis);
            auto translate = glm::translate(id, object.m_pos);

            CubeUBO cube_ubo{
                .model = rotate * translate
            };

            // Write Cube UBO.
            memcpy(object.m_ubo_data[m_current_frame], &cube_ubo, sizeof(CubeUBO));
        }
    }

    // Transition the swapchain image to be suitable for rendering.
    VkImageMemoryBarrier memory_barrier_1{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = m_swapchain->image(m_image_index),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &memory_barrier_1
    );

    VkImageMemoryBarrier depth_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .image = m_depth_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &depth_memory_barrier
    );
    
    VkClearValue clear_col = { .color = { .float32 = {0.2, 0.2, 0.2, 1 } } };
    VkClearValue clear_depth = { .depthStencil = { .depth = 1.0, .stencil = 0 } };

    VkRenderingAttachmentInfo color_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_swapchain->image_view(m_image_index),
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_col
    };

    VkRenderingAttachmentInfo depth_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = clear_depth
    };

    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = m_swapchain->extent()
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment = &depth_attachment,
    };

    vkCmdBeginRendering(command_buffer, &rendering_info);

    // Set viewport and scissor, which are dynamic.
    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = (f32) m_swapchain->extent().width,
        .height = (f32) m_swapchain->extent().height,
        .minDepth = 0,
        .maxDepth = 1
    };

    VkRect2D scissor{
        .offset = { 0, 0 },
        .extent = m_swapchain->extent()
    };

    VkBuffer vertex_bufs[] = { m_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_bufs, offsets);
    vkCmdBindIndexBuffer(command_buffer, m_index_buffer, 0, VK_INDEX_TYPE_UINT16);

    for (const auto& object : m_scene_objects) {
        // Bind the object's descriptor set.
        const auto descriptor_sets = std::to_array({ m_descriptor_sets[m_current_frame], object.m_descriptor_sets[m_current_frame] });
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);

        // Draw the object.
        vkCmdDrawIndexed(command_buffer, (u32) INDICES.size(), 1, 0, 0, 0);
    }

    vkCmdEndRendering(command_buffer);

    // Render imgui.
    render_imgui(command_buffer);

    // Transition the image back to be suitable for presenting.
    VkImageMemoryBarrier memory_barrier_2{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = m_swapchain->image(m_image_index),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &memory_barrier_2
    );

    vulkan_check_res(
        vkEndCommandBuffer(command_buffer),
        "failed to end command buffer"
    );

    VkSemaphore wait_semaphores[] = { image_available_semaphore };
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    auto signal_semaphores = std::to_array({ m_swapchain->submit_semaphore(m_image_index) });

    // Submit the command buffer.
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = (u32) signal_semaphores.size(),
        .pSignalSemaphores = signal_semaphores.data()
    };

    VkResult submit_result = vkQueueSubmit(graphics_queue(), 1, &submit_info, in_flight_fence);
    vulkan_check_res(submit_result, "failed to submit draw command buffer for {}", m_image_index);

    VkResult present_result = m_swapchain->present(present_queue(), signal_semaphores, m_image_index);

    if (present_result == VK_SUBOPTIMAL_KHR || present_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Recreate swapchain next frame. We usually get this right before SDL sends a resize event anyway
        m_need_swapchain_recreate = true;
    } else if (present_result != VK_SUCCESS) {
        vulkan_check_res(present_result, "failed to present image {}", m_image_index);
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::render_imgui(VkCommandBuffer command_buffer) {
    VkRenderingAttachmentInfo color_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_swapchain->image_view(m_image_index),
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = m_swapchain->extent()
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment
    };

    vkCmdBeginRendering(command_buffer, &rendering_info);

    // TODO: A dedicated render pass for imgui.
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Debug");

    imgui_text("brampling3D ({} {})", SDL_GetPlatform(), ENGINE_SYSTEM_PROCESSOR);
    imgui_text("Device: {}", m_device->device_name());
    auto& io = ImGui::GetIO();
    imgui_text("Frame time: {:.3f} ms ({:.1f} FPS)", 1000.0 / io.Framerate, io.Framerate);

    ImGui::Separator();

    imgui_text("Settings");
    ImGui::Checkbox("V-sync", &m_vsync);

    if (m_vsync != m_swapchain->vsync()) {
        // Update swapchain if vsync setting changed
        m_swapchain->set_vsync(m_vsync);
        m_need_swapchain_recreate = true;
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

    vkCmdEndRendering(command_buffer);
}
