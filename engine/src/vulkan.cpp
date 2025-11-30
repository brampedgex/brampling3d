#include "vulkan.hpp"

std::string_view vulkan_error_str(VkResult res) {
    return std::string_view{ string_VkResult(res) };
}
