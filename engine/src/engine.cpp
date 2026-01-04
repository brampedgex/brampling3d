#include "engine.hpp"

#include "assets.hpp"

#include "util/imgui.hpp"
#include "util/stb.hpp"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec2 tex_coord;

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
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, tex_coord)
            }
        });
    }
};


// Textured cube vertices
constexpr auto VERTICES = std::to_array<Vertex>({
    // -x
    {{ -0.5,  0.5, -0.5 }, { 0, 0 }},
    {{ -0.5,  0.5,  0.5 }, { 1, 0 }},
    {{ -0.5, -0.5, -0.5 }, { 0, 1 }},
    {{ -0.5, -0.5,  0.5 }, { 1, 1 }},
    // +z
    {{ -0.5,  0.5,  0.5 }, { 0, 0 }},
    {{  0.5,  0.5,  0.5 }, { 1, 0 }},
    {{ -0.5, -0.5,  0.5 }, { 0, 1 }},
    {{  0.5, -0.5,  0.5 }, { 1, 1 }},
    // +x
    {{  0.5,  0.5,  0.5 }, { 0, 0 }},
    {{  0.5,  0.5, -0.5 }, { 1, 0 }},
    {{  0.5, -0.5,  0.5 }, { 0, 1 }},
    {{  0.5, -0.5, -0.5 }, { 1, 1 }},
    // -z
    {{  0.5,  0.5, -0.5 }, { 0, 0 }},
    {{ -0.5,  0.5, -0.5 }, { 1, 0 }},
    {{  0.5, -0.5, -0.5 }, { 0, 1 }},
    {{ -0.5, -0.5, -0.5 }, { 1, 1 }},
    // +y
    {{  0.5,  0.5, -0.5 }, { 0, 0 }},
    {{  0.5,  0.5,  0.5 }, { 1, 0 }},
    {{ -0.5,  0.5, -0.5 }, { 0, 1 }},
    {{ -0.5,  0.5,  0.5 }, { 1, 1 }},
    // -y
    {{ -0.5, -0.5, -0.5 }, { 0, 0 }},
    {{ -0.5, -0.5,  0.5 }, { 1, 0 }},
    {{  0.5, -0.5, -0.5 }, { 0, 1 }},
    {{  0.5, -0.5,  0.5 }, { 1, 1 }}
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

struct CubemapVertex {
    glm::vec3 dir;

    static VkVertexInputBindingDescription binding_description() {
        return {
            .binding = 0,
            .stride = sizeof(CubemapVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
    }

    static std::array<VkVertexInputAttributeDescription, 1> attribute_descriptions() {
        return std::to_array<VkVertexInputAttributeDescription>({
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(CubemapVertex, dir),
            },
        });
    }
};

constexpr auto CUBEMAP_VERTICES = std::to_array<CubemapVertex>({
    {{ -1, -1, -1 }},
    {{ -1, -1, 1 }},
    {{ -1, 1, -1 }},
    {{ -1, 1, 1 }},
    {{ 1, -1, -1 }},
    {{ 1, -1, 1 }},
    {{ 1, 1, -1 }},
    {{ 1, 1, 1 }},
});

constexpr auto CUBEMAP_INDICES = std::to_array<u16>({
    // -z  
    2, 6, 0, 6, 4, 0,
    // +x
    6, 7, 4, 7, 5, 4,
    // +z
    7, 3, 5, 3, 1, 5,
    // -x
    3, 2, 1, 2, 0, 1,
    // +y
    3, 7, 2, 7, 6, 2,
    // -y
    0, 4, 1, 4, 5, 1,
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
    init_scene();

    SDL_ShowWindow(m_window);
    SDL_SetWindowRelativeMouseMode(m_window, true);

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
            case SDL_EVENT_KEY_DOWN: {
                auto& key_event = event.key;

                switch (key_event.key) {
                case SDLK_Q:
                    should_quit = true;
                    break;
                case SDLK_ESCAPE:
                    m_grab_mouse = !m_grab_mouse;
                    SDL_SetWindowRelativeMouseMode(m_window, m_grab_mouse);
                    break;
                }
            } break;
            case SDL_EVENT_MOUSE_MOTION: {
                auto& motion_event = event.motion;

                // If mouse is grabbed, do camera look
                if (m_grab_mouse) {
                    f32 x_rel = motion_event.xrel;
                    f32 y_rel = motion_event.yrel;

                    f32 x_degrees = x_rel * 0.1;
                    f32 y_degrees = y_rel * -0.1; // y_rel goes downwards in window space

                    m_camera.set_yaw(m_camera.yaw() + x_degrees);

                    // Clamp the pitch so the camera doesn't go upside down.
                    f32 new_pitch = m_camera.pitch() + y_degrees;
                    new_pitch = std::clamp(new_pitch, -90.f, 90.f);
                    m_camera.set_pitch(new_pitch);

                    m_camera.update_rot();
                }
            } break;
            case SDL_EVENT_WINDOW_RESIZED: {
                auto& window_event = event.window;

                m_window_width = (u32) window_event.data1;
                m_window_height = (u32) window_event.data2;
                m_window_resized = true;
            } break;
            default:
                break;
            }
        }

        update();
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

    for (const auto mem : m_camera_ubo_memory) {
        vkUnmapMemory(device(), mem);
        vkFreeMemory(device(), mem, nullptr);
    }
    for (const auto ubo : m_camera_ubos) {
        vkDestroyBuffer(device(), ubo, nullptr);
    }

    vkDestroySampler(device(), m_cubemap_sampler, nullptr);
    vkDestroyImageView(device(), m_cubemap_image_view, nullptr);
    vkDestroyImage(device(), m_cubemap_image, nullptr);
    vkFreeMemory(device(), m_cubemap_memory, nullptr);

    vkDestroySampler(device(), m_texture_sampler, nullptr);
    vkDestroyImageView(device(), m_texture_image_view, nullptr);
    vkFreeMemory(device(), m_texture_image_memory, nullptr);
    vkDestroyImage(device(), m_texture_image, nullptr);

    vkDestroyImageView(device(), m_depth_image_view, nullptr);
    vkDestroyImage(device(), m_depth_image, nullptr);
    vkFreeMemory(device(), m_depth_image_memory, nullptr);

    vkFreeMemory(device(), m_cubemap_vertex_buffer_memory, nullptr);
    vkFreeMemory(device(), m_cubemap_index_buffer_memory, nullptr);
    vkDestroyBuffer(device(), m_cubemap_vertex_buffer, nullptr);
    vkDestroyBuffer(device(), m_cubemap_index_buffer, nullptr);

    vkFreeMemory(device(), m_index_buffer_memory, nullptr);
    vkFreeMemory(device(), m_vertex_buffer_memory, nullptr);
    vkDestroyBuffer(device(), m_index_buffer, nullptr);
    vkDestroyBuffer(device(), m_vertex_buffer, nullptr);

    vkDestroyPipeline(device(), m_cubemap_pipeline, nullptr);
    vkDestroyPipelineLayout(device(), m_cubemap_pipeline_layout, nullptr);
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

    create_cubemap_image();
    create_cubemap_image_view();
    create_cubemap_sampler();

    create_descriptor_set_layouts();

    create_graphics_pipeline();
    create_cubemap_pipeline();

    create_camera_ubos();

    create_descriptor_pool();

    create_descriptor_sets();

    create_vertex_buffer();
    create_index_buffer();

    create_cubemap_buffers();

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

void Engine::init_scene() {
    // Setup camera.
    m_camera.set_pos({ 0, 0, 10 });
    m_camera.set_pitch(0);
    m_camera.set_yaw(0);
    m_camera.set_fov(45);

    m_camera.update_rot();
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

        VkDescriptorSetLayoutBinding soggy_sampler_binding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        };

        VkDescriptorSetLayoutBinding skybox_sampler_binding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };

        const auto bindings = std::to_array({ ubo_binding, soggy_sampler_binding, skybox_sampler_binding });

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
    std::vector<u8> vert_shader(std::from_range, get_asset<"shaders/soggycube.vertex.spv">());
    std::vector<u8> frag_shader(std::from_range, get_asset<"shaders/soggycube.fragment.spv">());

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

void Engine::create_cubemap_pipeline() {
    std::vector<u8> vert_shader(std::from_range, get_asset<"shaders/skybox.vertex.spv">());
    std::vector<u8> frag_shader(std::from_range, get_asset<"shaders/skybox.fragment.spv">());

    VkShaderModuleCreateInfo vert_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_shader.size(),
        .pCode = reinterpret_cast<const u32*>(vert_shader.data())
    };

    VkShaderModule vert_shader_module;
    vulkan_check_res(
        vkCreateShaderModule(device(), &vert_module_info, nullptr, &vert_shader_module),
        "failed to create skybox vertex shader"
    );

    VkShaderModuleCreateInfo frag_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_shader.size(),
        .pCode = reinterpret_cast<const u32*>(frag_shader.data())
    };

    VkShaderModule frag_shader_module;
    vulkan_check_res(
        vkCreateShaderModule(device(), &frag_module_info, nullptr, &frag_shader_module),
        "failed to create skybox fragment shader"
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

    auto binding_desc = CubemapVertex::binding_description();
    auto attr_descs = CubemapVertex::attribute_descriptions();

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

    const auto set_layouts = std::to_array({ m_descriptor_set_layout });
    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (u32) set_layouts.size(),
        .pSetLayouts = set_layouts.data()
    };

    vulkan_check_res(
        vkCreatePipelineLayout(device(), &pipeline_layout_info, nullptr, &m_cubemap_pipeline_layout),
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
        .layout = m_cubemap_pipeline_layout,
        // We use dynamic rendering instead of a render pass.
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0
    };

    vulkan_check_res(
        vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_cubemap_pipeline),
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

    create_image_2d(
        width,
        height,
        depth_format,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_depth_image,
        m_depth_image_memory
    );

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
    create_buffer(
        image_size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        staging_buffer, 
        staging_buffer_memory
    );

    // Upload the texture.
    void* data;
    vkMapMemory(device(), staging_buffer_memory, 0, image_size, 0, &data);
    memcpy(data, image->data(), (usize) image_size);
    vkUnmapMemory(device(), staging_buffer_memory);

    // Create the texture image.
    create_image_2d(
        image->width(), 
        image->height(), 
        VK_FORMAT_R8G8B8A8_SRGB, 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        m_texture_image, 
        m_texture_image_memory
    );

    VkCommandBuffer command_buffer = begin_single_time_commands();

    transition_image_layout(
        command_buffer, 
        m_texture_image, 
        0, 
        VK_ACCESS_TRANSFER_WRITE_BIT, 
        VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        VK_PIPELINE_STAGE_TRANSFER_BIT
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
    transition_image_layout(
        command_buffer, 
        m_texture_image, 
        VK_ACCESS_TRANSFER_WRITE_BIT, 
        VK_ACCESS_SHADER_READ_BIT, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_TRANSFER_BIT, 
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
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

void Engine::create_cubemap_image() {
    // Load the skybox image.
    auto skybox_image = stb::Image::from_bytes(get_asset<"images/skybox.png">(), 4);
    if (!skybox_image)
        throw std::runtime_error("Failed to load skybox.png");

    // Convert from the equirectangular image to six cubemap faces.
    const u32 face_size = skybox_image->height();
    std::vector<std::vector<u8>> face_data;

    face_data.resize(6);
    for (auto& face : face_data) {
        face.resize(face_size * face_size * 4);
    }

    auto get_direction = [](u8 face, f32 u, f32 v) -> glm::vec3 {
        switch (face) {
        case 0: return { 1, -v, -u };
        case 1: return { -1, -v, u };
        case 2: return { u, 1, v };
        case 3: return { u, -1, -v };
        case 4: return { u, -v, 1 };
        case 5: return { -u, -v, -1 };
        default: 
            return {};
        }
    };

    auto get_pixel = [&](u32 x, u32 y) -> glm::vec4 {
        const u8* pixel = skybox_image->data() + (y * skybox_image->width() + x) * 4;
        return {
            pixel[0] * 255.f,
            pixel[1] * 255.f,
            pixel[2] * 255.f,
            pixel[3] * 255.f
        };
    };

    for (u8 face = 0; face < 6; face++) {
        u8* data = face_data[face].data();

        for (u32 y = 0; y < face_size; y++) {
            for (u32 x = 0; x < face_size; x++) {
                f32 face_u = 2 * ((f32) x / face_size) - 1;
                f32 face_v = 2 * ((f32) y / face_size) - 1;

                auto dir = glm::normalize(get_direction(face, face_u, face_v));

                f32 theta = std::acos(dir.y);
                f32 phi = std::atan2(dir.z, dir.x);

                f32 u = phi / (2 * std::numbers::pi_v<f32>);
                f32 v = theta / std::numbers::pi_v<f32>;

                // phi ranges from [-pi, pi] so correct it after dividing.
                if (u < 0)
                    u += 1;

                u = std::clamp(u * (f32) skybox_image->width(), 0.f, (f32) skybox_image->width() - 1);
                v = std::clamp(v * (f32) skybox_image->height(), 0.f, (f32) skybox_image->height() - 1);

                // Bilinear interpolation...
                u32 u1 = std::floor(u);
                u32 v1 = std::floor(v);
                u32 u2 = std::min(u1 + 1, skybox_image->width() - 1);
                u32 v2 = std::min(v1 + 1, skybox_image->height() - 1);

                f32 s = u - u1;
                f32 t = v - v1;

                glm::vec4 tl = get_pixel(u1, v1);
                glm::vec4 tr = get_pixel(u2, v1);
                glm::vec4 bl = get_pixel(u1, v2);
                glm::vec4 br = get_pixel(u2, v2);

                glm::vec4 color = glm::mix(glm::mix(tl, tr, s), glm::mix(bl, br, s), t);

                // Write pixel value.
                u8* dst = data + (y * face_size + x) * 4;
                dst[0] = color[0] / 255.f;
                dst[1] = color[1] / 255.f;
                dst[2] = color[2] / 255.f;
                dst[3] = color[3] / 255.f;
            }
        }
    }

    // Create a staging buffer to upload our data to.
    u64 image_layer_size = face_size * face_size * 4;
    u64 image_size = image_layer_size * 6;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_mem;

    create_buffer(
        image_size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        staging_buffer, 
        staging_mem
    );

    void* data;
    vkMapMemory(device(), staging_mem, 0, image_size, 0, &data);
    for (u8 i = 0; i < 6; i++) {
        std::memcpy(static_cast<u8*>(data) + image_layer_size * i, face_data[i].data(), image_layer_size);
    }
    vkUnmapMemory(device(), staging_mem);

    // Create the cubemap image.
    create_image_cube(
        face_size, 
        VK_FORMAT_R8G8B8A8_SRGB, 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        m_cubemap_image, 
        m_cubemap_memory
    );

    VkCommandBuffer command_buffer = begin_single_time_commands();

    transition_image_layout(
        command_buffer,
        m_cubemap_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        6
    );

    std::array<VkBufferImageCopy, 6> regions;
    for (u8 i = 0; i < 6; i++) {
        regions[i] = {
            .bufferOffset = image_layer_size * i,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = i,
                .layerCount = 1,
            },
            .imageOffset = { .x = 0, .y = 0, .z = 0 },
            .imageExtent = { .width = face_size, .height = face_size, .depth = 1 }
        };
    }

    vkCmdCopyBufferToImage(
        command_buffer,
        staging_buffer,
        m_cubemap_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        regions.size(),
        regions.data()
    );

    transition_image_layout(
        command_buffer,
        m_cubemap_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        6
    );

    end_single_time_commands(command_buffer);

    // Cleanup staging buffer.
    vkDestroyBuffer(device(), staging_buffer, nullptr);
    vkFreeMemory(device(), staging_mem, nullptr);
}

void Engine::create_cubemap_image_view() {
    VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_cubemap_image,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6,
        }
    };
    vulkan_check_res(
        vkCreateImageView(device(), &view_info, nullptr, &m_cubemap_image_view),
        "failed to create cubemap image view"
    );
}

