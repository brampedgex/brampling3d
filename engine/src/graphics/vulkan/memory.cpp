#include "memory.hpp"


namespace vke {

u32 choose_memory_type(VkPhysicalDevice physical_device, u32 memory_type_bits, VkMemoryPropertyFlags mem_flags) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    u32 memory_type_index = UINT32_MAX;
    for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((memory_type_bits & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & mem_flags)) {
            memory_type_index = i;
            break;
        }
    }
    if (memory_type_index == UINT32_MAX) {
        throw std::runtime_error("failed to find suitable memory type for buffer");
    }

    return memory_type_index;
}

}
