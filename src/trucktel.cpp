#include <filesystem>
#include <list>
#include <map>
#include <set>

#include <scssdk_input.h>
#include <scssdk_telemetry.h>

#include "input.h"
#include "landing/info.h"
#include "landing/server_thread.h"
#include "license.h"
#include "logger.h"
#include "mdns/server_thread.h"
#include "recorder/recorder.h"
#include "server/server_thread.h"
#include "version.h"

//------------------------------------------------------------------------------
// Directory structure
//------------------------------------------------------------------------------
/// Name of the plugins directory loaded by the game.
static constexpr auto DIR_PLUGINS = "plugins";

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

/// Application servers, mapping from directory name to thread.
static std::map<std::string, ServerThread> servers;

/// Information object for the landing server.
static LandingInfo landing_info;

/// mDNS configuration.
static std::unique_ptr<MdnsConfiguration> mdns_configuration;

/// Initializes logic common to both the telemetry and input sides of the
/// plugin. This includes the logger and environment detection. These things
/// are initialized from whichever side of the plugin is initialized by the
/// game first.
static void common_init(const scs_sdk_init_params_v100_t &init_params) {
    // Initialize the logger. This won't do anything if initialize_input()
    // already initialized the logger.
    Logger::init(init_params.log);
    Logger::info("Loading %s", TRUCKTEL_FULL_NAME);

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
    trucktel_path = plugin_path / TRUCKTEL_NAMESPACE;

    // If the trucktel directory doesn't exist yet, create it.
    if (!std::filesystem::is_directory(trucktel_path)) {
        std::filesystem::create_directory(trucktel_path);
    }

    // If the license file within the trucktel directory doesn't exist yet,
    // create it.
    const auto license_path = trucktel_path / TRUCKTEL_LICENSE_FILENAME;
    if (!std::filesystem::is_regular_file(license_path)) {
        std::ofstream ofs;
        ofs.open(license_path.string().c_str());
        ofs << TRUCKTEL_LICENSE_CONTENT;
        ofs.close();
    }

    // Write to log file within the trucktel directory.
    Logger::set_file((trucktel_path / LOG_FILENAME).string());

    // Load the mDNS configuration file.
    mdns_configuration = std::make_unique<MdnsConfiguration>(trucktel_path);

    // Each subdirectory within the trucktel directory represents a separate
    // app, each with its own configuration file. If there is no app yet, a
    // default one is generated.
    servers.clear();
    bool no_apps = true;
    for (auto const &entry :
         std::filesystem::directory_iterator{trucktel_path}) {
        if (!entry.is_directory()) continue;
        no_apps = false;
        const auto &app_path = entry.path();
        const auto app_directory = app_path.filename().string();
        Logger::info(
            "Loading configuration for app: %s", app_directory.c_str()
        );

        // Create server thread, which loads the configuration.
        try {
            servers.emplace(app_directory, app_path);
        } catch (const std::exception &e) {
            Logger::error(
                "Failed to load %s app: %s", app_directory.c_str(), e.what()
            );
            LandingAppInfo app_info{};
            app_info.error_message = e.what();
            landing_info.apps.emplace(app_directory, std::move(app_info));
        }
    }
    if (no_apps) {
        Logger::warn("No apps configured! Generating default...");
        const auto default_app_path = trucktel_path / DIR_APP_DEFAULT;
        std::filesystem::create_directory(default_app_path);
        servers.emplace(DIR_APP_DEFAULT, default_app_path);
    }

    // Generate app information structure for apps that loaded successfully.
    for (const auto &[app_directory, server] : servers) {
        const auto &metadata = server.metadata();
        LandingAppInfo app_info{};
        app_info.title = metadata.title;
        app_info.subtitle = metadata.subtitle;
        app_info.link = metadata.link;
        app_info.port = server.port();
        landing_info.apps.emplace(app_directory, std::move(app_info));
    }

    // Check for conflicts in port assignments.
    std::set<uint16_t> ports_used;

    // Claim a port for the landing server.
    // TODO: make configurable
    static constexpr uint16_t LANDING_PORT = 8000;
    ports_used.insert(LANDING_PORT);
    mdns_configuration->register_app(
        std::string(TRUCKTEL_NAMESPACE) + "-landing", LANDING_PORT
    );

    // Claim ports for the servers. In case of conflict, first come first serve.
    for (auto &[app_directory, app_info] : landing_info.apps) {
        auto it = servers.find(app_directory);
        if (it == servers.end()) continue;
        const auto port = it->second.port();
        if (ports_used.insert(port).second) {
            // Port is free to use.
            Logger::info("%s will run on port %d", app_directory.c_str(), port);

            // Register the app and its port as a service with the mDNS server.
            mdns_configuration->register_app(app_directory, port);
        } else {
            // Conflict!
            Logger::error(
                "%s wants to use port %d, which is already in use.",
                app_directory.c_str(), port
            );
            Logger::error(
                "App will be disabled. Assign it a different port in its "
                "config file!"
            );

            // Destroy the server so we don't start it, and set an error message
            // for the app in the landing page information object.
            servers.erase(it);
            app_info.error_message =
                "port " + std::to_string(port) + " already in use";
        }
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

/// mDNS server.
static std::unique_ptr<MdnsServerThread> mdns_server;

/// Landing page server.
static std::unique_ptr<LandingServerThread> landing_server;

/// Initializes the server thread. Must be called only when both sides of the
/// plugin have finished initializing.
static void server_init() {

    // Start HTTP server for each application.
    if (!servers.empty()) {
        Logger::info("Initializing %d app server thread(s)...", servers.size());
        for (auto &[_, server] : servers) {
            server.start();
        }
    }

    // Initialize and start mDNS server.
    Logger::info("Initializing mDNS server thread...");
    mdns_server = std::make_unique<MdnsServerThread>(*mdns_configuration);
    mdns_server->start();
    landing_info.local_ip_address = mdns_server->get_local_ip_address();
    landing_info.mdns_hostname = mdns_server->get_hostname();

    // Initialize and start landing server.
    // TODO: port should come from a config file, and be rejected for normal
    //  apps to keep it free.
    Logger::info("Initializing landing server thread...");
    landing_server = std::make_unique<LandingServerThread>(8000, landing_info);
    landing_server->start();

    Logger::info("Server thread initialization complete.");
}

/// Instructs the servers to fetch new data from the recorder.
static void server_update() {
    for (auto &[_, server] : servers) {
        server.update();
    }
}

/// Shuts down the server thread, if any, including waiting for it to join.
static void server_shutdown() {
    if (servers.empty()) return;
    Logger::info("Shutting down server(s)...");

    // Send stop signal to all server threads.
    for (auto &[_, server] : servers) {
        server.stop();
    }
    if (mdns_server) mdns_server->stop();
    if (landing_server) landing_server->stop();

    // Wait for all server threads to stop.
    for (auto &[_, server] : servers) {
        server.join();
    }
    if (mdns_server) mdns_server->join();
    if (landing_server) landing_server->join();

    // Destroy server thread managers.
    servers.clear();
    mdns_server.reset();
    landing_server.reset();

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
    Recorder::set_update_server_callback(server_update);
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
    for (auto &[_, server] : servers) {
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
        landing_info.game_version = init_params->common.game_name;
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
