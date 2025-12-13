#include "sdl3.hpp"

bool sdl3_init() {
    SDL_SetLogOutputFunction([](void*, int category, SDL_LogPriority priority, const char* message) {
        spdlog::level::level_enum level;

        switch (priority) {
        case SDL_LOG_PRIORITY_TRACE:
            level = spdlog::level::trace;
            break;
        case SDL_LOG_PRIORITY_VERBOSE:
        case SDL_LOG_PRIORITY_DEBUG:
            level = spdlog::level::debug;
            break;
        case SDL_LOG_PRIORITY_INFO:
        default:
            level = spdlog::level::info;
            break;
        case SDL_LOG_PRIORITY_WARN:
            level = spdlog::level::warn;
            break;
        case SDL_LOG_PRIORITY_ERROR:
            level = spdlog::level::err;
            break;
        case SDL_LOG_PRIORITY_CRITICAL:
            level = spdlog::level::critical;
            break;
        }

        spdlog::log(level, "[SDL3] {}", message);
    }, nullptr);

    SDL_SetAppMetadata("brampling3D", nullptr, "brampling3D");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        sdl3_perror("Failed to initialize SDL3");
        return false;
    }

    return true;
}

