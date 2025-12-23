#pragma once

#include "camera.hpp"

#include "util/vulkan.hpp"
#include "util/sdl3.hpp"

#include "graphics/vulkan/device.hpp"
#include "graphics/vulkan/swapchain.hpp"

static constexpr auto ENGINE_VULKAN_API_VERSION = VK_API_VERSION_1_3;

class Engine {
public:
    /// Initialize SDL and Vulkan
    void start();

    /// Run the main event loop.
    void run();

private:
    void quit();

    void init_window();

    void init_graphics();

    void init_imgui();

    void init_scene();

    void create_instance();
    void create_window_surface();
    void create_command_pools();
    void create_descriptor_set_layouts();
    void create_graphics_pipeline();
    void create_depth_image();
    void create_texture_image();
    void create_texture_image_view();
    void create_texture_sampler();
    void create_camera_ubos();
    void create_descriptor_pool();
    void create_descriptor_sets();
    void create_vertex_buffer();
    void create_index_buffer();
    void create_scene_objects();
    void create_command_buffers();
    void create_sync_objects();

    u32 choose_memory_type(u32 memory_type_bits, VkMemoryPropertyFlags mem_flags);
    void create_buffer(usize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_flags, VkBuffer& buffer, VkDeviceMemory& mem);

    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer command_buffer);

    void transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkImageLayout src_layout, VkImageLayout dst_layout, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask, VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

    void recreate_swapchain();   

    void update();
    void update_graphics();

    void render_frame();
    void render_imgui(VkCommandBuffer command_buffer);

private:
    auto physical_device() const { return m_device->physical_device(); }
    auto device() const { return m_device->device(); }
    auto graphics_queue() const { return m_device->graphics_queue(); }
    auto present_queue() const { return m_device->present_queue(); }

private:
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    struct CubeObject {
        glm::vec3 m_pos;
        glm::vec3 m_rotate_axis;

        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_ubos;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> m_ubo_memory;
        std::array<void*, MAX_FRAMES_IN_FLIGHT> m_ubo_data;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_descriptor_sets;
    };

    SDL_Window* m_window{};
    u32 m_window_width;
    u32 m_window_height;

    VkInstance m_instance;
    VkSurfaceKHR m_window_surface;
    
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapchain> m_swapchain;

    VkDescriptorSetLayout m_descriptor_set_layout;
    VkDescriptorSetLayout m_scene_object_descriptor_set_layout;

    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_pipeline;

    VkBuffer m_vertex_buffer;
    VkDeviceMemory m_vertex_buffer_memory;

    VkBuffer m_index_buffer;
    VkDeviceMemory m_index_buffer_memory;

    VkImage m_depth_image;
    VkDeviceMemory m_depth_image_memory;
    VkImageView m_depth_image_view;

    VkImage m_texture_image;
    VkDeviceMemory m_texture_image_memory;
    VkImageView m_texture_image_view;
    VkSampler m_texture_sampler;
    
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_camera_ubos;
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> m_camera_ubo_memory;
    std::array<void*, MAX_FRAMES_IN_FLIGHT> m_camera_ubo_data;

    VkDescriptorPool m_descriptor_pool;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_descriptor_sets;

    std::vector<CubeObject> m_scene_objects;

    VkCommandPool m_command_pool;
    VkCommandPool m_transient_command_pool;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_command_buffers;
    
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_image_available_semaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_in_flight_fences;

    usize m_current_frame = 0;
    u32 m_image_index;
    bool m_window_resized = false;
    bool m_need_swapchain_recreate = false;

    Camera m_camera;

    bool m_vsync = true;

    std::chrono::steady_clock::time_point m_last_update;
};
