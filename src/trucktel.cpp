#include <filesystem>

#include <scssdk_input.h>
#include <scssdk_telemetry.h>

#include "config.h"
#include "input.h"
#include "logger.h"
#include "recorder/recorder.h"
#include "server/server_thread.h"

//------------------------------------------------------------------------------
// Common logic
//------------------------------------------------------------------------------
// Depended on by both the telemetry and input sides of the plugin.

/// Detected game installation directory.
static std::filesystem::path game_install_path;

/// Detected trucktel configuration directory.
static std::filesystem::path trucktel_path;

/// TruckTel configuration.
static Configuration configuration;

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
    game_install_path = cwd.parent_path().parent_path();

    // We'll try to pollute the plugins directory only minimally by putting
    // everything other than the plugin itself into our own directory. This
    // will hold the log file, configuration, static content for the server,
    // etc.
    trucktel_path = plugin_path / "trucktel";

    // If the trucktel directory doesn't exist yet, create it.
    if (!std::filesystem::is_directory(trucktel_path)) {
        std::filesystem::create_directory(trucktel_path);
    }

    // Log file within the trucktel directory.
    Logger::set_file((trucktel_path / "log.txt").string());

    // Load the configuration file.
    configuration = load_config_file(trucktel_path / "config.yaml");
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
    // Initialize the HTTP server logic.
    Logger::info("Initializing server thread...");
    configuration.document_root = trucktel_path / "www";
    ServerThread::init(configuration);
}

/// Shuts down the server thread, if any, including waiting for it to join.
static void server_shutdown() {
    ServerThread::shutdown();
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
    Input::init(init_params, configuration);
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
