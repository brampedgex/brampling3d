#include <print>
#include <iostream>
#include <thread>

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_hints.h>

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

int main(int argc, char** argv) {
    spdlog::info("hello, spdlog");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(std::cerr, "Failed to initialize SDL!");
        return 1;
    }

    SDL_Window* window;

    if ((window = SDL_CreateWindow("brampling3D", 640, 480, SDL_WINDOW_HIDDEN)) == nullptr) {
        std::println(std::cerr, "Failed to create window!");
        return 1;
    }

    SDL_SetWindowResizable(window, true);
    SDL_SetWindowMinimumSize(window, 640, 480);
    SDL_ShowWindow(window);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, "software");

    std::println("Initialized SDL!");

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
