#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "scssdk_telemetry.h"

#include "fkYAML/node.hpp"
#include "nlohmann/json.hpp"

#include "logger.h"
#include "recorder/recorder.h"
#include "server/server_thread.h"

/// Tries to load the configuration file. If the file does not exist, a default
/// file is written.
static ServerConfig load_config_file(const std::filesystem::path &path) {
    // Generate a default configuration file if no configuration file exists
    // yet.
    if (!std::filesystem::exists(path)) {
        std::ofstream ofs;
        ofs.open(path.string().c_str());
        ofs << "---" << std::endl;
        ofs << "# Which port the server should listen on." << std::endl;
        ofs << "port: 8080" << std::endl;
        ofs << "" << std::endl;
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
        ServerConfig server_config = {};
        server_config.port = yaml["port"].get_value<int>();
        for (const auto &ob : yaml["content-types"]) {
            auto regex = std::regex(ob["if"].get_value<std::string>());
            auto content_type = ob["then"].get_value<std::string>();
            server_config.content_types.emplace_back(regex, content_type);
        }
        return server_config;
    } catch (std::exception &e) {
        throw std::runtime_error(
            "failed to parse configuration file: " + std::string(e.what())
        );
    }
}

/// Main initialization function.
static void initialize(
    const scs_u32_t version,
    const scs_telemetry_init_params_v101_t *const init_params
) {
    // Initialize the logger.
    Logger::init(init_params->common.log);

    // The working directory for ETS2 seems to be the directory its
    // executable is placed in, so the path below should point to the
    // plugin directory.
    const auto cwd = std::filesystem::current_path();
    const auto plugin_path = cwd / "plugins";

    // Make sure that this directory actually exists, to get some confidence
    // that we're looking in the right place.
    if (!std::filesystem::is_directory(plugin_path)) {
        throw std::runtime_error(
            "failed to find plugin directory! Expected it to be " +
            plugin_path.string()
        );
    }

    // If that's where the plugin directory is, then the installation
    // directory for the game is two levels up.
    const auto game_install_path = cwd.parent_path().parent_path();

    // We'll try to pollute the plugins directory only minimally by putting
    // everything other than the plugin itself into our own directory. This
    // will hold the log file, configuration, static content for the server,
    // etc.
    const auto trucktel_path = plugin_path / "trucktel";

    // If the trucktel directory doesn't exist yet, create it.
    if (!std::filesystem::is_directory(trucktel_path)) {
        std::filesystem::create_directory(trucktel_path);
    }

    // Log file within the trucktel directory.
    Logger::set_file((trucktel_path / "log.txt").string());

    // Load the configuration file.
    ServerConfig server_config =
        load_config_file(trucktel_path / "config.yaml");

    // Initialize the data recording logic.
    Recorder::init(version, init_params, game_install_path.string());

    // Initialize the HTTP server logic.
    server_config.document_root = trucktel_path / "www";
    ServerThread::init(server_config);
}

/// SCS API initialization callback. This is the entry point that ETS2/ATS
/// calls.
SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version, const scs_telemetry_init_params_t *const params
) {
    // Check that we're called as expected.
    if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
    if (!params) return SCS_RESULT_invalid_parameter;
    const auto *init_params =
        reinterpret_cast<const scs_telemetry_init_params_v101_t *>(params);

    try {
        initialize(version, init_params);
        Logger::info("Init complete");
        return SCS_RESULT_ok;
    } catch (std::exception &e) {
        Logger::error("Init error: %s", e.what());
        return SCS_RESULT_generic_error;
    }
}

/// SCS API cleanup handler.
SCSAPI_VOID scs_telemetry_shutdown() {
    Logger::info("Waiting for server to shut down");
    ServerThread::shutdown();
    Logger::info("Shutting down");
    Recorder::shutdown();
    Logger::shutdown();
}
