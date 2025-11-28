#include "sdl3.hpp"

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
    u32 extension_count;
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (!extensions) {
        sdl3_perror("failed to get vulkan instance extensions");
        return 1;
    }

    // ApplicationInfo lets drivers enable application-specific optimizations. So Intel, NVIDIA, and AMD can implement the best optimizations for brampling3D.
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "brampling3D",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "brampling3D",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    // Create a vulkan instance with the required extensions
    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions
    };

    VkInstance instance;
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
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
    u32 device_count;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0) {
        spdlog::error("Failed to enumerate vulkan devices");
        return 1;
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    u32 graphics_family = UINT32_MAX;
    u32 present_family = UINT32_MAX;

    for (const auto& device : devices) {
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
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
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
        spdlog::critical("Failed to find suitable vulkan device");
        return 1;
    }

    spdlog::info("present_family: {}, graphics_family: {}", present_family, graphics_family);

    // Create a logical device.
    f32 queue_priority = 1;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    });

    if (graphics_family != present_family) {
        queue_create_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = present_family,
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

    VkDevice device;
    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
        spdlog::critical("Failed to create vulkan device");
        return 1;
    }

    volkLoadDevice(device);

    VkQueue graphics_queue, present_queue;
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_family, 0, &present_queue);

    // Create swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, surface_formats.data());
    
    VkSurfaceFormatKHR surface_format = surface_formats[0];
    for (const auto& format : surface_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = format;
            break;
        }
    }

    VkExtent2D swapchain_extent = capabilities.currentExtent;
    if (swapchain_extent.width == UINT32_MAX) {
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        swapchain_extent.width = (u32) width;
        swapchain_extent.height = (u32) height;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
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
    
    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain) != VK_SUCCESS) {
        spdlog::critical("Failed to create swapchain");
        return 1;
    }

    // Create views for each swapchain image
    u32 image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    std::vector<VkImage> swapchain_images(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());

    std::vector<VkImageView> swapchain_image_views(image_count);
    for (usize i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
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

        if (vkCreateImageView(device, &view_info, nullptr, &swapchain_image_views[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create image view {}", i);
            return 1;
        }
    }

    // Create a VkRenderPass, which (seemingly?) represents the bound render targets / attachments
    VkAttachmentDescription color_attachment{
        .format = surface_format.format,
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

    VkRenderPass render_pass;
    if (vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass) != VK_SUCCESS) {
        spdlog::critical("failed to create render pass");
        return 1;
    }

    // Compile shaders and create a graphics pipeline.

    std::vector<u8> vert_shader(_binary_shader_vert_spv_start, _binary_shader_vert_spv_end);
    std::vector<u8> frag_shader(_binary_shader_frag_spv_start, _binary_shader_frag_spv_end);

    VkShaderModuleCreateInfo vert_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_shader.size(),
        .pCode = reinterpret_cast<const u32*>(vert_shader.data())
    };

    VkShaderModule vert_shader_module;
    if (vkCreateShaderModule(device, &vert_module_info, nullptr, &vert_shader_module) != VK_SUCCESS) {
        spdlog::critical("failed to create vertex shader");
        return 1;
    }

    VkShaderModuleCreateInfo frag_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_shader.size(),
        .pCode = reinterpret_cast<const u32*>(frag_shader.data())
    };

    VkShaderModule frag_shader_module;
    if (vkCreateShaderModule(device, &frag_module_info, nullptr, &frag_shader_module) != VK_SUCCESS) {
        spdlog::critical("failed to create fragment shader");
        return 1;
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

    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = (float) swapchain_extent.width,
        .height = (float) swapchain_extent.height,
        .minDepth = 0,
        .maxDepth = 0
    };

    VkRect2D scissor{
        .offset = { 0, 0 },
        .extent = swapchain_extent
    };

    VkPipelineViewportStateCreateInfo viewport_state{
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

    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    VkPipelineLayout pipeline_layout;
    if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        spdlog::critical("failed to create pipeline layout");
        return 1;
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
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0
    };

    VkPipeline graphics_pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
        spdlog::critical("failed to create graphics pipeline");
        return 1;
    }

    vkDestroyShaderModule(device, frag_shader_module, nullptr);
    vkDestroyShaderModule(device, vert_shader_module, nullptr);

    // Create frame buffers attached to each swapchain image
    std::vector<VkFramebuffer> swapchain_framebuffers(image_count);
    for (usize i = 0; i < image_count; i++) {
        VkImageView attachments[] = {swapchain_image_views[i]};
        VkFramebufferCreateInfo framebuffer_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchain_extent.width,
            .height = swapchain_extent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swapchain_framebuffers[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create framebuffer {}", i);
            return 1;
        }
    }

    // Create a grapics command pool.
    VkCommandPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_family,
    };

    VkCommandPool command_pool;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
        spdlog::critical("failed to create command pool");
        return 1;
    }

    // Create the vertex buffer.
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * VERTICES.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    if (vkCreateBuffer(device, &buffer_info, nullptr, &vertex_buffer) != VK_SUCCESS) {
        spdlog::critical("failed to create vertex buffer");
        return 1;
    }

    // Allocate memory and bind it to the buffer
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, vertex_buffer, &mem_requirements);

    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

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
        spdlog::critical("failed to find suitable memory type for vertex buffer");
        return 1;
    }

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index
    };

    if (vkAllocateMemory(device, &alloc_info, nullptr, &vertex_buffer_memory) != VK_SUCCESS) {
        spdlog::critical("failed to allocate vertex buffer memory");
        return 1;
    }

    vkBindBufferMemory(device, vertex_buffer, vertex_buffer_memory, 0);

    // Upload vertex data.
    void* data;
    vkMapMemory(device, vertex_buffer_memory, 0, buffer_info.size, 0, &data);
    memcpy(data, VERTICES.data(), static_cast<usize>(buffer_info.size));
    vkUnmapMemory(device, vertex_buffer_memory);

    // Allocate a command buffer for each swapchain image.
    std::vector<VkCommandBuffer> command_buffers(image_count);
    VkCommandBufferAllocateInfo alloc_info_cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = image_count
    };

    if (vkAllocateCommandBuffers(device, &alloc_info_cmd, command_buffers.data()) != VK_SUCCESS) {
        spdlog::critical("failed to allocate command buffers");
        return 1;
    }

    spdlog::info("recording {} command buffers", image_count);
    for (usize i = 0; i < image_count; i++) {
        VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };

        if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
            spdlog::critical("failed to begin recording command buffer {}", i);
            return 1;
        }

        VkClearValue clear_col = {{{0.2, 0.2, 0.2, 1}}};

        // Begin the render pass by clearing the image.
        VkRenderPassBeginInfo rp_begin_info{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = swapchain_framebuffers[i],
            .renderArea = {
                .offset = { 0, 0 },
                .extent = swapchain_extent
            },
            .clearValueCount = 1,
            .pClearValues = &clear_col
        };

        // Then draw.
        vkCmdBeginRenderPass(command_buffers[i], &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
        VkBuffer vertex_bufs[] = {vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_bufs, offsets);
        vkCmdDraw(command_buffers[i], (u32) VERTICES.size(), 1, 0, 0);
        vkCmdEndRenderPass(command_buffers[i]);

        if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
            spdlog::critical("failed to end command buffer {}", i);
            return 1;
        }
    }

    std::vector<VkSemaphore> image_available_semaphores(image_count);
    std::vector<VkSemaphore> render_finished_semaphores(image_count);
    std::vector<VkFence> in_flight_fences(image_count);
    VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (usize i = 0; i < image_count; i++) {
        if (vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create image available semaphore");
            return 1;
        }

        if (vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS) {
            spdlog::critical("failed to create render finished semaphore");
            return 1;
        }

        if (vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS) {
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

    u32 current_frame = 0;

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

        VkSemaphore image_available_semaphore = image_available_semaphores[current_frame];
        VkFence in_flight_fence = in_flight_fences[current_frame];

        vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &in_flight_fence);

        u32 image_index;
        VkResult acquire_result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);
        if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
            spdlog::error("failed to acquire next image: {}", (int)acquire_result);
            quit = true;
            continue;
        }

        VkSemaphore wait_semaphores[] = {image_available_semaphore};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signal_semaphores[] = {render_finished_semaphores[image_index]};

        // Submit the command buffer that we already recorded.
        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = wait_semaphores,
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffers[image_index],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signal_semaphores
        };

        VkResult submit_result = vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fence);
        if (submit_result != VK_SUCCESS) {
            spdlog::error("failed to submit draw command buffer for {}: {}", image_index, (int)submit_result);
            quit = true;
            continue;
        }

        VkPresentInfoKHR present_info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signal_semaphores,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index
        };

        VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);
        if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
            spdlog::error("failed to present image {}: {}", image_index, (int)present_result);
            quit = true;
            continue;
        }

        current_frame = (current_frame + 1) % image_count;

        // 60 fps limit so as to not nuke the CPU (TODO: vsync?)
        std::this_thread::sleep_for(16ms);
    }

    spdlog::info("quitting");

    return 0;
}
