#include "config.h"

#include <fstream>

#include "fkYAML/node.hpp"

#include "json_utils.h"

Configuration load_config_file(const std::filesystem::path &path) {
    // Generate a default configuration file if no configuration file exists
    // yet.
    if (!std::filesystem::exists(path)) {
        std::ofstream ofs;
        ofs.open(path.string().c_str());
        ofs << "---" << std::endl;
        ofs << "# Which port the server should listen on." << std::endl;
        ofs << "port: 8080" << std::endl;
        ofs << std::endl;
        ofs << "# Which content types the static server should use. 'if' is a "
               "regex that's"
            << std::endl;
        ofs << "# matched against the filename; if it matches, the content "
               "type in 'then'"
            << std::endl;
        ofs << "# is sent." << std::endl;
        ofs << "content-types:" << std::endl;
        ofs << "  - if: .*\\.html?" << std::endl;
        ofs << "    then: text/html; charset=utf-8" << std::endl;
        ofs << "  - if: .*\\.json" << std::endl;
        ofs << "    then: application/json; charset=utf-8" << std::endl;
        ofs << "  - if: .*\\.js" << std::endl;
        ofs << "    then: text/javascript; charset=utf-8" << std::endl;
        ofs << "  - if: .*\\.css" << std::endl;
        ofs << "    then: text/css; charset=utf-8" << std::endl;
        ofs << "  - if: .*\\.svg" << std::endl;
        ofs << "    then: image/svg+xml; charset=utf-8" << std::endl;
        ofs << "  - if: .*\\.png" << std::endl;
        ofs << "    then: image/png" << std::endl;
        ofs << "  - if: .*\\.jpe?g" << std::endl;
        ofs << "    then: image/jpeg" << std::endl;
        ofs << "  - if: .*\\.gif" << std::endl;
        ofs << "    then: image/gif" << std::endl;
        ofs << "  - if: .*\\.ico" << std::endl;
        ofs << "    then: image/vnd.microsoft.icon" << std::endl;
        ofs << std::endl;
        ofs << "# Example of a custom data structure. See api.md for more "
               "information."
            << std::endl;
        ofs << "custom-structures:" << std::endl;
        ofs << "  gps:" << std::endl;
        ofs << "    coord:" << std::endl;
        ofs << "      x:" << std::endl;
        ofs << "        key: truck.world.placement.0" << std::endl;
        ofs << "        operator: round" << std::endl;
        ofs << "      y:" << std::endl;
        ofs << "        key: truck.world.placement.1" << std::endl;
        ofs << "        operator: round" << std::endl;
        ofs << "      z:" << std::endl;
        ofs << "        key: truck.world.placement.2" << std::endl;
        ofs << "        operator: round" << std::endl;
        ofs << "    heading:" << std::endl;
        ofs << "      key: truck.world.placement.3" << std::endl;
        ofs << "      scale: 360" << std::endl;
        ofs << "      operator: round" << std::endl;
        ofs << "    time:" << std::endl;
        ofs << "      key: game.time" << std::endl;
        ofs << "      operator: date" << std::endl;
        ofs.close();
    }

    // Load the configuration file.
    std::ifstream ifs;
    ifs.open(path.string().c_str());
    if (!ifs.is_open()) {
        throw std::runtime_error("failed to load default configuration file");
    }
    try {
        auto yaml = fkyaml::node::deserialize(ifs);
        Configuration server_config = {};
        server_config.port = yaml["port"].get_value<int>();
        for (const auto &ob : yaml["content-types"]) {
            auto regex = std::regex(ob["if"].get_value<std::string>());
            auto content_type = ob["then"].get_value<std::string>();
            server_config.content_types.emplace_back(regex, content_type);
        }
        if (yaml.contains("custom-structures")) {
            server_config.custom_structures =
                yaml_to_json(yaml["custom-structures"]);
        }
        return server_config;
    } catch (std::exception &e) {
        throw std::runtime_error(
            "failed to parse configuration file: " + std::string(e.what())
        );
    }
}
