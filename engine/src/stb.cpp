#include "stb.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace stb {

std::optional<Image> Image::from_bytes(std::span<const u8> buffer, u8 channels) {
    int x, y, n;
    u8* data = stbi_load_from_memory(buffer.data(), buffer.size(), &x, &y, &n, channels);
    
    // Error?
    if (!data) {
        return std::nullopt;
    }

    return Image(data, x, y, n);
}

std::optional<Image> Image::from_stream(std::istream& in, u8 channels) {
    stbi_io_callbacks callbacks{
        .read = +[](void* user, char* data, i32 size) -> i32 {
            auto& in = *static_cast<std::istream*>(user);
            auto pos = in.tellg();
            in.read(data, size);
            auto pos_after = in.tellg();
            return pos_after - pos;
        },
        .skip = +[](void* user, i32 n) {
            auto& in = *static_cast<std::istream*>(user);
            in.seekg(n, std::ios_base::cur);
        },
        .eof = +[](void* user) -> i32 {
            auto& in = *static_cast<std::istream*>(user);
            return in.eof();
        }
    };

    int x, y, n;
    u8* data = stbi_load_from_callbacks(&callbacks, &in, &x, &y, &n, channels);

    // Error?
    if (!data) {
        return std::nullopt;
    }

    return Image(data, x, y, n);
}

void Image::cleanup() {
    if (m_image)
        stbi_image_free(m_image);
}

}
