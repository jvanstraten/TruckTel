#include <filesystem>
#include <list>
#include <set>

#include <scssdk_input.h>
#include <scssdk_telemetry.h>

#include "input.h"
#include "logger.h"
#include "recorder/recorder.h"
#include "server/server_thread.h"

//------------------------------------------------------------------------------
// Directory structure
//------------------------------------------------------------------------------
/// Name of the plugins directory loaded by the game.
static constexpr auto DIR_PLUGINS = "plugins";

/// Name of TruckTel's working directory, containing the app directories and
/// log file.
static constexpr auto DIR_TRUCKTEL = "trucktel";

/// Name of TruckTel's log file.
static constexpr auto LOG_FILENAME = "log.txt";

/// Default app that TruckTel generates when it's missing any app
/// subdirectories.
static constexpr auto DIR_APP_DEFAULT = "your-app-name";

//------------------------------------------------------------------------------
// Common logic
//------------------------------------------------------------------------------
// Depended on by both the telemetry and input sides of the plugin.

/// Detected game installation directory.
static std::filesystem::path game_install_path;

/// Detected trucktel configuration directory.
static std::filesystem::path trucktel_path;

/// List of application servers.
static std::list<ServerThread> servers;

/// Initializes logic common to both the telemetry and input sides of the
/// plugin. This includes the logger and environment detection. These things
/// are initialized from whichever side of the plugin is initialized by the
/// game first.
static void common_init(const scs_sdk_init_params_v100_t &init_params) {
    // Initialize the logger. This won't do anything if initialize_input()
    // already initialized the logger.
    Logger::init(init_params.log);

    // The working directory for ETS2 seems to be the directory its
    // executable is placed in, so the path below should point to the
    // plugin directory.
    const auto cwd = std::filesystem::current_path();
    const auto plugin_path = cwd / DIR_PLUGINS;

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
    game_install_path = cwd.parent_path().parent_path();

    // We'll try to pollute the plugins directory only minimally by putting
    // everything other than the plugin itself into our own directory. This
    // will hold the log file, configuration, static content for the server,
    // etc.
    trucktel_path = plugin_path / DIR_TRUCKTEL;

    // If the trucktel directory doesn't exist yet, create it.
    if (!std::filesystem::is_directory(trucktel_path)) {
        std::filesystem::create_directory(trucktel_path);
    }

    // Write to log file within the trucktel directory.
    Logger::set_file((trucktel_path / LOG_FILENAME).string());

    // Each subdirectory within the trucktel directory represents a separate
    // app, each with its own configuration file. If there is no app yet, a
    // default one is generated.
    servers.clear();
    std::set<uint16_t> ports_used;
    for (auto const &entry :
         std::filesystem::directory_iterator{trucktel_path}) {
        if (!entry.is_directory()) continue;
        const auto &app_path = entry.path();
        const auto app_name = app_path.filename().string();
        Logger::info("Loading configuration for app: %s", app_name.c_str());
        try {
            servers.emplace_back(app_path);
            const auto port = servers.back().port();
            if (ports_used.insert(port).second) {
                Logger::info("%s will run on port %d", app_name.c_str(), port);
            } else {
                Logger::error(
                    "%s wants to use port %d, which is already in use.",
                    app_name.c_str(), port
                );
                Logger::error(
                    "App will be disabled. Assign it a different port in its "
                    "config file!"
                );
                servers.pop_back();
            }
        } catch (const std::exception &e) {
            Logger::error(
                "Failed to load %s app: %s", app_name.c_str(), e.what()
            );
        }
    }
    if (servers.empty()) {
        const auto default_app_path = trucktel_path / DIR_APP_DEFAULT;
        Logger::warn("No apps configured! Generating default...");
        std::filesystem::create_directory(default_app_path);
        servers.emplace_back(default_app_path);
    }
}

/// Shuts down common logic. Shutdown is only performed if this was the last
/// reference.
static void common_shutdown() {
    Logger::info("Shutting down logger. Bye!");
    Logger::shutdown();
}

//------------------------------------------------------------------------------
// Server logic
//------------------------------------------------------------------------------
// This is the opposite of the common logic: it must be initialized only when
// both sides of the plugin have done there independent initializations, because
// the threading model we use depends on those things not being mutated while
// the server thread is running (aside from explicitly-locked things for the
// actual communication).

/// Initializes the server thread. Must be called only when both sides of the
/// plugin have finished initializing.
static void server_init() {
    if (servers.empty()) return;
    Logger::info("Initializing server thread(s)...");
    for (auto &server : servers) {
        server.start();
    }
}

/// Shuts down the server thread, if any, including waiting for it to join.
static void server_shutdown() {
    if (servers.empty()) return;
    Logger::info("Shutting down server(s)...");
    for (auto &server : servers) {
        server.stop();
    }
    for (auto &server : servers) {
        server.join();
    }
    servers.clear();
    Logger::info("All servers have shut down");
}

//------------------------------------------------------------------------------
// Telemetry and input
//------------------------------------------------------------------------------

