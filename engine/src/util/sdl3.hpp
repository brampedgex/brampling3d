#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_vulkan.h>

/// Log an error message and the last SDL3 error
template <class... Args>
void sdl3_perror(fmt::format_string<Args...> fmt, Args&&... args) {
    std::string str = fmt::format(fmt, std::forward<Args>(args)...);
    spdlog::error("{}: {}", str, SDL_GetError());
}

bool sdl3_init();
