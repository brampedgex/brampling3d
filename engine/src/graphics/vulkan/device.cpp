#include "device.hpp"

VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface) : m_instance(instance) {
    choose_physical_device(surface);
    create_device();
}

void VulkanDevice::choose_physical_device(VkSurfaceKHR surface) {
    u32 device_count;
    vulkan_check_res(
        vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr),
        "Failed to enumerate vulkan devices"
    );
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    u32 graphics_family = UINT32_MAX;
    u32 present_family = UINT32_MAX;

    for (const auto device : devices) {
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

        VkPhysicalDeviceFeatures supported_features;
        vkGetPhysicalDeviceFeatures(device, &supported_features);

        // fuh kinda 1998 ass graphics card do you got with no sampler anisotropy 
        if (graphics_found && present_found && supported_features.samplerAnisotropy) {
            physical_device = device;
            break;
        }
    }

    if (physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable vulkan device");
    }

    m_physical_device = physical_device;
    m_graphics_family = graphics_family;
    m_present_family = present_family;

    vkGetPhysicalDeviceProperties(m_physical_device, &m_physical_device_properties);
}

void VulkanDevice::create_device() {
    f32 queue_priority = 1;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    });

    if (m_graphics_family != m_present_family) {
        queue_create_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = m_present_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        });
    }

    VkPhysicalDeviceFeatures device_features{
        .samplerAnisotropy = VK_TRUE
    };

    std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    u32 available_extension_count;
    vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &available_extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(available_extension_count);
    vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &available_extension_count, available_extensions.data());

    // VK_KHR_portability_subset must be enabled if supported by the device.
    for (const auto& extension : available_extensions) {
        if (extension.extensionName == std::string_view{ VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME })
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }

    // Enable dynamic rendering.
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = true
    };

    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dynamic_rendering_features,
        .queueCreateInfoCount = (u32) queue_create_infos.size(),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = (u32) device_extensions.size(),
        .ppEnabledExtensionNames = device_extensions.data(),
        .pEnabledFeatures = &device_features,
    };

    vulkan_check_res(
        vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device),
        "failed to create vulkan device"
    );

    vkGetDeviceQueue(m_device, m_graphics_family, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, m_present_family, 0, &m_present_queue);
}
