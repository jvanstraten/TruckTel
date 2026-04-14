#pragma once

#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>

#include <scssdk_telemetry.h>

/// Singleton class for logging.
class Logger {
private:
    /// Verbose log messages only go to the log file, not to the SCS API.
    static constexpr scs_log_type_t SCS_LOG_TYPE_verbose = -1;

    /// Callback for logging to the game's console.
    const scs_log_t game_log_callback;

    /// The SCS telemetry API only allows calls to it from within the game
    /// thread, in response to functions from the game. Whether this actually
    /// also applies to logging is unclear, but we'll assume it does. The
    /// telemetry API also assures that it will only ever call the game from
    /// a single thread, so we can keep track of that thread's ID and call
    /// the game log function only when we're in that thread. Whenever we're
    /// in another thread, we still write to the log file directly, but queue
    /// up messages for the in-game log.
    std::thread::id game_api_thread;

    /// Queue for log messages pending transmission to the game's console.
    std::queue<std::pair<scs_log_type_t, std::string>> game_log_queue;

    /// Flushes any pending messages in game_log_queue to game_log_callback.
    /// The logging mutex must already be held.
    void flush_queue_unlocked();

    /// File pointer to a file that also stores all the log messages.
    std::ofstream log_file{};

    /// Mutex for log actions.
    std::mutex mutex;

    /// Logs a message without formatting.
    void log_raw(scs_log_type_t severity, const std::string &message);

    /// Logs a message with printf-style formatting.
    void log_formatted(scs_log_type_t severity, const char *format, ...);

    /// Constructs the logger.
    explicit Logger(scs_log_t game_log_callback);

    /// Instance of the logger, used by static methods.
    static std::unique_ptr<Logger> instance;

public:
    /// Call from the SCS API telemetry initialization hook to initialize the
    /// logging system.
    static void init(scs_log_t game_log_callback);

    /// Call from the SCS API telemetry shutdown hook to clean up the logging
    /// system.
    static void shutdown();

    /// Call periodically from SCS telemetry API callbacks to flush the log
    /// queue.
    static void periodic();

    /// Send a message with customizable severity.
    static void log(const scs_log_type_t verbosity, const char *message) {
        if (!instance) return;
        instance->log_raw(verbosity, message);
    }

    /// Send a verbose message (log file only) without formatting.
    static void verbose(const char *message) {
        if (!instance) return;
        instance->log_raw(SCS_LOG_TYPE_verbose, message);
    }

    /// Send an info message without formatting.
    static void info(const char *message) {
        if (!instance) return;
        instance->log_raw(SCS_LOG_TYPE_message, message);
    }

    /// Send a warning message without formatting.
    static void warn(const char *message) {
        if (!instance) return;
        instance->log_raw(SCS_LOG_TYPE_warning, message);
    }

    /// Send an error message without formatting.
    static void error(const char *message) {
        if (!instance) return;
        instance->log_raw(SCS_LOG_TYPE_error, message);
    }

    /// Send a message with formatting and customizable severity.
    template <typename... Args>
    static void log(
        const scs_log_type_t verbosity, const char *format, Args... args
    ) {
        if (!instance) return;
        instance->log_formatted(verbosity, format, args...);
    }

    /// Send a verbose message (log file only) with formatting.
    template <typename... Args>
    static void verbose(const char *format, Args... args) {
        if (!instance) return;
        instance->log_formatted(SCS_LOG_TYPE_verbose, format, args...);
    }

    /// Send an info message with formatting.
    template <typename... Args>
    static void info(const char *format, Args... args) {
        if (!instance) return;
        instance->log_formatted(SCS_LOG_TYPE_message, format, args...);
    }

    /// Send a warning message with formatting.
    template <typename... Args>
    static void warn(const char *format, Args... args) {
        if (!instance) return;
        instance->log_formatted(SCS_LOG_TYPE_warning, format, args...);
    }

    /// Send an error message with formatting.
    template <typename... Args>
    static void error(const char *format, Args... args) {
        if (!instance) return;
        instance->log_formatted(SCS_LOG_TYPE_error, format, args...);
    }
};
