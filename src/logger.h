#pragma once

// Standard libraries.
#include <fstream>
#include <memory>
#include <string>

// Dependencies.
#include <scssdk_telemetry.h>

/// Singleton class for logging.
class Logger {
private:
    /// Verbose log messages only go to the log file, not to the SCS API.
    static constexpr scs_log_type_t SCS_LOG_TYPE_verbose = -1;

    /// Callback for logging to the game's console.
    const scs_log_t game_log_callback;

    /// File pointer to a file that also stores all the log messages.
    std::ofstream log_file{};

    /// Instance of the logger, used by static methods.
    static std::unique_ptr<Logger> instance;

    /// Logs a message without formatting.
    void log(scs_log_type_t severity, const std::string &message);

    /// Logs a message with printf-style formatting.
    void logf(scs_log_type_t severity, const char *format, ...);

    /// Constructs the logger.
    explicit Logger(scs_log_t game_log_callback);

public:
    /// Call from the SCS API telemetry initialization hook to initialize the
    /// logging system.
    static void init(scs_log_t game_log_callback);

    /// Call from the SCS API telemetry shutdown hook to clean up the logging
    /// system.
    static void shutdown();

    /// Send a verbose message (log file only) without formatting.
    static void verbose(const char *msg) {
        if (!instance) return;
        instance->log(SCS_LOG_TYPE_verbose, msg);
    }

    /// Send an info message without formatting.
    static void info(const char *msg) {
        if (!instance) return;
        instance->log(SCS_LOG_TYPE_message, msg);
    }

    /// Send a warning message without formatting.
    static void warn(const char *msg) {
        if (!instance) return;
        instance->log(SCS_LOG_TYPE_warning, msg);
    }

    /// Send an error message without formatting.
    static void error(const char *msg) {
        if (!instance) return;
        instance->log(SCS_LOG_TYPE_error, msg);
    }

    /// Send a verbose message (log file only) with formatting.
    template<typename... Args>
    static void verbose(const char *format, Args... args) {
        if (!instance) return;
        instance->logf(SCS_LOG_TYPE_verbose, format, args...);
    }

    /// Send an info message with formatting.
    template<typename... Args>
    static void info(const char *format, Args... args) {
        if (!instance) return;
        instance->logf(SCS_LOG_TYPE_message, format, args...);
    }

    /// Send a warning message with formatting.
    template<typename... Args>
    static void warn(const char *format, Args... args) {
        if (!instance) return;
        instance->logf(SCS_LOG_TYPE_warning, format, args...);
    }

    /// Send an error message with formatting.
    template<typename... Args>
    static void error(const char *format, Args... args) {
        if (!instance) return;
        instance->logf(SCS_LOG_TYPE_error, format, args...);
    }
};
