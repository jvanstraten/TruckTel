#include "config.h"

#include <fstream>

#include <common/scssdk_telemetry_common_channels.h>
#include <common/scssdk_telemetry_truck_common_channels.h>

#include <fkYAML/node.hpp>

#include "../json_utils.h"

/// Default index.html file generated if no www folder exists.
static constexpr auto DEFAULT_INDEX_FILE = R"(
<!doctype html>
<html>
<head>
<title>TruckTel is working!</title>
<body>
<h1>TruckTel is working!</h1>
<p>This is the default landing page for the webserver embedded in TruckTel.</p>
<p>If you're the developer of an app, you should probably replace this with
your own landing page.</p>
</body>
</head>
</html>
)";

Configuration load_app_config(const std::filesystem::path &app_path) {
    // Get configuration file path.
    const auto config_path = app_path / CONFIG_FILENAME;

    // Generate a default configuration file if no configuration file exists
    // yet.
    if (!std::filesystem::exists(config_path)) {
        std::ofstream ofs;
        ofs.open(config_path.string().c_str());
        ofs << "---" << std::endl;
        ofs << "# Which port the server should listen on." << std::endl;
        ofs << CONFIG_PORT << ": " << CONFIG_DEFAULT_PORT << std::endl;
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
    ifs.open(config_path.string().c_str());
    if (!ifs.is_open()) {
        throw std::runtime_error("failed to load default configuration file");
    }
    try {
        auto yaml = fkyaml::node::deserialize(ifs);
        Configuration server_config = {};

        // Parse port number.
        server_config.port = yaml[CONFIG_PORT].get_value<uint16_t>();

        // Set document root.
        server_config.document_root = app_path / CONFIG_DOCUMENT_ROOT_SUBDIR;

        // If the document root does not exist yet, create it, and add a default
        // index.html.
        if (!std::filesystem::is_directory(server_config.document_root)) {
            std::filesystem::create_directory(server_config.document_root);
            const auto index =
                server_config.document_root / CONFIG_INDEX_FILENAME;
            std::ofstream ofs;
            ofs.open(index.string().c_str());
            ofs << DEFAULT_INDEX_FILE;
            ifs.close();
        }

        // Parse content types.
        for (const auto &ob : yaml[CONFIG_CONTENT_TYPES]) {
            auto regex = std::regex(
                ob[CONFIG_CONTENT_TYPE_FILENAME].get_value<std::string>()
            );
            auto content_type =
                ob[CONFIG_CONTENT_TYPE_RESULT].get_value<std::string>();
            server_config.content_types.emplace_back(regex, content_type);
        }

        // Parse custom data structures for the API.
        if (yaml.contains(CONFIG_CUSTOM_STRUCTURES)) {
            server_config.custom_structures =
                yaml_to_json(yaml[CONFIG_CUSTOM_STRUCTURES]);
        }

        // Parse input configuration.
        if (yaml.contains(CONFIG_INPUT)) {
            const auto &input_yaml = yaml[CONFIG_INPUT];
            if (input_yaml.contains(CONFIG_INPUT_FLOAT)) {
                const auto &inputs = input_yaml[CONFIG_INPUT_FLOAT];
                for (const auto &item : inputs.map_items()) {
                    const auto name = item.key().get_value<std::string>();
                    const auto friendly = item.value().get_value<std::string>();
                    server_config.input_channel_descriptors.emplace(
                        name, InputChannelDescriptor{
                                  friendly, InputChannelType::FLOAT
                              }
                    );
                }
            }
            if (input_yaml.contains(CONFIG_INPUT_BINARY)) {
                const auto &inputs = input_yaml[CONFIG_INPUT_BINARY];
                for (const auto &item : inputs.map_items()) {
                    const auto name = item.key().get_value<std::string>();
                    const auto friendly = item.value().get_value<std::string>();
                    server_config.input_channel_descriptors.emplace(
                        name, InputChannelDescriptor{
                                  friendly, InputChannelType::BINARY
                              }
                    );
                }
            }
        }

        return server_config;
    } catch (std::exception &e) {
        throw std::runtime_error(
            "failed to parse configuration file: " + std::string(e.what())
        );
    }
}
