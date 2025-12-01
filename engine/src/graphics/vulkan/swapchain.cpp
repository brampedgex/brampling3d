#include "swapchain.hpp"

VulkanSwapchain::VulkanSwapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface) :
        m_physical_device(physical_device),
        m_device(device),
        m_surface(surface) {
    choose_surface_format();
}

VulkanSwapchain::~VulkanSwapchain() {
    cleanup();
}

void VulkanSwapchain::choose_surface_format() {
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_count, surface_formats.data());

    VkSurfaceFormatKHR surface_format = surface_formats[0];
    for (const auto& format : surface_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = format;
            break;
        }
    }

    m_surface_format = surface_format;
}

bool VulkanSwapchain::create(u32 window_width, u32 window_height, VkRenderPass render_pass) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &capabilities);

    // Choose the extent. If currentExtent returns UINT32_MAX then we'll get it from the window dimensions passed in.
    VkExtent2D swapchain_extent = capabilities.currentExtent;
    if (swapchain_extent.width == UINT32_MAX) {
        swapchain_extent.width = window_width;
        swapchain_extent.height = window_height;
    }
    m_extent = swapchain_extent;

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_surface,
        .minImageCount = capabilities.minImageCount,
        .imageFormat = m_surface_format.format,
        .imageColorSpace = m_surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE
    };
    
    if (vkCreateSwapchainKHR(m_device, &swapchain_create_info, nullptr, &m_swapchain) != VK_SUCCESS) {
        spdlog::error("Failed to create swapchain");
        return false;
    }

    // Get images.
    u32 image_count;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
    m_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_images.data());

    // Create image views.
    m_image_views.resize(image_count);
    for (usize i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_surface_format.format,
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

        if (vkCreateImageView(m_device, &view_info, nullptr, &m_image_views[i]) != VK_SUCCESS) {
            spdlog::error("failed to create image view {}", i);
            return false;
        }
    }

    // Create framebuffers for the passed in render pass for each image.
    // TODO: There's a bunch of other parameters about the framebuffer creation we'll have to pass in as well...
    // I should probably make the framebuffers externally with some kind of dependency system to recreate them when the swapchain is recreated.
    for (const auto [i, image_view] : m_image_views | std::views::enumerate) {
        VkImageView attachments[] = { image_view };
        VkFramebufferCreateInfo framebuffer_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = m_extent.width,
            .height = m_extent.height,
            .layers = 1
        };

        VkFramebuffer framebuffer;
        if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &framebuffer) != VK_SUCCESS) {
            spdlog::error("failed to create framebuffer {}", i);
            return false;
        }

        m_framebuffers.push_back(framebuffer);
    }

    return true;
}

void VulkanSwapchain::reset() {
    cleanup();

    // Swapchain recreation could've happened because of a surface format change, so we'll re-query the surface format
    // and let the engine recreate the render_pass before calling create() again
    choose_surface_format();
}

VkResult VulkanSwapchain::acquire(VkSemaphore image_available_semaphore, u32& image_index) {
    return vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);
}

VkResult VulkanSwapchain::present(VkQueue present_queue, VkSemaphore wait_semaphore, u32 image_index) {
    VkSemaphore wait_semaphores[] = { wait_semaphore };

    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index
    };

    return vkQueuePresentKHR(present_queue, &present_info);
}

void VulkanSwapchain::cleanup() {
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    for (const auto image_view : m_image_views) {
        vkDestroyImageView(m_device, image_view, nullptr);
    }
    for (const auto framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    m_images.clear();
    m_image_views.clear();
    m_framebuffers.clear();
}
