#include "engine.hpp"

int main(int argc, char** argv) {
    Engine engine;
    
    if (!engine.start()) {
        spdlog::critical("startup failed!");
        return 1;
    }

    engine.run();
    return 0;
}
