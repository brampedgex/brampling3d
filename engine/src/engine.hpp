#pragma once

#include "camera.hpp"

#include "util/vulkan.hpp"
#include "util/sdl3.hpp"

#include "graphics/vulkan/device.hpp"
#include "graphics/vulkan/swapchain.hpp"
#include "graphics/vulkan/memory.hpp"
#include "graphics/vulkan/buffer.hpp"

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
    void create_cube_pipeline();
    void create_cubemap_pipeline();
    void create_ground_pipeline();
    void create_depth_image();

    void create_cube_texture_image();
    void create_cube_texture_image_view();
    void create_cube_texture_sampler();

    void create_cubemap_image();
    void create_cubemap_image_view();
    void create_cubemap_sampler();

    void create_ground_image();
    void create_ground_image_view();
    void create_ground_sampler();

    void create_camera_ubos();

    void create_descriptor_pool();
    void create_descriptor_sets();

    void create_cube_buffers();
    void create_cubemap_buffers();
    void create_ground_buffers();
    void create_scene_objects();
    void create_command_buffers();
    void create_sync_objects();


    void create_image_2d(u32 width, u32 height, u32 mip_levels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_flags, VkImage& image, VkDeviceMemory& mem);
    void create_image_cube(u32 size, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_flags, VkImage& image, VkDeviceMemory& mem);

    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer command_buffer);

    // Generates mips and transitions them to the given layout. Assumes the image is in TRANSFER_DST_OPTIMAL layout.
    void generate_mips(VkCommandBuffer command_buffer, VkImage image, u32 width, u32 height, u32 mip_levels, VkAccessFlags dst_access_mask, VkImageLayout dst_layout, VkPipelineStageFlags dst_stage_mask);

    void transition_image_layout(
        VkCommandBuffer command_buffer, 
        VkImage image, 
        VkAccessFlags src_access_mask, 
        VkAccessFlags dst_access_mask, 
        VkImageLayout src_layout, 
        VkImageLayout dst_layout, 
        VkPipelineStageFlags src_stage_mask, 
        VkPipelineStageFlags dst_stage_mask, 
        u32 mip_levels, 
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, 
        u32 layer_count = 1
    );

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
        glm::quat m_rot;

        std::array<vke::Buffer, MAX_FRAMES_IN_FLIGHT> m_ubos;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_descriptor_sets;
    };

    SDL_Window* m_window{};
    u32 m_window_width;
    u32 m_window_height;

    VkInstance m_instance;
    VkSurfaceKHR m_window_surface;
    
    std::unique_ptr<vke::Device> m_device;
    std::unique_ptr<vke::Swapchain> m_swapchain;

    VkDescriptorSetLayout m_descriptor_set_layout;
    VkDescriptorSetLayout m_scene_object_descriptor_set_layout;

    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_pipeline;

    VkPipelineLayout m_cubemap_pipeline_layout;
    VkPipeline m_cubemap_pipeline;

    VkPipelineLayout m_ground_pipeline_layout;
    VkPipeline m_ground_pipeline;

    vke::Buffer m_cube_vertex_buffer;
    vke::Buffer m_cube_index_buffer;
    vke::Buffer m_ground_vertex_buffer;
    vke::Buffer m_ground_index_buffer;
    vke::Buffer m_cubemap_vertex_buffer;
    vke::Buffer m_cubemap_index_buffer;

    VkImage m_depth_image;
    VkDeviceMemory m_depth_image_memory;
    VkImageView m_depth_image_view;

    // Soggy cat texture
    VkImage m_texture_image;
    VkDeviceMemory m_texture_image_memory;
    VkImageView m_texture_image_view;
    VkSampler m_texture_sampler;
    u32 m_texture_mip_levels;

    // Cubemap image
    VkImage m_cubemap_image;
    VkDeviceMemory m_cubemap_memory;
    VkImageView m_cubemap_image_view;
    VkSampler m_cubemap_sampler;

    // Ground image
    VkImage m_ground_image;
    VkDeviceMemory m_ground_image_memory;
    VkImageView m_ground_image_view;
    VkSampler m_ground_sampler;
    u32 m_ground_mip_levels;
    
    std::array<vke::Buffer, MAX_FRAMES_IN_FLIGHT> m_camera_ubos;

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
    bool m_need_swapchain_recreate = false;
    bool m_grab_mouse = true;

    Camera m_camera;

    bool m_vsync = true;

    std::chrono::steady_clock::time_point m_last_update;
};
