#include <filesystem>

#include "scssdk_telemetry.h"

#include "config.h"
#include "logger.h"
#include "recorder/recorder.h"
#include "server/server_thread.h"

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
    Configuration server_config =
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
