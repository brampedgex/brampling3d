#include <thread>

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_vulkan.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>

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

    if ((window = SDL_CreateWindow("brampling3D", 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)) == nullptr) {
        sdl3_perror("Failed to create window");
        return 1;
    }

    if (volkInitialize() != VK_SUCCESS) {
        spdlog::critical("Failed to initialize Vulkan!");
        return 1;
    }

    uint32_t extensionCount;
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    if (!extensions) {
        sdl3_perror("failed to get vulkan instance extensions");
        return 1;
    }

    spdlog::info("extensionCount: {}", extensionCount);
    for (uint32_t i = 0; i < extensionCount; i++) {
        spdlog::info("extension {}: {}", i, extensions[i]);
    }

    SDL_SetWindowResizable(window, true);
    SDL_SetWindowMinimumSize(window, 640, 480);
    SDL_ShowWindow(window);

    spdlog::info("Initialized window!");

    bool quit = false;

    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
        }

        std::this_thread::sleep_for(16ms);
    }

    return 0;
}
