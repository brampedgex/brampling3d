#pragma once

#include <stb_image.h>

namespace stb {

class Image {
public: // Factory functions
    static std::optional<Image> from_bytes(std::span<const u8> buffer, u8 channels = 0);
    static std::optional<Image> from_stream(std::istream& in, u8 channels = 0);

public: // Constructors/destructors/operators
    ~Image() {
        cleanup();
    }

    Image() = default;

    Image(const Image&) = delete;
    Image(Image&& move) noexcept {
        *this = std::move(move);
    }

    Image& operator=(const Image&) = delete;
    Image& operator=(Image&& move) noexcept {
        cleanup();

        m_image = std::exchange(move.m_image, nullptr);
        m_width = move.m_width;
        m_height = move.m_height;
        m_channels = move.m_channels;
        return *this;
    }

public: // Getters
    [[nodiscard]] const u8* data() const { return m_image; }
    [[nodiscard]] auto width() const { return m_width; }
    [[nodiscard]] auto height() const { return m_height; }
    [[nodiscard]] auto channels() const { return m_channels; }

private: // Internal methods
    Image(u8* image, u32 width, u32 height, u8 channels) :
        m_image(image),
        m_width(width),
        m_height(height),
        m_channels(channels) {
    }

    void cleanup();

private:
    u8* m_image = nullptr;
    u32 m_width = 0;
    u32 m_height = 0;
    u8 m_channels = 0;
};

}
