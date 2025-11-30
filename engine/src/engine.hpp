#pragma once

#include "vulkan.hpp"
#include "sdl3.hpp"

class Engine {
public:
    bool start();
    void run();

private:
    void quit();

    bool init_window();

    bool init_graphics();
    bool create_vk_instance();
    bool create_window_surface();
    bool find_physical_device();
    bool create_device();
    bool create_swapchain();
    bool create_render_pass();
    bool create_framebuffers();
    bool create_graphics_pipeline();
    bool create_vertex_buffer();
    bool create_command_buffers();
    bool create_sync_objects();

    void recreate_swapchain();   

    void update_graphics();

    void render_frame();

private:
    SDL_Window* m_window{};

    VkInstance m_vk_instance;
    VkSurfaceKHR m_window_surface;
    
    VkPhysicalDevice m_physical_device;
    u32 m_graphics_family;
    u32 m_present_family;

    VkDevice m_device;
    VkQueue m_graphics_queue, m_present_queue;
    VkCommandPool m_command_pool;

    VkSwapchainKHR m_swapchain;
    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;
    VkSurfaceFormatKHR m_surface_format;
    VkExtent2D m_swapchain_extent;

    VkRenderPass m_render_pass;
    VkPipeline m_graphics_pipeline;
    std::vector<VkFramebuffer> m_swapchain_framebuffers;

    VkBuffer m_vertex_buffer;
    VkDeviceMemory m_vertex_buffer_memory;

    std::vector<VkCommandBuffer> m_command_buffers;
    
    std::vector<VkSemaphore> m_image_available_semaphores;
    std::vector<VkSemaphore> m_render_finished_semaphores;
    std::vector<VkFence> m_in_flight_fences;

    usize m_current_frame = 0;
    bool m_window_resized = false;
    bool m_need_swapchain_recreate = false;
};
