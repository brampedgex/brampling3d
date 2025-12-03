#pragma once

#include "vulkan.hpp"
#include "sdl3.hpp"
#include "graphics/vulkan/swapchain.hpp"

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

    bool create_vk_instance();
    bool create_window_surface();
    bool find_physical_device();
    bool create_device();
    bool create_render_pass();
    bool create_swapchain();
    bool create_descriptor_set_layout();
    bool create_graphics_pipeline();
    bool create_uniform_buffers();
    bool create_descriptor_pool();
    bool create_descriptor_sets();
    bool create_vertex_buffer();
    bool create_index_buffer();
    bool create_command_buffers();
    bool create_sync_objects();

    void recreate_swapchain();   

    void update_graphics();

    void render_frame();

private:
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    SDL_Window* m_window{};

    VkInstance m_vk_instance;
    VkSurfaceKHR m_window_surface;
    
    VkPhysicalDevice m_physical_device;
    u32 m_graphics_family;
    u32 m_present_family;

    VkDevice m_device;
    VkQueue m_graphics_queue, m_present_queue;
    VkCommandPool m_command_pool;

    std::unique_ptr<VulkanSwapchain> m_swapchain;

    VkDescriptorSetLayout m_descriptor_set_layout;

    VkRenderPass m_render_pass;
    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_pipeline;

    VkBuffer m_vertex_buffer;
    VkDeviceMemory m_vertex_buffer_memory;

    VkBuffer m_index_buffer;
    VkDeviceMemory m_index_buffer_memory;
    
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_uniform_buffers;
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> m_uniform_buffer_memory;

    VkDescriptorPool m_descriptor_pool;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_descriptor_sets;

    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_command_buffers;
    
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_image_available_semaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_in_flight_fences;

    usize m_current_frame = 0;
    bool m_window_resized = false;
    bool m_need_swapchain_recreate = false;
};