void Engine::create_cubemap_sampler() {
    VkSamplerCreateInfo sampler_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0,
        .anisotropyEnable = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0,
        .maxLod = 0,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    vulkan_check_res(
        vkCreateSampler(device(), &sampler_info, nullptr, &m_cubemap_sampler),
        "failed to create cubemap sampler"
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

        vkMapMemory(device(), m_camera_ubo_memory[i], 0, sizeof(CameraUBO), 0, &m_camera_ubo_data[i]);
    }
}

void Engine::create_descriptor_pool() {
    constexpr u32 MAX_SETS = 1024;
    
    VkDescriptorPoolSize ubo_pool_size{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_SETS,
    };

    VkDescriptorPoolSize sampler_pool_size{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_SETS,
    };

    const auto pool_sizes = std::to_array({ ubo_pool_size, sampler_pool_size });

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = MAX_SETS,
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

    vulkan_check_res(
        vkAllocateDescriptorSets(device(), &alloc_info, m_descriptor_sets.data()), 
        "failed to allocate descriptor sets"
    );

    // Cube descriptor sets
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = m_camera_ubos[i],
            .offset = 0,
            .range = sizeof(CameraUBO),
        };

        VkDescriptorImageInfo soggy_image_info{
            .sampler = m_texture_sampler,
            .imageView = m_texture_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkDescriptorImageInfo skybox_image_info{
            .sampler = m_cubemap_sampler,
            .imageView = m_cubemap_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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
                .pImageInfo = &soggy_image_info
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptor_sets[i],
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &skybox_image_info
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

void Engine::create_cubemap_buffers() {
    usize vertex_size = sizeof(Vertex) * CUBEMAP_VERTICES.size();

    create_buffer(
        vertex_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_cubemap_vertex_buffer,
        m_cubemap_vertex_buffer_memory
    );

    // Upload vertex data.
    void* vertex_data;
    vkMapMemory(device(), m_cubemap_vertex_buffer_memory, 0, vertex_size, 0, &vertex_data);
    memcpy(vertex_data, CUBEMAP_VERTICES.data(), vertex_size);
    vkUnmapMemory(device(), m_cubemap_vertex_buffer_memory);


    usize index_size = sizeof(u16) * CUBEMAP_INDICES.size();

    create_buffer(
        index_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_cubemap_index_buffer,
        m_cubemap_index_buffer_memory
    );

    // Upload index data.
    void* index_data;
    vkMapMemory(device(), m_cubemap_index_buffer_memory, 0, index_size, 0, &index_data);
    memcpy(index_data, CUBEMAP_INDICES.data(), index_size);
    vkUnmapMemory(device(), m_cubemap_index_buffer_memory);
}

void Engine::create_scene_objects() {
    std::random_device rd;
    std::mt19937 mt{ rd() };
    std::uniform_real_distribution<f32> pos_dist(-5, 5);
    std::uniform_real_distribution<f32> rot_dist(0, 1);

    constexpr size_t NUM_CUBES = 60;

    for (usize i = 0; i < NUM_CUBES; i++) {
        glm::vec3 pos{
            pos_dist(mt),
            pos_dist(mt),
            pos_dist(mt),
        };

        // Generates a uniformly distributed quaternion. Not gonna pretend like I have the slightest idea as to how the hell this works
        // https://stackoverflow.com/a/44031492
        constexpr auto PI = std::numbers::pi_v<f32>;
        f32 u = rot_dist(mt);
        f32 v = rot_dist(mt);
        f32 w = rot_dist(mt);
        glm::quat rot{
            std::sqrtf(1 - u) * sinf(2 * PI * v),
            std::sqrtf(1 - u) * cosf(2 * PI * v),
            std::sqrtf(u) * sinf(2 * PI * w),
            std::sqrtf(u) * cosf(2 * PI * w)
        };

        CubeObject cube{
            .m_pos = pos,
            .m_rot = rot
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
        if ((memory_type_bits & (1 << i)) &&
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

void Engine::create_image_2d(u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_flags, VkImage& image, VkDeviceMemory& mem) {
    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { .width = width, .height = height, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    vulkan_check_res(
        vkCreateImage(device(), &image_info, nullptr, &image),
        "failed to create image"
    );

    // Allocate memory for the image.
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device(), image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = choose_memory_type(mem_requirements.memoryTypeBits, mem_flags),
    };
    vulkan_check_res(
        vkAllocateMemory(device(), &alloc_info, nullptr, &mem),
        "failed to allocate image memory"
    );

    vkBindImageMemory(device(), image, mem, 0);
}

void Engine::create_image_cube(u32 size, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_flags, VkImage& image, VkDeviceMemory& mem) {
    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { .width = size, .height = size, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 6,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    vulkan_check_res(
        vkCreateImage(device(), &image_info, nullptr, &image),
        "failed to create image"
    );

    // Allocate memory for the image.
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device(), image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = choose_memory_type(mem_requirements.memoryTypeBits, mem_flags),
    };
    vulkan_check_res(
        vkAllocateMemory(device(), &alloc_info, nullptr, &mem),
        "failed to allocate image memory"
    );

    vkBindImageMemory(device(), image, mem, 0);
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
    // (lets us start single time tasks while others are already in flight and wait on all of them in one go... or just one since queues execute command buffers sequentially)
    vkQueueSubmit(graphics_queue(), 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue());

    vkFreeCommandBuffers(device(), m_transient_command_pool, 1, &command_buffer);
}

void Engine::transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkImageLayout src_layout, VkImageLayout dst_layout, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask, VkImageAspectFlags aspect_mask, u32 layer_count) {
    // TODO: Use vkCmdPipelineBarrier2 provided by Vulkan 1.3
    VkImageMemoryBarrier memory_barrier_1{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = src_layout,
        .newLayout = dst_layout,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect_mask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = layer_count,
        }
    };
    vkCmdPipelineBarrier(
        command_buffer,
        src_stage_mask,
        dst_stage_mask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &memory_barrier_1
    );
}

void Engine::recreate_swapchain() {
    // Wait for the device to be idle before recreating the swapchain
    vkDeviceWaitIdle(device());

    auto last_extent = m_swapchain->extent();

    m_swapchain->reset();

    // Do things that may depend on the surface format here...

    m_swapchain->create(m_window_width, m_window_height);

    auto extent = m_swapchain->extent();

    if (last_extent.width != extent.width || last_extent.height != extent.height) {
        // Size changed.
        
        // TODO: Re-use existing image/memory if possible (eg. the new extent is smaller than the old one)
        vkDestroyImageView(device(), m_depth_image_view, nullptr);
        vkDestroyImage(device(), m_depth_image, nullptr);
        vkFreeMemory(device(), m_depth_image_memory, nullptr);

        create_depth_image();
    }

    // Update the ImGui backend
    ImGui_ImplVulkan_SetMinImageCount(m_swapchain->min_image_count());

    spdlog::info("finished swapchain recreate");
}

void Engine::update() {
    const auto now = std::chrono::steady_clock::now();
    const auto diff = now - m_last_update;
    m_last_update = now;

    const auto time_delta = std::chrono::duration_cast<std::chrono::duration<f32>>(diff).count();

    
    // Update camera position.
    f32 forward = 0;
    f32 right = 0;
    f32 up = 0;

    const auto keyboard_state = SDL_GetKeyboardState(nullptr);
    
    if (keyboard_state[SDL_GetScancodeFromKey(SDLK_W, nullptr)])
        forward += 1;
    if (keyboard_state[SDL_GetScancodeFromKey(SDLK_S, nullptr)])
        forward -= 1;
    if (keyboard_state[SDL_GetScancodeFromKey(SDLK_A, nullptr)])
        right -= 1;
    if (keyboard_state[SDL_GetScancodeFromKey(SDLK_D, nullptr)])
        right += 1;
    if (keyboard_state[SDL_GetScancodeFromKey(SDLK_SPACE, nullptr)])
        up += 1;
    if (keyboard_state[SDL_GetScancodeFromKey(SDLK_LCTRL, nullptr)])
        up -= 1;

    glm::vec3 dir_xz = m_camera.dir_xz();

    glm::vec3 move_dir = {
        dir_xz.x * forward - dir_xz.z * right,
        up,
        dir_xz.z * forward + dir_xz.x * right
    };
    move_dir *= time_delta * 5;

    m_camera.set_pos(m_camera.pos() + move_dir);
    m_camera.update_rot();
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
        auto extent = m_swapchain->extent();

        m_camera.set_aspect_ratio((f32) extent.width / extent.height);
        m_camera.update_matrices();

        CameraUBO camera_ubo{
            .view = m_camera.view_mtx(),
            .proj = m_camera.proj_mtx()
        };

        // Write Camera UBO.
        memcpy(m_camera_ubo_data[m_current_frame], &camera_ubo, sizeof(CameraUBO));

        for (const auto& object : m_scene_objects) {
            auto id = glm::identity<glm::mat4x4>();
            auto rotate = glm::mat4_cast(object.m_rot); // They could not have made this function any less obscure
            auto translate = glm::translate(id, object.m_pos);

            CubeUBO cube_ubo{
                .model = translate * rotate
            };

            // Write Cube UBO.
            memcpy(object.m_ubo_data[m_current_frame], &cube_ubo, sizeof(CubeUBO));
        }
    }

    // Transition the swapchain image to be suitable for rendering.
    transition_image_layout(
        command_buffer,
        m_swapchain->image(m_image_index),
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );

    transition_image_layout(
        command_buffer,
        m_depth_image,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    
    VkClearValue clear_col = { .color = { .float32 = { 0.15, 0.15, 0.15, 1 } } };
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

    {
        VkBuffer vertex_bufs[] = { m_cubemap_vertex_buffer };
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cubemap_pipeline);
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_bufs, offsets);
        vkCmdBindIndexBuffer(command_buffer, m_cubemap_index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cubemap_pipeline_layout, 0, 1, &m_descriptor_sets[m_current_frame], 0, nullptr);
        vkCmdDrawIndexed(command_buffer, (u32) CUBEMAP_INDICES.size(), 1, 0, 0, 0);
    }

    {
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
    }

    vkCmdEndRendering(command_buffer);

    // Render imgui.
    render_imgui(command_buffer);

    // Transition the image back to be suitable for presenting.
    transition_image_layout(
        command_buffer,
        m_swapchain->image(m_image_index),
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
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

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Debug");

    imgui_text("brampling3D ({} {}, {})", SDL_GetPlatform(), ENGINE_SYSTEM_PROCESSOR, SDL_GetCurrentVideoDriver());
    imgui_text("GPU: {}", m_device->device_name());
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
