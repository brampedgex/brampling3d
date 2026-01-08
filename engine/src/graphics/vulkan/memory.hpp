#pragma once

#include "../../util/vulkan.hpp"

namespace vke {

u32 choose_memory_type(VkPhysicalDevice physical_device, u32 memory_type_bits, VkMemoryPropertyFlags mem_flags);

}
