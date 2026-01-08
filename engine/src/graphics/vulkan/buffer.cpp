#include "buffer.hpp"
#include "memory.hpp"

namespace vke {

Buffer::~Buffer() {
    cleanup();
}

void Buffer::cleanup() {
    if (m_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_buffer, nullptr);
    if (m_data)
        vkUnmapMemory(m_device, m_memory);
    if (m_memory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_memory, nullptr);

    m_buffer = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_data = nullptr;
}

Buffer Buffer::create(Device& device, usize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_flags) {
    vke::Buffer buffer;
    buffer.m_device = device.device();

    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    vulkan_check_res(
        vkCreateBuffer(device.device(), &buffer_info, nullptr, &buffer.m_buffer),
        "failed to create buffer"
    );

    // Find suitable memory type.
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device.device(), buffer.buffer(), &mem_requirements);

    u32 memory_type_index = choose_memory_type(device.physical_device(), mem_requirements.memoryTypeBits, mem_flags);

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index
    };

    vulkan_check_res(
        vkAllocateMemory(device.device(), &alloc_info, nullptr, &buffer.m_memory),
        "failed to allocate buffer memory"
    );

    vkBindBufferMemory(device.device(), buffer.buffer(), buffer.memory(), 0);

    if (mem_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        vkMapMemory(device.device(), buffer.memory(), 0, size, 0, &buffer.m_data);

    return buffer;
}

}
