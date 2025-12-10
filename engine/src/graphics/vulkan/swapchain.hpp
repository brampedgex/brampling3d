#pragma once

#include "../../vulkan.hpp"

class VulkanSwapchain {
public:
    VulkanSwapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface);
    ~VulkanSwapchain();

    /// Creates the swapchain.
    /// window_width and window_height are the dimensions of the window.
    /// render_pass is the render pass used for Framebuffer creation.
    void create(u32 window_width, u32 window_height, VkRenderPass render_pass);

    /// Destroys the swapchain and re-queries the surface format.
    void reset();
        
    VkResult acquire(VkSemaphore image_available_semaphore, u32& image_index);
    VkResult present(VkQueue present_queue, VkSemaphore wait_semaphore, u32 image_index);

public:     // Getters
    /// Returns the surface format. This can be called before swapchain creation.
    const auto& surface_format() const { return m_surface_format; }

    /// Returns the surface extent, i.e. the resolution.
    const auto& extent() const { return m_extent; }

    auto min_image_count() const { return m_min_image_count; }

    usize image_count() const { return m_images.size(); } 

    auto submit_semaphore(usize index) const { return m_submit_semaphores[index]; }
    
    const auto& framebuffers() const { return m_framebuffers; }
    auto framebuffer(usize index) const { return m_framebuffers[index]; }

private:
    void choose_surface_format();
    void cleanup();

private:
    VkPhysicalDevice m_physical_device;
    VkDevice m_device;
    VkSurfaceKHR m_surface;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_image_views;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkSemaphore> m_submit_semaphores;
    
    VkSurfaceFormatKHR m_surface_format;
    VkExtent2D m_extent;
    u32 m_min_image_count;
};
