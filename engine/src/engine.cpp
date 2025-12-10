#include "engine.hpp"

#include <engine-generated/engine_assets.hpp>

#include "imgui.hpp"
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include "stb.hpp"

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
    {{  0.5, -0.5,  0.5 }, { 0, 1, 1 }, { 1, 1 }}
});

constexpr auto INDICES = std::to_array<u16>({
     0,  1,  2,  1,  3,  2,
     4,  5,  6,  5,  7,  6,
     8,  9, 10,  9, 11, 10,
    12, 13, 14, 13, 15, 14,
    16, 17, 18, 17, 19, 18,
    20, 21, 22, 21, 23, 22,
});

struct UniformBufferObject {
    glm::mat4x4 model;
    glm::mat4x4 view;
    glm::mat4x4 proj;
};

#ifdef DEBUG_BUILD
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#else
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#endif

bool Engine::start() {
    if (!sdl3_init())
        return false;

    if (!init_window())
        return false;

    if (!init_graphics())
        return false;

    init_imgui();

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

bool Engine::init_window() {
    constexpr u32 DEFAULT_WIDTH = 960;
    constexpr u32 DEFAULT_HEIGHT = 640;

    // Hide the window until we are done initializing GPU resources.
    // Maybe in the future we want to show some kind of splash screen when the loading process takes longer,
    // but for now this is fine.
    if ((m_window = SDL_CreateWindow("brampling3D", DEFAULT_WIDTH, DEFAULT_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)) == nullptr) {
        sdl3_perror("Failed to create window");
        return false;
    }

    int width, height;
    SDL_GetWindowSize(m_window, &width, &height);
    m_window_width = (u32) width;
    m_window_height = (u32) height;

    SDL_SetWindowResizable(m_window, true);
    SDL_SetWindowMinimumSize(m_window, 640, 480);

    spdlog::info("Window initialized");
    return true;
}

void Engine::quit() {
    spdlog::info("quitting");

    // Destroy Vulkan resources before destroying the window, which will rug-pull our resources and make the destructors explode instead.
    vkDeviceWaitIdle(device());
    m_swapchain.reset();

    SDL_Quit();
}

bool Engine::init_graphics() {
    if (!create_vk_instance())
        return false;

    if (!create_window_surface())
        return false;
    
    m_device = std::make_unique<VulkanDevice>(m_vk_instance, m_window_surface);

    // create_render_pass() depends on the surface format, which is chosen by VulkanSwapchain, before creating the actual swapchain.
    m_swapchain = std::make_unique<VulkanSwapchain>(physical_device(), device(), m_window_surface);

    if (!create_render_pass())
        return false;

    if (!m_swapchain->create(m_window_width, m_window_height, m_render_pass))
        return false;

    create_command_pools();

    create_texture_image();
    create_texture_image_view();
    create_texture_sampler();

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
    
    // TODO: Global constant for api version so they dont get out of sync
    ImGui_ImplVulkan_InitInfo init_info{
        .ApiVersion = ENGINE_VULKAN_API_VERSION,
        .Instance = m_vk_instance,
        .PhysicalDevice = physical_device(),
        .Device = device(),
        .QueueFamily = m_device->graphics_family(),
        .Queue = graphics_queue(),
        .DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
        .MinImageCount = m_swapchain->min_image_count(),
        .ImageCount = (u32) m_swapchain->image_count(),
        .PipelineInfoMain = {
            .RenderPass = m_render_pass,
            .Subpass = 0,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT
        }
    };
    ImGui_ImplVulkan_Init(&init_info);
}

bool Engine::create_vk_instance() {
    // Get instance extensions needed for vkCreateInstance
    u32 extension_count;
    auto* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (!extensions) {
        sdl3_perror("Failed to get vulkan instance extensions");
        return false;
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
    // So Intel, NVIDIA, and AMD can implement the best optimizations for brampling3D
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
        vkCreateInstance(&create_info, nullptr, &m_vk_instance),
        "Failed to create vulkan instance"
    );

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

    vulkan_check_res(
        vkCreateRenderPass(device(), &render_pass_info, nullptr, &m_render_pass),
        "failed to create render pass"
    );

    return true;
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

bool Engine::create_descriptor_set_layout() {
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

    return true;
}

bool Engine::create_graphics_pipeline() {
    std::vector<u8> vert_shader(_binary_shaders_color3d_vertex_spv_start, _binary_shaders_color3d_vertex_spv_end);
    std::vector<u8> frag_shader(_binary_shaders_color3d_fragment_spv_start, _binary_shaders_color3d_fragment_spv_end);

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

    vulkan_check_res(
        vkCreatePipelineLayout(device(), &pipeline_layout_info, nullptr, &m_pipeline_layout),
        "failed to create pipeline layout"
    );

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

    vulkan_check_res(
        vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline),
        "failed to create graphics pipeline"
    );

    vkDestroyShaderModule(device(), frag_shader_module, nullptr);
    vkDestroyShaderModule(device(), vert_shader_module, nullptr);
    return true;
}

bool Engine::create_texture_image() {
    std::span<const u8> soggy_data{
        (const u8*) _binary_images_soggy_png_start,
        (const u8*) _binary_images_soggy_png_end,
    };

    auto image = stb::Image::from_bytes(soggy_data, 4);
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

    return true;
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

bool Engine::create_uniform_buffers() {
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        create_buffer(
            sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_uniform_buffers[i],
            m_uniform_buffer_memory[i]
        );
    }

    return true;
}

bool Engine::create_descriptor_pool() {
    VkDescriptorPoolSize ubo_pool_size{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };

    VkDescriptorPoolSize sampler_pool_size{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT     // TODO does it need to be this and not one or is this a bug in the tutorial?
    };

    const auto pool_sizes = std::to_array({ ubo_pool_size, sampler_pool_size });

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = pool_sizes.size(),
        .pPoolSizes = pool_sizes.data()
    };
    VkResult res = vkCreateDescriptorPool(device(), &pool_info, nullptr, &m_descriptor_pool);
    vulkan_check_res(res, "failed to create descriptor pool");

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

    VkResult res = vkAllocateDescriptorSets(device(), &alloc_info, m_descriptor_sets.data());
    vulkan_check_res(res, "failed to allocate descriptor sets");

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = m_uniform_buffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject),
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
    
    return true;
}

