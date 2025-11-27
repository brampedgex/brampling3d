#include <thread>

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_hints.h>

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

/// Log an error message and the last SDL3 error
template <class... Args>
static void sdl3_perror(fmt::format_string<Args...> fmt, Args&&... args) {
    std::string str = fmt::format(fmt, std::forward<Args>(args)...);
    spdlog::error("{}: {}", str, SDL_GetError());
}

static bool init_sdl3() {
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

int main(int argc, char** argv) {
    if (!init_sdl3()) {
        return 1;
    }

    SDL_Window* window;

    if ((window = SDL_CreateWindow("brampling3D", 640, 480, SDL_WINDOW_HIDDEN)) == nullptr) {
        sdl3_perror("Failed to create window");
        return 1;
    }

    SDL_SetWindowResizable(window, true);
    SDL_SetWindowMinimumSize(window, 640, 480);
    SDL_ShowWindow(window);

    SDL_Renderer* renderer;
    if ((renderer = SDL_CreateRenderer(window, "software")) == nullptr) {
        sdl3_perror("Failed to create renderer");
        return 1;
    }

    spdlog::info("Initialized window!");

    bool quit = false;

    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        std::this_thread::sleep_for(16ms);
    }

    return 0;
}
