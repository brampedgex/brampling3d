#include "engine.hpp"
#include "cli.hpp"

#include <argparse/argparse.hpp>

namespace {

CliConfig parse_cli(int argc, char** argv) {
    argparse::ArgumentParser argparse{ "brampling3d" };

    argparse.add_argument("-w", "--width")
        .help("The width of the game window")
        .scan<'i', u32>();

    argparse.add_argument("-h", "--height")
        .help("The height of the game window")
        .scan<'i', u32>();

    try {
        argparse.parse_args(argc, argv);
    } catch (std::exception& e) {
        fmt::println(stderr, "error parsing command line arguments: {}", e.what());
        fmt::println("{}", argparse.help().str());
        std::exit(1);
    }
    
    CliConfig config;

    if (auto width = argparse.present<u32>("--width"))
        config.m_width = *width;
    if (auto height = argparse.present<u32>("--height"))
        config.m_height = *height; 

    return config;
}

}

int main(int argc, char** argv) {
    try {
        CliConfig config = parse_cli(argc, argv);

        Engine engine{ config };
        
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
