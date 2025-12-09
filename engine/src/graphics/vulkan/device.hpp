#include "../../vulkan.hpp"

/// Manages the VkDevice and associated queues
class VulkanDevice {
public:
    VulkanDevice(VkInstance instance, VkSurfaceKHR surface);

public: // Getters
    [[nodiscard]] auto physical_device() const { return m_physical_device; }
    [[nodiscard]] auto device() const { return m_device; }
    [[nodiscard]] auto graphics_queue() const { return m_graphics_queue; }
    [[nodiscard]] auto present_queue() const { return m_present_queue; }

    [[nodiscard]] const auto& physical_device_properties() const { return m_physical_device_properties; }
    [[nodiscard]] std::string_view device_name() const {
        return m_physical_device_properties.deviceName;
    }

    [[nodiscard]] u32 graphics_family() const { return m_graphics_family; }
    [[nodiscard]] u32 present_family() const { return m_present_family; }

private:
    void choose_physical_device(VkSurfaceKHR surface);
    void create_device();

private:
    VkInstance m_instance;

    VkPhysicalDevice m_physical_device;
    VkPhysicalDeviceProperties m_physical_device_properties;
    u32 m_graphics_family;
    u32 m_present_family;

    VkDevice m_device;
    VkQueue m_graphics_queue;
    VkQueue m_present_queue;
};
