#include "logger.h"

// Standard libraries.
#include <cstdarg>
#include <cstdlib>
#include <filesystem>

void Logger::flush_queue_unlocked() {
    while (!game_log_queue.empty()) {
        const auto &[severity, message] = game_log_queue.front();
        if (game_log_callback) {
            game_log_callback(severity, message.c_str());
        }
        game_log_queue.pop();
    }
}

void Logger::log(const scs_log_type_t severity, const std::string &message) {
    std::lock_guard guard(mutex);

    if (game_log_callback) {
        const auto prefixed = "[TruckTel] " + message;
        if (std::this_thread::get_id() == game_api_thread) {
            flush_queue_unlocked();
            game_log_callback(severity, prefixed.c_str());
        } else {
            game_log_queue.emplace(severity, prefixed);
        }
    }
    if (log_file.is_open() && log_file.good()) {
        switch (severity) {
            case SCS_LOG_TYPE_verbose:
                log_file << "VERB: ";
                break;
            case SCS_LOG_TYPE_message:
                log_file << "INFO: ";
                break;
            case SCS_LOG_TYPE_warning:
                log_file << "WARN: ";
                break;
            case SCS_LOG_TYPE_error:
                log_file << "ERR!: ";
                break;
            default:
                log_file << "????: ";
                break;
        }
        log_file << message << std::endl;
    }
}

void Logger::logf(const scs_log_type_t severity, const char *format, ...) {
    va_list args1;
    va_start(args1, format);
    va_list args2;
    va_copy(args2, args1);
    const auto size = vsnprintf(nullptr, 0, format, args1) + 1;
    va_end(args1);
    const auto buf = static_cast<char *>(malloc(size));
    if (!buf) {
        fprintf(stderr, "malloc for logging buffer failed\n");
        abort();
    }
    vsnprintf(buf, size, format, args2);
    va_end(args2);
    log(severity, buf);
    free(buf);
}

Logger::Logger(const scs_log_t game_log_callback)
    : game_log_callback(game_log_callback),
      game_api_thread(std::this_thread::get_id()) {
    // The working directory for ETS2 seems to be the directory its
    // executable is placed in, so the path below points to a file
    // in the plugin directory.
    const auto log_path =
        std::filesystem::current_path() / "plugins" / "trucktel.txt";

    // For some ungodly reason, std::filesystem::path::c_str() emits UTF16
    // on Windows. Avoid by converting to std::string first.
    const auto log_path_str = log_path.string();

    // Report where we're logging to for good measure.
    logf(SCS_LOG_TYPE_message, "Logging to %s", log_path_str.c_str());
    log_file.open(log_path_str.c_str());
}

std::unique_ptr<Logger> Logger::instance{};

void Logger::init(const scs_log_t game_log_callback) {
    if (instance) throw std::runtime_error("can only have one logger at once");
    instance.reset(new Logger(game_log_callback));
}

void Logger::shutdown() {
    instance.reset();
}

void Logger::periodic() {
    if (!instance) return;
    std::lock_guard guard(instance->mutex);
    instance->flush_queue_unlocked();
}
