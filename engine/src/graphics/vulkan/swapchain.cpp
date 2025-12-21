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

VkPresentModeKHR VulkanSwapchain::choose_present_mode() {
    // VK_PRESENT_MODE_FIFO_KHR limits frame throughput to refresh rate (ie. vsync), which reduces power consumption.
    if (m_vsync) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, present_modes.data());

    // VK_PRESENT_MODE_IMMEDIATE_KHR allows tearing.
    // There's also VK_PRESENT_MODE_MAILBOX_KHR which can do triple buffering with dropped frames,
    // but it seems to not have any effect on Windows :(
    const auto preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    for (const auto present_mode : present_modes) {
        if (present_mode == preferred_present_mode) {
            return present_mode;
        }
    }

    // VK_PRESENT_MODE_FIFO_KHR is always supported.
    return VK_PRESENT_MODE_FIFO_KHR;
}

void VulkanSwapchain::create(u32 window_width, u32 window_height) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &capabilities);

    // Choose the extent. If currentExtent returns UINT32_MAX then we'll get it from the window dimensions passed in.
    VkExtent2D swapchain_extent = capabilities.currentExtent;
    if (swapchain_extent.width == UINT32_MAX) {
        swapchain_extent.width = window_width;
        swapchain_extent.height = window_height;
    }
    m_extent = swapchain_extent;
    m_min_image_count = capabilities.minImageCount;

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_surface,
        .minImageCount = m_min_image_count,
        .imageFormat = m_surface_format.format,
        .imageColorSpace = m_surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = choose_present_mode(), 
        .clipped = VK_TRUE
    };
    vulkan_check_res(
        vkCreateSwapchainKHR(m_device, &swapchain_create_info, nullptr, &m_swapchain),
        "failed to create swapchain"
    );

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

        vulkan_check_res(
            vkCreateImageView(m_device, &view_info, nullptr, &m_image_views[i]),
            "failed to create image view {}", i
        );
    }

    // Submit semaphores are managed in this class, because they need to be indexed by the image index instead of the current frame.
    // https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
    m_submit_semaphores.resize(image_count);

    VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    for (usize i = 0; i < image_count; i++) {
        vulkan_check_res(
            vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_submit_semaphores[i]),
            "failed to create submit semaphore {}", i
        );
    }
}

void VulkanSwapchain::reset() {
    cleanup();

    // Swapchain recreation could've happened because of a surface format change (eg. toggling monitor HDR), so we'll re-query the surface format
    choose_surface_format();
}

VkResult VulkanSwapchain::acquire(VkSemaphore image_available_semaphore, u32& image_index) {
    return vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);
}

VkResult VulkanSwapchain::present(VkQueue present_queue, std::span<VkSemaphore> wait_semaphores, u32 image_index) {
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = (u32) wait_semaphores.size(),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index
    };

    return vkQueuePresentKHR(present_queue, &present_info);
}

void VulkanSwapchain::cleanup() {
    if (m_swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    for (const auto image_view : m_image_views) {
        vkDestroyImageView(m_device, image_view, nullptr);
    }
    for (const auto semaphore : m_submit_semaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }

    m_images.clear();
    m_image_views.clear();
    m_submit_semaphores.clear();
}
