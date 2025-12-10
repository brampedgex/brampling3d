#include "engine.hpp"

int main(int argc, char** argv) {
    try {
        Engine engine;
        
        // Initialize everything.
        try {
            engine.start();
        } catch (std::exception& e) {
            spdlog::critical("exception thrown during initialization: {}", e.what());
            return -1;
        }

        // Run main loop.
        engine.run();
    } catch(std::exception& e) {
        spdlog::critical("uncaught exception: {}", e.what());
        return -1;
    }

    return 0;
}
