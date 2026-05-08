#include "config.h"

#include <fstream>

#include <fkYAML/node.hpp>

#include "logger.h"

LandingConfiguration::LandingConfiguration(
    const std::filesystem::path &trucktel_path
)
    : trucktel_path(trucktel_path) {
    // Get configuration file path.
    const auto config_path = trucktel_path / CONFIG_FILENAME;

    // Generate a default configuration file if no configuration file exists
    // yet.
    if (!std::filesystem::exists(config_path)) {
        std::ofstream ofs;
        ofs.open(config_path.string().c_str());
        ofs << "---" << std::endl;
        ofs << "# Whether the landing server should be enabled. This server "
               "hosts a simple"
            << std::endl;
        ofs << "# page that links to and generates QR codes for apps running "
               "on TruckTel."
            << std::endl;
        ofs << CONFIG_KEY_ENABLE << ": true" << std::endl;
        ofs << std::endl;
        ofs << "# The port on which the landing server should listen."
            << std::endl;
        ofs << CONFIG_KEY_PORT << ": " << CONFIG_DEFAULT_PORT << std::endl;
        ofs << std::endl;
        ofs << "# Whether to open the landing page in a browser when the "
               "plugin loads."
            << std::endl;
        ofs << CONFIG_KEY_AUTO_OPEN << ": true" << std::endl;
        ofs.close();
    }

    // Load the configuration file.
    std::ifstream ifs;
    ifs.open(config_path.string().c_str());
    if (!ifs.is_open()) {
        Logger::error("Failed to load landing configuration file.");
    }
    try {
        auto yaml = fkyaml::node::deserialize(ifs);
        port = yaml[CONFIG_KEY_PORT].get_value<uint16_t>();
        auto_open = yaml[CONFIG_KEY_AUTO_OPEN].get_value<bool>();

        // Enabled last, so we only enable if all other keys loaded correctly.
        enabled = yaml[CONFIG_KEY_ENABLE].get_value<bool>();
    } catch (std::exception &e) {
        Logger::error(
            "Failed to parse landing configuration file: %s", e.what()
        );
    }
}

bool LandingConfiguration::is_enabled() const {
    return enabled;
}

const std::filesystem::path &LandingConfiguration::get_trucktel_path() const {
    return trucktel_path;
}

uint16_t LandingConfiguration::get_port() const {
    return port;
}

bool LandingConfiguration::is_auto_open_enabled() const {
    return auto_open;
}
