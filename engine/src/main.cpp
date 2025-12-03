#include "engine.hpp"

int main(int argc, char** argv) {
    try {
        Engine engine;
        
        if (!engine.start()) {
            spdlog::critical("startup failed!");
            return 1;
        }

        engine.run();
    } catch(std::exception& e) {
        spdlog::critical("uncaught exception: {}", e.what());
    }

    return 0;
}
