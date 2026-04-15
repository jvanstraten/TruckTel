#include "logger.h"

#include <cstdarg>
#include <cstdlib>

void Logger::flush_queue_unlocked() {
    while (!game_log_queue.empty()) {
        const auto &[severity, message] = game_log_queue.front();
        if (game_log_callback) {
            game_log_callback(severity, message.c_str());
        }
        game_log_queue.pop();
    }
}

void Logger::log_raw(
    const scs_log_type_t severity, const std::string &message
) {
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

void Logger::log_formatted(
    const scs_log_type_t severity, const char *format, ...
) {
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
    log_raw(severity, buf);
    free(buf);
}

Logger::Logger(const scs_log_t game_log_callback)
    : game_log_callback(game_log_callback),
      game_api_thread(std::this_thread::get_id()) {}

std::unique_ptr<Logger> Logger::instance{};

void Logger::init(const scs_log_t game_log_callback) {
    if (instance) throw std::runtime_error("can only have one logger at once");
    instance.reset(new Logger(game_log_callback));
}

void Logger::set_file(const std::string &path) {
    info("Logging to %s", path.c_str());
    instance->log_file.open(path.c_str());
}

void Logger::shutdown() {
    instance.reset();
}

void Logger::periodic() {
    if (!instance) return;
    std::lock_guard guard(instance->mutex);
    instance->flush_queue_unlocked();
}