bool Engine::create_vertex_buffer() {
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

    return true;
}

bool Engine::create_index_buffer() {
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

    vulkan_check_res(
        vkAllocateCommandBuffers(device(), &alloc_info_cmd, m_command_buffers.data()),
        "failed to allocate command buffers"
    );

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
        vulkan_check_res(
            vkCreateSemaphore(device(), &semaphore_info, nullptr, &m_image_available_semaphores[i]),
            "failed to create image available semaphore {}", i
        );

        vulkan_check_res(
            vkCreateFence(device(), &fence_info, nullptr, &m_in_flight_fences[i]),
            "failed to create in-flight fence {}", i
        );
    }

    return true;
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
    vkQueueSubmit(graphics_queue(), 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue());

    vkFreeCommandBuffers(device(), m_transient_command_pool, 1, &command_buffer);
}

void Engine::recreate_swapchain() {
    // Wait for the device to be idle before recreating the swapchain
    vkDeviceWaitIdle(device());

    m_swapchain->reset();
    // TODO: recreate render pass if the surface format changed
    // TODO: handle errors. just nuke everything if resize fails ig
    m_swapchain->create(m_window_width, m_window_height, m_render_pass);

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
        ubo.view = glm::lookAt(glm::vec3(6, 5, 6), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        ubo.proj = glm::perspective(glm::radians(45.f), aspect, 0.1f, 100.f);
        ubo.proj[1][1] *= -1; // vulkan coordinate system bullshit

        auto mem = m_uniform_buffer_memory[m_current_frame];

        void* data;
        vkMapMemory(device(), mem, 0, sizeof(UniformBufferObject), 0, &data);
        memcpy(data, &ubo, sizeof(UniformBufferObject));
        vkUnmapMemory(device(), mem);
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

    // Render imgui in the same pass.
    render_imgui(command_buffer);

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

    VkResult submit_result = vkQueueSubmit(graphics_queue(), 1, &submit_info, in_flight_fence);
    vulkan_check_res(submit_result, "failed to submit draw command buffer for {}", image_index);

    VkResult present_result = m_swapchain->present(present_queue(), signal_semaphores[0], image_index);

    if (present_result == VK_SUBOPTIMAL_KHR || present_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Recreate swapchain next frame. We usually get this right before SDL sends a resize event anyway
        m_need_swapchain_recreate = true;
    } else if (present_result != VK_SUCCESS) {
        vulkan_check_res(present_result, "failed to present image {}", image_index);
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::render_imgui(VkCommandBuffer command_buffer) {
    // TODO: A dedicated render pass for imgui.
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Debug");

    imgui_text("Device: {}", m_device->device_name());
    auto& io = ImGui::GetIO();
    imgui_text("Frame time: {:.3f} ms ({:.1f} FPS)", 1000.0 / io.Framerate, io.Framerate);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}
