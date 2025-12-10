#pragma once

#include "vulkan.hpp"
#include "sdl3.hpp"

#include "graphics/vulkan/device.hpp"
#include "graphics/vulkan/swapchain.hpp"

static constexpr auto ENGINE_VULKAN_API_VERSION = VK_API_VERSION_1_3;

class Engine {
public:
    /// Initialize SDL and Vulkan
    bool start();

    /// Run the main event loop.
    void run();

private:
    void quit();

    bool init_window();

    bool init_graphics();

    void init_imgui();

    bool create_vk_instance();
    bool create_window_surface();
    bool create_render_pass();
    void create_command_pools();
    bool create_descriptor_set_layout();
    bool create_graphics_pipeline();
    bool create_texture_image();
    void create_texture_image_view();
    void create_texture_sampler();
    bool create_uniform_buffers();
    bool create_descriptor_pool();
    bool create_descriptor_sets();
    bool create_vertex_buffer();
    bool create_index_buffer();
    bool create_command_buffers();
    bool create_sync_objects();

    u32 choose_memory_type(u32 memory_type_bits, VkMemoryPropertyFlags mem_flags);
    void create_buffer(usize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_flags, VkBuffer& buffer, VkDeviceMemory& mem);

    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer command_buffer);

    void recreate_swapchain();   

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

    SDL_Window* m_window{};
    u32 m_window_width;
    u32 m_window_height;

    VkInstance m_vk_instance;
    VkSurfaceKHR m_window_surface;
    
    std::unique_ptr<VulkanDevice> m_device;

    std::unique_ptr<VulkanSwapchain> m_swapchain;

    VkDescriptorSetLayout m_descriptor_set_layout;

    VkRenderPass m_render_pass;
    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_pipeline;

    VkBuffer m_vertex_buffer;
    VkDeviceMemory m_vertex_buffer_memory;

    VkBuffer m_index_buffer;
    VkDeviceMemory m_index_buffer_memory;

    VkImage m_texture_image;
    VkDeviceMemory m_texture_image_memory;
    VkImageView m_texture_image_view;
    VkSampler m_texture_sampler;
    
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_uniform_buffers;
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> m_uniform_buffer_memory;

    VkDescriptorPool m_descriptor_pool;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_descriptor_sets;

    VkCommandPool m_command_pool;
    VkCommandPool m_transient_command_pool;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_command_buffers;
    
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_image_available_semaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_in_flight_fences;

    usize m_current_frame = 0;
    bool m_window_resized = false;
    bool m_need_swapchain_recreate = false;
};
