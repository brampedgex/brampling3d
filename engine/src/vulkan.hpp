#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <volk.h>

std::string_view vulkan_error_str(VkResult res);
