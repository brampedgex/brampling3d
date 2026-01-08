#pragma once

#include "../../util/vulkan.hpp"

#include "device.hpp"

namespace vke {

class Buffer {
public: // Constructor/destructor/assignment
    Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& buf) noexcept : 
        m_device(buf.m_device), 
        m_buffer(buf.m_buffer), 
        m_memory(buf.m_memory),
        m_data(buf.m_data) {
        buf.m_buffer = VK_NULL_HANDLE;
        buf.m_memory = VK_NULL_HANDLE;
        buf.m_data = nullptr;
    }

    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& buf) {
        if (this == &buf)
            return *this;

        cleanup();

        m_device = buf.m_device;
        m_buffer = buf.m_buffer;
        m_memory = buf.m_memory;
        m_data = buf.m_data;

        buf.m_buffer = VK_NULL_HANDLE;
        buf.m_memory = VK_NULL_HANDLE;
        buf.m_data = nullptr;

        return *this;
    }

    ~Buffer();

    void cleanup();

public: // Getters
    [[nodiscard]] auto buffer() const { return m_buffer; }
    [[nodiscard]] auto memory() const { return m_memory; }

    template <class T = void*>
    [[nodiscard]] auto data() const { return static_cast<T>(m_data); }

public: // Static methods
    static Buffer create(Device& device, usize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_flags);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    void* m_data = nullptr;
};

}
