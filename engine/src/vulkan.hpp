#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vk_enum_string_helper.h>
#include <volk.h>

std::string_view vulkan_error_str(VkResult res);

template <class... Args>
std::string vulkan_error_msg(VkResult res, fmt::format_string<Args...> fmt, Args&&... args) {
    auto out = fmt::memory_buffer();
    fmt::format_to(std::back_inserter(out), fmt, std::forward<Args>(args)...);
    fmt::format_to(std::back_inserter(out), ": {}", vulkan_error_str(res));
    return fmt::to_string(out);
}

template <class... Args>
void vulkan_check_res(VkResult res, fmt::format_string<Args...> fmt, Args&&... args) {
    if (res != VK_SUCCESS) {
        std::string message = vulkan_error_msg(res, fmt, std::forward<Args>(args)...);
        spdlog::error("fatal vulkan error: {}", message);
        throw std::runtime_error(message);
    }
}
