#include "config.h"

#include <fstream>

#include <common/scssdk_telemetry_common_channels.h>
#include <common/scssdk_telemetry_truck_common_channels.h>

#include <fkYAML/node.hpp>

#include "json_utils.h"

Configuration load_config_file(const std::filesystem::path &path) {
    // Generate a default configuration file if no configuration file exists
    // yet.
    if (!std::filesystem::exists(path)) {
        std::ofstream ofs;
        ofs.open(path.string().c_str());
        ofs << "---" << std::endl;
        ofs << "# Which port the server should listen on." << std::endl;
        ofs << CONFIG_PORT << ": 8080" << std::endl;
        ofs << std::endl;
        ofs << "# Which content types the static server should use. '"
            << CONFIG_CONTENT_TYPE_FILENAME << "' is a regex that's"
            << std::endl;
        ofs << "# matched against the filename; if it matches, the content "
               "type in '"
            << CONFIG_CONTENT_TYPE_RESULT << "'" << std::endl;
        ofs << "# is sent." << std::endl;
        ofs << CONFIG_CONTENT_TYPES << ":" << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.html?"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT
            << ": text/html; charset=utf-8" << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.json"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT
            << ": application/json; charset=utf-8" << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.js"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT
            << ": text/javascript; charset=utf-8" << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.css"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT
            << ": text/css; charset=utf-8" << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.svg"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT
            << ": image/svg+xml; charset=utf-8" << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.png"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT << ": image/png"
            << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.jpe?g"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT << ": image/jpeg"
            << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.gif"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT << ": image/gif"
            << std::endl;
        ofs << "  - " << CONFIG_CONTENT_TYPE_FILENAME << ": .*\\.ico"
            << std::endl;
        ofs << "    " << CONFIG_CONTENT_TYPE_RESULT
            << ": image/vnd.microsoft.icon" << std::endl;
        ofs << std::endl;
        ofs << "# Example of a custom data structure. See api.md for more "
               "information."
            << std::endl;
        ofs << CONFIG_CUSTOM_STRUCTURES << ":" << std::endl;
        ofs << "  gps:" << std::endl;
        ofs << "    coord:" << std::endl;
        ofs << "      x:" << std::endl;
        ofs << "        " << CONFIG_FORMAT_KEY << ": "
            << SCS_TELEMETRY_TRUCK_CHANNEL_world_placement << ".0" << std::endl;
        ofs << "        " << CONFIG_FORMAT_OPERATOR << ": "
            << CONFIG_OPERATOR_ROUND << std::endl;
        ofs << "      y:" << std::endl;
        ofs << "        " << CONFIG_FORMAT_KEY << ": "
            << SCS_TELEMETRY_TRUCK_CHANNEL_world_placement << ".1" << std::endl;
        ofs << "        " << CONFIG_FORMAT_OPERATOR << ": "
            << CONFIG_OPERATOR_ROUND << std::endl;
        ofs << "      z:" << std::endl;
        ofs << "        " << CONFIG_FORMAT_KEY << ": "
            << SCS_TELEMETRY_TRUCK_CHANNEL_world_placement << ".2" << std::endl;
        ofs << "        " << CONFIG_FORMAT_OPERATOR << ": "
            << CONFIG_OPERATOR_ROUND << std::endl;
        ofs << "    heading:" << std::endl;
        ofs << "      " << CONFIG_FORMAT_KEY << ": "
            << SCS_TELEMETRY_TRUCK_CHANNEL_world_placement << ".3" << std::endl;
        ofs << "      " << CONFIG_FORMAT_SCALE << ": 360" << std::endl;
        ofs << "      " << CONFIG_FORMAT_OPERATOR << ": "
            << CONFIG_OPERATOR_ROUND << std::endl;
        ofs << "    time:" << std::endl;
        ofs << "      " << CONFIG_FORMAT_KEY << ": "
            << SCS_TELEMETRY_CHANNEL_game_time << std::endl;
        ofs << "      " << CONFIG_FORMAT_OPERATOR << ": "
            << CONFIG_OPERATOR_DATE << std::endl;
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
        server_config.port = yaml[CONFIG_PORT].get_value<int>();
        for (const auto &ob : yaml[CONFIG_CONTENT_TYPES]) {
            auto regex = std::regex(
                ob[CONFIG_CONTENT_TYPE_FILENAME].get_value<std::string>()
            );
            auto content_type =
                ob[CONFIG_CONTENT_TYPE_RESULT].get_value<std::string>();
            server_config.content_types.emplace_back(regex, content_type);
        }
        if (yaml.contains(CONFIG_CUSTOM_STRUCTURES)) {
            server_config.custom_structures =
                yaml_to_json(yaml[CONFIG_CUSTOM_STRUCTURES]);
        }
        if (yaml.contains(CONFIG_INPUT)) {
            server_config.input_configuration =
                yaml_to_json(yaml[CONFIG_INPUT]);
        } else {
            server_config.input_configuration = nullptr;
        }
        return server_config;
    } catch (std::exception &e) {
        throw std::runtime_error(
            "failed to parse configuration file: " + std::string(e.what())
        );
    }
}
