#include "core/main_system.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <aircraft_data_file>" << std::endl;
        return 1;
    }

    try {
        atc::MainSystem system;

        // Initialize system
        if (!system.initialize()) {
            std::cerr << "System initialization failed" << std::endl;
            return 1;
        }

        // Load aircraft data
        if (!system.loadAircraftData(argv[1])) {
            std::cerr << "Failed to load aircraft data" << std::endl;
            return 1;
        }

        // Run the system
        system.run();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