/// Whether the telemetry recorder is initialized.
static bool telemetry_initialized = false;

/// Whether the input side is initialized.
static bool input_initialized = false;

/// Main initialization function for the telemetry recorder side of the plugin.
static void telemetry_init(
    const scs_u32_t version,
    const scs_telemetry_init_params_v101_t *const init_params
) {
    // Only one recorder at once, please.
    if (telemetry_initialized) {
        throw std::runtime_error(
            "attempt to initialize telemetry recorder twice"
        );
    }

    // Make sure common logic is initialized.
    if (!input_initialized) {
        common_init(init_params->common);
    }

    // Initialize the data recording logic.
    Logger::info("Initializing telemetry logic...");
    Recorder::init(version, init_params, game_install_path.string());
    telemetry_initialized = true;

    // If the input side of the plugin was loaded first, initialize the server
    // thread.
    if (input_initialized) {
        server_init();
    }
}

/// Main shutdown function for the telemetry recorder side of the plugin.
static void telemetry_shutdown() {
    // Can't shut down if we're not initialized.
    if (!telemetry_initialized) {
        throw std::runtime_error(
            "attempt to shut down uninitialized telemetry recorder"
        );
    }

    // Make sure the server is shut down.
    if (input_initialized) {
        server_shutdown();
    }

    // Shut down the recorder.
    Logger::info("Shutting down telemetry logic...");
    Recorder::shutdown();
    telemetry_initialized = false;

    // If the input side was shut down first, shut down the common logic.
    if (!input_initialized) {
        common_shutdown();
    }
}

/// Main initialization function for the input side of the plugin.
static void input_init(const scs_input_init_params_v100_t *const init_params) {
    // Only one input handler at once, please.
    if (input_initialized) {
        throw std::runtime_error("attempt to initialize input handler twice");
    }

    // Make sure common logic is initialized.
    if (!telemetry_initialized) {
        common_init(init_params->common);
    }

    // Initialize the input logic.
    Logger::info("Initializing input logic...");
    InputChannelDescriptors descriptors;
    for (auto &server : servers) {
        for (const auto &item : server.get_input_descriptors()) {
            descriptors.emplace(item);
        }
    }
    Input::init(init_params, descriptors);
    input_initialized = true;

    // If the telemetry side of the plugin was loaded first, initialize the
    // server thread.
    if (telemetry_initialized) {
        server_init();
    }
}

/// Main shutdown function for the input side of the plugin.
static void input_shutdown() {
    // Can't shut down if we're not initialized.
    if (!input_initialized) {
        throw std::runtime_error(
            "attempt to shut down uninitialized input handler"
        );
    }

    // Make sure the server is shut down.
    if (telemetry_initialized) {
        server_shutdown();
    }

    // Shut down the recorder.
    Logger::info("Shutting down input logic...");
    Input::shutdown();
    input_initialized = false;

    // If the telemetry side was shut down first, shut down the common logic.
    if (!telemetry_initialized) {
        common_shutdown();
    }
}

//------------------------------------------------------------------------------
// Entry points
//------------------------------------------------------------------------------

/// SCS API telemetry initialization callback. This is the entry point that
/// ETS2/ATS calls for telemetry plugins.
SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version, const scs_telemetry_init_params_t *const params
) {
    // Check that we're called as expected.
    if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
    if (!params) return SCS_RESULT_invalid_parameter;
    const auto *init_params =
        reinterpret_cast<const scs_telemetry_init_params_v101_t *>(params);

    try {
        telemetry_init(version, init_params);
        Logger::info("scs_telemetry_init() complete");
        return SCS_RESULT_ok;
    } catch (std::exception &e) {
        Logger::error("scs_telemetry_init() error: %s", e.what());
        return SCS_RESULT_generic_error;
    }
}

/// SCS telemetry API cleanup handler.
SCSAPI_VOID scs_telemetry_shutdown() {
    try {
        telemetry_shutdown();
    } catch (std::exception &e) {
        Logger::error("Telemetry recorder shutdown error: %s", e.what());
    }
}

/// SCS API input initialization callback. This is the entry point that ETS2/ATS
/// calls for custom input devices.
SCSAPI_RESULT scs_input_init(
    const scs_u32_t version, const scs_input_init_params_t *const params
) {
    // Check that we're called as expected.
    if (version != SCS_INPUT_VERSION_1_00) return SCS_RESULT_unsupported;
    if (!params) return SCS_RESULT_invalid_parameter;
    const auto *init_params =
        reinterpret_cast<const scs_input_init_params_v100_t *>(params);

    try {
        input_init(init_params);
        Logger::info("scs_input_init() complete");
        return SCS_RESULT_ok;
    } catch (std::exception &e) {
        Logger::error("scs_input_init() error: %s", e.what());
        return SCS_RESULT_generic_error;
    }
}

/// SCS input API cleanup handler.
SCSAPI_VOID scs_input_shutdown() {
    try {
        input_shutdown();
    } catch (std::exception &e) {
        Logger::error("Input handler shutdown error: %s", e.what());
    }
}
