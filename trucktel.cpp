#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <fstream>
#include <list>
#include <array>
#include <vector>
#include <mutex>

#include "scssdk_telemetry.h"
#include "common/scssdk_telemetry_common_channels.h"
#include "common/scssdk_telemetry_job_common_channels.h"
#include "common/scssdk_telemetry_trailer_common_channels.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "nlohmann/json.hpp"

/// Verbose log messages only go to the log file, not to the SCS API.
static constexpr scs_log_type_t SCS_LOG_TYPE_verbose = -1;

/// Maximum number of wheels for a truck or trailer queried.
static constexpr uint32_t MAX_WHEELS = 14;

/// Singleton class for logging.
class Logger {
private:
    /// Callback for logging to the game's console.
    const scs_log_t game_log_callback;

    /// File pointer to a file that also stores all the log messages.
    std::ofstream log_file{};

    /// Buffer used for formatting log messages.
    char *buf = nullptr;

    /// Allocated size of the buffer used for printing, including null
    /// terminator.
    int buf_size = 0;

public:
    /// Constructs the logger.
    explicit Logger(const scs_log_t game_log_callback) : game_log_callback(game_log_callback) {
        // The working directory for ETS2 seems to be the directory its
        // executable is placed in, so the path below points to a file
        // in the plugin directory.
        const auto log_path = std::filesystem::current_path() / "plugins" / "trucktel.txt";

        // For some ungodly reason, std::filesystem::path::c_str() emits UTF16
        // on Windows. Avoid by converting to std::string first.
        const auto log_path_str = log_path.string();

        // Report where we're logging to for good measure.
        logf(SCS_LOG_TYPE_message, "TruckTel is logging to %s", log_path_str.c_str());
        log_file.open(log_path_str.c_str());
    }

    /// Destroys the logger.
    ~Logger() {
        if (buf) free(buf);
    }

    /// Logs a message without formatting.
    void log(const scs_log_type_t severity, const char *message) {
        if (game_log_callback) {
            game_log_callback(severity, message);
        }
        if (log_file.is_open() && log_file.good()) {
            switch (severity) {
                case SCS_LOG_TYPE_verbose: log_file << "VERB: "; break;
                case SCS_LOG_TYPE_message: log_file << "INFO: "; break;
                case SCS_LOG_TYPE_warning: log_file << "WARN: "; break;
                case SCS_LOG_TYPE_error: log_file << "ERR!: "; break;
                default: log_file << "????: "; break;
            }
            log_file << message << std::endl;
        }
    }

    /// Logs a message with printf-style formatting.
    void logf(const scs_log_type_t severity, const char *format, ...) {
        va_list args1;
        va_start(args1, format);
        va_list args2;
        va_copy(args2, args1);
        const auto size = vsnprintf(buf, buf_size, format, args1) + 1;
        va_end(args1);
        if (size > buf_size) {
            free(buf);
            buf = static_cast<char *>(malloc(size));
            if (!buf) {
                fprintf(stderr, "malloc for logging buffer failed\n");
                abort();
            }
            buf_size = size;
            vsnprintf(buf, buf_size, format, args2);
        }
        va_end(args2);
        log(severity, buf);
    }
};

/// Singleton instance of the logger.
std::unique_ptr<Logger> logger;

// Convenience macros for logging messages. These are nop when no logger is
// present.
#define VERBOSE(msg) do { if (logger) { logger->log(SCS_LOG_TYPE_verbose, msg); } } while (false)
#define VERBOSEF(msg, ...) do { if (logger) { logger->logf(SCS_LOG_TYPE_verbose, msg, __VA_ARGS__); } } while (false)
#define INFO(msg) do { if (logger) { logger->log(SCS_LOG_TYPE_message, msg); } } while (false)
#define INFOF(msg, ...) do { if (logger) { logger->logf(SCS_LOG_TYPE_message, msg, __VA_ARGS__); } } while (false)
#define WARN(msg) do { if (logger) { logger->log(SCS_LOG_TYPE_warning, msg); } } while (false)
#define WARNF(msg, ...) do { if (logger) { logger->logf(SCS_LOG_TYPE_warning, msg, __VA_ARGS__); } } while (false)
#define ERROR(msg) do { if (logger) { logger->log(SCS_LOG_TYPE_error, msg); } } while (false)
#define ERRORF(msg, ...) do { if (logger) { logger->logf(SCS_LOG_TYPE_error, msg, __VA_ARGS__); } } while (false)

/// Assigns data to the hierarchical key identified by path in json. The
/// path separator is a period. This tries to be smarter than it really
/// should be, because the paths used by the SCS API are not uniform enough to
/// work with a naive implementation: non-leaf paths can have data in them.
/// Whenever that happens, the data of a non-leaf node is put in a node with
/// key "_".
void json_assign_path(nlohmann::json &json, const std::string &path, const nlohmann::json &data) {
    size_t start = 0;
    auto json_ptr = &json;
    while (true) {
        // If there was already a nontrivial and non-dict value for the level
        // we're about to index, punt it to a "_" key wrapped inside a
        // surrounding object. For example, if "cargo" already exists, and
        // we're trying to set "cargo.mass", the "cargo" key will become
        // "cargo._".
        if (!json_ptr->is_object() && !json_ptr->is_null()) {
            *json_ptr = {{"_", *json_ptr}};
        }

        // See if there is another separator.
        const auto pos = path.find('.', start);
        const auto element = path.substr(start, pos - start);
        if (pos == std::string::npos) {

            // This is the last path element. If the path already exists and
            // is an object, put the data in a _ key. This is the complement
            // of the above: if "cargo.mass" already exists, and we're trying
            // to set "cargo", the "cargo" key will become "cargo._".
            if ((*json_ptr)[element].is_object()) {
                (*json_ptr)[element]["_"] = data;
            } else {
                (*json_ptr)[element] = data;
            }
            return;
        }

        // Index the next path element in our JSON structure and continue
        // with the remainder of the path.
        json_ptr = &(*json_ptr)[element];
        start = pos + 1;
    }
}

/// Converts an SCS version number to a JSON array.
nlohmann::json scs_version_to_json(const scs_u32_t version) {
    return {SCS_GET_MAJOR_VERSION(version), SCS_GET_MINOR_VERSION(version)};
}

/// Converts an scs_value_t variant into an equivalent JSON form.
nlohmann::json scs_value_to_json(const scs_value_t &value) {
    switch (value.type) {
        case SCS_VALUE_TYPE_bool:
            return value.value_bool.value ? true : false;
        case SCS_VALUE_TYPE_s32:
            return value.value_s32.value;
        case SCS_VALUE_TYPE_s64:
            return value.value_s64.value;
        case SCS_VALUE_TYPE_u32:
            return value.value_u32.value;
        case SCS_VALUE_TYPE_u64:
            return value.value_u64.value;
        case SCS_VALUE_TYPE_float:
            return value.value_float.value;
        case SCS_VALUE_TYPE_double:
            return value.value_double.value;
        case SCS_VALUE_TYPE_fvector:
            return {
                {"x", value.value_fvector.x},
                {"y", value.value_fvector.y},
                {"z", value.value_fvector.z}
            };
        case SCS_VALUE_TYPE_dvector:
            return {
                {"x", value.value_dvector.x},
                {"y", value.value_dvector.y},
                {"z", value.value_dvector.z}
            };
        case SCS_VALUE_TYPE_euler:
            return {
                {"heading", value.value_euler.heading},
                {"pitch", value.value_euler.pitch},
                {"roll", value.value_euler.roll}
            };
        case SCS_VALUE_TYPE_fplacement:
            return {
                {"x", value.value_fplacement.position.x},
                {"y", value.value_fplacement.position.y},
                {"z", value.value_fplacement.position.z},
                {"heading", value.value_fplacement.orientation.heading},
                {"pitch", value.value_fplacement.orientation.pitch},
                {"roll", value.value_fplacement.orientation.roll}
            };
        case SCS_VALUE_TYPE_dplacement:
            return {
                {"x", value.value_dplacement.position.x},
                {"y", value.value_dplacement.position.y},
                {"z", value.value_dplacement.position.z},
                {"heading", value.value_dplacement.orientation.heading},
                {"pitch", value.value_dplacement.orientation.pitch},
                {"roll", value.value_dplacement.orientation.roll}
            };
        case SCS_VALUE_TYPE_string:
            return value.value_string.value;
        default:
            return "unknown type " + std::to_string(value.type);
    }
}

/// Converts an array of named attributes to a JSON structure.
nlohmann::json scs_attributes_to_json(const scs_named_value_t *attributes) {
    if (!attributes) return {};
    nlohmann::json json{};
    while (attributes->name) {
        const auto value = scs_value_to_json(attributes->value);
        auto key = std::string(attributes->name);
        if (attributes->index != SCS_U32_NIL) {
            key += "." + std::to_string(attributes->index);
        }
        //INFO("attribute %s = %s", key.c_str(), value.dump().c_str());
        json_assign_path(json, key, value);
        ++attributes;
    }
    return json;
}

/// Recorder for a stream of events that should be delivered to clients
/// completely but without duplicates.
class EventRecorder {
private:
    /// Structure for individual events in the queue.
    struct Event {
        /// Unique ID of this event.
        uint64_t id;

        /// Timestamp that this event was generated.
        std::chrono::system_clock::time_point timestamp;

        /// JSON-based data identifying the event.
        nlohmann::json data;
    };

    /// Event queue. Newer events are inserted at the end of the queue, and old
    /// events are pruned from the start of the queue.
    std::list<Event> events;

    /// Events are stored for at most this amount of time. In order to not drop
    /// events, a client must query the event recorder at least this often.
    /// Pruning only happens when a new event is posted by the SCS API, though.
    const std::chrono::system_clock::duration max_age;

    /// The next event ID.
    uint64_t id_counter = 0;

    /// The mutex that protects all of the above.
    std::mutex mutex;

public:
    explicit EventRecorder(const std::chrono::system_clock::duration max_age) : max_age(max_age) {
    }

    /// Initializes the next_id value for poll() when a client first connects.
    uint64_t poll_init() {
        std::lock_guard guard(mutex);
        return id_counter;
    }

    /// Polls for events. The client doing the polling should keep track of the
    /// next_id variable; it is used to avoid sending duplicates. next_id will
    /// be incremented by the number of events returned by the function if and
    /// only if no events were dropped; it will be incremented by N more than
    /// the number of events returned if N events were dropped. The result is a
    /// JSON array of events.
    nlohmann::json poll(uint64_t &next_id) {
        std::lock_guard guard(mutex);

        // Starting from the newest event, rewind the list until we find the
        // event with next_id or the start of the event list. The event that
        // came before that will already have been reported to this client.
        auto it = events.cend();
        while (it != events.cbegin()) {
            --it;
            if (it->id == next_id) break;
        }

        // Copy all the events that are new for this client into a JSON array.
        nlohmann::json result = nlohmann::json::array();
        for (; it != events.cend(); ++it) {
            result.emplace_back(it->data);
        }

        // Update next_id to identify the event that will come after the last
        // reported event.
        next_id = events.back().id + 1;
        return result;
    }

    /// Pushes an event into the queue. This first prunes old events to avoid
    /// the memory footprint from growing without bound.
    void push(nlohmann::json event) {
        std::lock_guard guard(mutex);

        // Perform pruning.
        const auto now = std::chrono::system_clock::now();
        const auto prune_before = now - max_age;
        while (!events.empty() && events.front().timestamp < prune_before) {
            events.pop_front();
        }

        // Push the event.
        events.emplace_back(id_counter, now, std::move(event));

        // Update the ID counter.
        id_counter++;
    }
};

/// Recorder for data that does not change often, and is reported to clients
/// only when it is updated. The basic data structure is a JSON map. Whenever
/// the SCS API side updates a key, a version ID is incremented to indicate
/// that the structure has changed.
class VersionedRecorder {
private:
    /// The current state of the data structure.
    nlohmann::json data;

    /// The current version.
    uint64_t current_version = 0;

    /// The mutex that protects all of the above.
    std::mutex mutex;

public:
    /// Polls for data. The client doing the polling should keep track of the
    /// version variable; it is used to avoid sending the structure when nothing
    /// has changed since the previous call. The result is the JSON structure
    /// associated with this recorder if anything has changed, or null if the
    /// data has not changed since the previous call.
    nlohmann::json poll(uint64_t &version) {
        std::lock_guard guard(mutex);
        if (version == current_version) return nullptr;
        version = current_version;
        return data;
    }

    /// Polls for data without a version check. This always returns a copy of the
    /// latest data.
    nlohmann::json poll() {
        std::lock_guard guard(mutex);
        return data;
    }

    /// Updates the given key of the toplevel JSON structure to the given value,
    /// while incrementing the version number. If value is null, the key is
    /// removed if it previously existed.
    void push(const std::string &key, nlohmann::json value) {
        std::lock_guard guard(mutex);

        // Update the data structure.
        if (value.is_null()) {
            data.erase(key);
        } else {
            data[key] = std::move(value);
        }

        // Increment the version number.
        current_version++;
    }
};

/// Metadata for channels.
struct ChannelMetadata {
    /// SCS API name for this channel.
    std::string scs_name;

    /// SCS API index for this channel.
    scs_u32_t scs_index;

    /// SCS API data type for this channel.
    scs_value_type_t scs_type;

    /// Corresponding JSON path from Funbit's ets2-telemetry-server for
    /// compatibility mode. If a channel is not present there, this is
    /// empty.
    std::string funbit_name = "";
};

/// Double-buffered recorder for channel data.
class ChannelRecorder {
private:
    /// Metadata for the channels. This must not change after plugin
    /// initialization. That way, locking can be avoided.
    std::vector<ChannelMetadata> channel_metadata = {};

    /// The buffers. Which buffer is which is determined by front.
    std::array<std::vector<scs_value_t>, 2> buffers = {};

    /// The current index of the front buffer (client side). The back buffer
    /// (what the SCS API side is writing to) has index 1-front.
    size_t front = 0;

    /// Whether the game is paused.
    bool paused = true;

    /// The mutex that protects the front buffer and the index thereof. The
    /// back buffer is NOT protected, and must only be accessed by the SCS API
    /// side.
    std::mutex mutex;

    // Indices of pseudochannels.
    size_t idx_render_time;
    size_t idx_simulation_time;
    size_t idx_paused_simulation_time;
    size_t idx_paused;

public:
    /// Registers a channel. This must only be done during initialization,
    /// before any other calls are made from any thread. The return value is
    /// the index of the new channel.
    size_t register_channel(ChannelMetadata metadata) {
        const auto index = channel_metadata.size();
        channel_metadata.emplace_back(std::move(metadata));
        return index;
    }

    /// Creates a ChannelRecorder.
    ChannelRecorder() {
        // Register "pseudochannels" for the information carried with the
        // start-frame event.
        idx_render_time = register_channel({"frame.render_time", SCS_U32_NIL, SCS_VALUE_TYPE_u64, ""});
        idx_simulation_time = register_channel({"frame.simulation_time", SCS_U32_NIL, SCS_VALUE_TYPE_u64, ""});
        idx_paused_simulation_time = register_channel({"frame.paused_simulation_time", SCS_U32_NIL, SCS_VALUE_TYPE_u64, ""});
        idx_paused = register_channel({"frame.paused", SCS_U32_NIL, SCS_VALUE_TYPE_bool, "game.paused"});
    }

    /// Sets the game simulation state to paused.
    void pause() {
        paused = true;
    }

    /// Sets the game simulation state to unpaused.
    void unpause() {
        paused = false;
    }

    /// Starts a frame.
    void start(const scs_telemetry_frame_start_t &info) {
        // Clear all items in the frame.
        for (auto &item : buffers[1 - front]) {
            item.type = SCS_VALUE_TYPE_INVALID;
        }

        // Sets frame information.
        scs_value_t val = {};
        val.type = SCS_VALUE_TYPE_u64;
        val.value_u64.value = info.render_time;
        push(idx_render_time, val);
        val.value_u64.value = info.simulation_time;
        push(idx_simulation_time, val);
        val.value_u64.value = info.paused_simulation_time;
        push(idx_paused_simulation_time, val);
        val.type = SCS_VALUE_TYPE_bool;
        val.value_bool.value = paused;
        push(idx_paused, val);
    }

    /// Updates the given key of back buffer to the given value.
    void push(const size_t index, const scs_value_t &value) {
        auto &buffer = buffers[1 - front];
        while (index >= buffer.size()) {
            buffer.emplace_back();
            buffer.back().type = SCS_VALUE_TYPE_INVALID;
        }
        buffer[index] = value;
    }

    /// Flips the buffers. To be called by the SCS API side when a frame is
    /// complete. The type codes of all values are set to invalid in the new
    /// buffer, to indicate that they have not been set.
    void end() {
        std::lock_guard guard(mutex);
        front = 1 - front;
    }

    /// Returns const access to the channel metadata structure.
    [[nodiscard]] const std::vector<ChannelMetadata> &channels() const {
        return channel_metadata;
    }

    /// Returns a copy of the most recent frame. The indices in the returned
    /// array correspond to those returned by channels(), though the returned
    /// array may be smaller than the array returned by channels(). Channels
    /// that did not receive a value in the most recent frame will have their
    /// type code set to SCS_VALUE_TYPE_INVALID and must be ignored by the
    /// caller.
    std::vector<scs_value_t> poll() {
        std::lock_guard guard(mutex);
        return buffers[front];
    }

    /// Generates JSON data of everything in the most recent frame.
    nlohmann::json poll_json_scs() {
        nlohmann::json json_data{};
        const auto raw_data = poll();
        for (size_t i = 0; i < raw_data.size() && i < channel_metadata.size(); i++) {
            if (raw_data[i].type == SCS_VALUE_TYPE_INVALID) continue;
            const auto value = scs_value_to_json(raw_data[i]);
            const auto &metadata = channel_metadata[i];
            auto path = metadata.scs_name;
            if (metadata.scs_index != SCS_U32_NIL) {
                path += "." + std::to_string(metadata.scs_index);
            }
            json_assign_path(json_data, path, value);
        }
        return json_data;
    }

    /// Generates JSON data in roughly the format used by Funbit's telemetry
    /// server. This doesn't include static config data yet, and some post-
    /// conversions are necessary for compatibility.
    nlohmann::json poll_json_funbit() {
        nlohmann::json json_data{};
        const auto raw_data = poll();
        for (size_t i = 0; i < raw_data.size() && i < channel_metadata.size(); i++) {
            if (raw_data[i].type == SCS_VALUE_TYPE_INVALID) continue;
            const auto &metadata = channel_metadata[i];
            if (metadata.funbit_name.empty()) continue;
            const auto value = scs_value_to_json(raw_data[i]);
            json_assign_path(json_data, metadata.funbit_name, value);
        }
        return json_data;
    }

};

/// Singleton class that records data received from the SCS API via events and
/// channels and provides asynchronous and thread-safe access to this data.
class Recorder {
public:
    /// Basic game and API information.
    VersionedRecorder game;

    /// Game configuration data as received via the configuration event.
    VersionedRecorder config;

    /// Gameplay event recorder. This also records paused/started events.
    EventRecorder gameplay;

    /// Frame data from channels.
    ChannelRecorder frame;

    /// Constructor.
    Recorder() : gameplay(std::chrono::seconds(10)) {
    }
};

/// Singleton instance of the recorder.
std::unique_ptr<Recorder> recorder;

/// Exception catcher for all calls from ETS2/ATS.
#define API_CATCH(body) { \
    try { \
        body \
    } catch (std::exception &e) { \
        ERRORF("exception: %s", e.what()); \
    } catch (...) { \
        ERROR("bad exception"); \
    } \
}

/// Exception catcher for all calls from ETS2/ATS.
#define API_CATCH_RESULT(body) { \
    try { \
        body \
    } catch (std::exception &e) { \
        ERRORF("exception: %s", e.what()); \
        return SCS_RESULT_generic_error; \
    } catch (...) { \
        ERROR("bad exception"); \
        return SCS_RESULT_generic_error; \
    } \
}

/// Frame start event handler.
SCSAPI_VOID scs_frame_start(const scs_event_t event, const void *const event_info, const scs_context_t context) API_CATCH (
    if (!recorder) return;
    if (!event_info) return;
    recorder->frame.start(*static_cast<const scs_telemetry_frame_start_t*>(event_info));
)

/// Frame end event handler.
SCSAPI_VOID scs_frame_end(const scs_event_t event, const void *const event_info, const scs_context_t context) API_CATCH (
    if (!recorder) return;
    recorder->frame.end();
)

/// Pause event handler.
SCSAPI_VOID scs_paused(const scs_event_t event, const void *const event_info, const scs_context_t context) API_CATCH (
    if (!recorder) return;
    recorder->frame.pause();
    recorder->gameplay.push({{"_", "paused"}});

    VERBOSE("Simulation paused!");
    VERBOSEF("Last frame before pause: %s", recorder->frame.poll_json_scs().dump().c_str());
    VERBOSEF("Configuration: %s", recorder->config.poll().dump().c_str());
)

/// Unpause event handler.
SCSAPI_VOID scs_started(const scs_event_t event, const void *const event_info, const scs_context_t context) API_CATCH (
    if (!recorder) return;
    recorder->frame.unpause();
    recorder->gameplay.push({{"_", "started"}});

    VERBOSE("Simulation started!");
)

/// Configuration event handler.
SCSAPI_VOID scs_configuration(const scs_event_t event, const void *const event_info, const scs_context_t context) API_CATCH (
    if (!recorder) return;
    if (!event_info) return;
    const auto scs_data = static_cast<const scs_telemetry_configuration_t*>(event_info);
    nlohmann::json json_data = scs_attributes_to_json(scs_data->attributes);
    recorder->config.push(scs_data->id, std::move(json_data));
)

/// Gameplay event handler.
SCSAPI_VOID scs_gameplay(const scs_event_t event, const void *const event_info, const scs_context_t context) API_CATCH (
    if (!recorder) return;
    if (!event_info) return;
    const auto scs_data = static_cast<const scs_telemetry_gameplay_event_t*>(event_info);
    nlohmann::json json_data = scs_attributes_to_json(scs_data->attributes);
    json_data["_"] = scs_data->id;
    VERBOSEF("Gameplay event: %s", json_data.dump().c_str());
    recorder->gameplay.push(std::move(json_data));
)

/// Channel data handler. The context value is used to keep track of the raw
/// channel indices, so we don't need to spend time messing with strings for
/// basic recording of values.
SCSAPI_VOID scs_channel(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context) API_CATCH (
    if (!recorder) return;
    const auto channel_index = reinterpret_cast<uintptr_t>(context);
    scs_value_t value_copy {};
    if (value) {
        value_copy = *value;
    } else {
        value_copy.type = SCS_VALUE_TYPE_INVALID;
    }
    recorder->frame.push(channel_index, value_copy);
)

/// Registers an event handler, warning if this failed.
void register_event_handler(
    const scs_telemetry_init_params_v101_t *init_params,
    const scs_event_t event,
    const scs_telemetry_event_callback_t callback
) {
    const auto result = init_params->register_for_event(event, callback, nullptr);
    if (result != SCS_RESULT_ok) {
        WARNF("failed to register event %d: code %d", event, result);
    }
}

/// Registers a channel handler, warning if this failed.
void register_channel_handler(
    const scs_telemetry_init_params_v101_t *init_params,
    const ChannelMetadata &metadata
) {
    if (!recorder) return;
    const auto channel_index = recorder->frame.register_channel(metadata);
    const auto context = reinterpret_cast<scs_context_t>(channel_index);
    const auto result = init_params->register_for_channel(
        metadata.scs_name.c_str(),
        metadata.scs_index,
        metadata.scs_type,
        SCS_TELEMETRY_CHANNEL_FLAG_each_frame,
        scs_channel,
        context
    );
    if (result != SCS_RESULT_ok) {
        auto name = metadata.scs_name;
        if (metadata.scs_index != SCS_U32_NIL) {
            name += "[" + std::to_string(metadata.scs_index) + "]";
        }
        WARNF("failed to register channel %s (type %d): code %d", name.c_str(), metadata.scs_type, result);
    }
}

/// Registers a channel handler, warning if this failed.
void register_trailer_handler(
    const scs_telemetry_init_params_v101_t *init_params,
    ChannelMetadata metadata
) {
    register_channel_handler(init_params, metadata);

    // Support multiple trailers.
    metadata.funbit_name.clear();
    if (!metadata.scs_name.starts_with("trailer.")) return;
    const auto remainder = metadata.scs_name.substr(7); // including period
    for (uint32_t trailer_index = 0; trailer_index < SCS_TELEMETRY_trailers_count; trailer_index++) {
        metadata.scs_name = "trailer." + std::to_string(trailer_index) + remainder;
        register_channel_handler(init_params, metadata);
    }
}

/// SCS API initialization callback. This is the entry point that ETS2/ATS
/// calls.
SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version,
    const scs_telemetry_init_params_t *const params
) API_CATCH_RESULT (
    // Check input. In particular, initialize only for the latest version of
    // the API at the time of writing.
    if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
    if (!params) return SCS_RESULT_invalid_parameter;
    const auto *init_params = reinterpret_cast<const scs_telemetry_init_params_v101_t *>(params);
    logger = std::make_unique<Logger>(init_params->common.log);
    recorder = std::make_unique<Recorder>();

    // Push basic game information.
    recorder->game.push("game_id", init_params->common.game_id);
    recorder->game.push("game_name", init_params->common.game_name);
    recorder->game.push("game_version", scs_version_to_json(init_params->common.game_version));
    recorder->game.push("api_version", scs_version_to_json(version));

    // Register event handlers.
    register_event_handler(init_params, SCS_TELEMETRY_EVENT_frame_start, scs_frame_start);
    register_event_handler(init_params, SCS_TELEMETRY_EVENT_frame_end, scs_frame_end);
    register_event_handler(init_params, SCS_TELEMETRY_EVENT_paused, scs_paused);
    register_event_handler(init_params, SCS_TELEMETRY_EVENT_started, scs_started);
    register_event_handler(init_params, SCS_TELEMETRY_EVENT_configuration, scs_configuration);
    register_event_handler(init_params, SCS_TELEMETRY_EVENT_gameplay, scs_gameplay);

    // Register common channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_CHANNEL_local_scale, SCS_U32_NIL, SCS_VALUE_TYPE_float, "game.timeScale"});
    register_channel_handler(init_params, {SCS_TELEMETRY_CHANNEL_game_time, SCS_U32_NIL, SCS_VALUE_TYPE_u32, "game.time"}); // TODO convert date time
    register_channel_handler(init_params, {SCS_TELEMETRY_CHANNEL_multiplayer_time_offset, SCS_U32_NIL, SCS_VALUE_TYPE_s32});
    register_channel_handler(init_params, {SCS_TELEMETRY_CHANNEL_next_rest_stop, SCS_U32_NIL, SCS_VALUE_TYPE_s32, "game.nextRestStopTime"}); // TODO convert date time

    // Register job channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_JOB_CHANNEL_cargo_damage, SCS_U32_NIL, SCS_VALUE_TYPE_float});

    // Register truck movement channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_world_placement, SCS_U32_NIL, SCS_VALUE_TYPE_dplacement, "truck.placement"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_local_linear_velocity, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_local_angular_velocity, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_local_linear_acceleration, SCS_U32_NIL, SCS_VALUE_TYPE_fvector, "truck.acceleration"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_local_angular_acceleration, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_cabin_offset, SCS_U32_NIL, SCS_VALUE_TYPE_fplacement, "truck.cabin"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_cabin_angular_velocity, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_cabin_angular_acceleration, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_head_offset, SCS_U32_NIL, SCS_VALUE_TYPE_fplacement, "truck.head"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.speed"});

    // Register truck powertrain channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.engineRpm"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear, SCS_U32_NIL, SCS_VALUE_TYPE_s32, "truck.gear"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_displayed_gear, SCS_U32_NIL, SCS_VALUE_TYPE_s32, "truck.displayedGear"});

    // Register truck driving channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_input_steering, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.userSteer"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_input_throttle, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.userThrottle"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_input_brake, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.userBrake"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_input_clutch, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.userClutch"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.gameSteer"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_effective_throttle, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.gameThrottle"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_effective_brake, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.gameBrake"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_effective_clutch, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.gameClutch"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_cruise_control, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.cruiseControlSpeed"}); // TODO also convert to boolean

    // Register truck gearbox channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_hshifter_slot, SCS_U32_NIL, SCS_VALUE_TYPE_u32, "truck.shifterSlot"});
    // not implemented: SCS_TELEMETRY_TRUCK_CHANNEL_hshifter_selector

    // Register truck braking channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.parkBrakeOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_motor_brake, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.motorBrakeOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_retarder_level, SCS_U32_NIL, SCS_VALUE_TYPE_u32, "truck.retarderBrake"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.airPressure"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.airPressureWarningOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure_emergency, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.airPressureEmergencyOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_brake_temperature, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.brakeTemperature"});

    // Register truck consumable channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_fuel, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.fuel"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.fuelWarningOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_average_consumption, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.fuelAverageConsumption"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_range, SCS_U32_NIL, SCS_VALUE_TYPE_float});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_adblue, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.adblue"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_adblue_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.adblueWarningOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_adblue_average_consumption, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.adblueAverageConsumpton"});

    // Register truck oil channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_oil_pressure, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.oilPressure"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_oil_pressure_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.oilPressureWarningOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_oil_temperature, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.oilTemperature"});

    // Register truck cooling channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_water_temperature, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.waterTemperature"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_water_temperature_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.waterTemperatureWarningOn"});

    // Register truck battery channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_battery_voltage, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.batteryVoltage"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_battery_voltage_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.batteryVoltageWarningOn"});

    // Register enabled state of various truck elements.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_electric_enabled, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.electricOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_engine_enabled, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.engineOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_lblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.blinkerLeftActive"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_rblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.blinkerRightActive"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_hazard_warning, SCS_U32_NIL, SCS_VALUE_TYPE_bool});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_lblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.blinkerLeftOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_rblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.blinkerRightOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_parking, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.lightsParkingOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_low_beam, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.lightsBeamLowOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_high_beam, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.lightsBeamHighOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_aux_front, SCS_U32_NIL, SCS_VALUE_TYPE_u32, "truck.lightsAuxFrontOn"}); // TODO convert to boolean
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_aux_roof, SCS_U32_NIL, SCS_VALUE_TYPE_u32, "truck.lightsAuxRoofOn"}); // TODO convert to boolean
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_beacon, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.lightsBeaconOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_brake, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.lightsBrakeOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_light_reverse, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.lightsReverseOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wipers, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "truck.wipersOn"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_dashboard_backlight, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.lightsDashboardValue"}); // TODO also convert to boolean
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_differential_lock, SCS_U32_NIL, SCS_VALUE_TYPE_bool});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_lift_axle, SCS_U32_NIL, SCS_VALUE_TYPE_bool});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_lift_axle_indicator, SCS_U32_NIL, SCS_VALUE_TYPE_bool});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_trailer_lift_axle, SCS_U32_NIL, SCS_VALUE_TYPE_bool});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_trailer_lift_axle_indicator, SCS_U32_NIL, SCS_VALUE_TYPE_bool});

    // Register truck wear information channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wear_engine, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.wearEngine"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wear_transmission, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.wearTransmission"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wear_cabin, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.wearCabin"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wear_chassis, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.wearChassis"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wear_wheels, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.wearWheels"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_odometer, SCS_U32_NIL, SCS_VALUE_TYPE_float, "truck.odometer"});

    // Register navigation channels.
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance, SCS_U32_NIL, SCS_VALUE_TYPE_float, "navigation.estimatedDistance"});
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_time, SCS_U32_NIL, SCS_VALUE_TYPE_float, "navigation.estimatedTime"}); // TODO convert date time
    register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_speed_limit, SCS_U32_NIL, SCS_VALUE_TYPE_float, "navigation.speedLimit"});

    // Register wheel channels.
    for (uint32_t wheel_index = 0; wheel_index < MAX_WHEELS; wheel_index++) {
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_susp_deflection, wheel_index, SCS_VALUE_TYPE_float});
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_on_ground, wheel_index, SCS_VALUE_TYPE_bool});
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_substance, wheel_index, SCS_VALUE_TYPE_u32});
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_velocity, wheel_index, SCS_VALUE_TYPE_float});
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_steering, wheel_index, SCS_VALUE_TYPE_float});
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_rotation, wheel_index, SCS_VALUE_TYPE_float});
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_lift, wheel_index, SCS_VALUE_TYPE_float});
        register_channel_handler(init_params, {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_lift_offset, wheel_index, SCS_VALUE_TYPE_float});
    }

    // Register trailer channels.
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_connected, SCS_U32_NIL, SCS_VALUE_TYPE_bool, "trailer.attached"});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_cargo_damage, SCS_U32_NIL, SCS_VALUE_TYPE_float});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_world_placement, SCS_U32_NIL, SCS_VALUE_TYPE_dplacement, "trailer.placement"});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_local_linear_velocity, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_local_angular_velocity, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_local_linear_acceleration, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_local_angular_acceleration, SCS_U32_NIL, SCS_VALUE_TYPE_fvector});

    // Register trailer wear information channels.
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wear_body, SCS_U32_NIL, SCS_VALUE_TYPE_float, "trailer.wear"});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wear_chassis, SCS_U32_NIL, SCS_VALUE_TYPE_float});
    register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wear_wheels, SCS_U32_NIL, SCS_VALUE_TYPE_float});

    // Register trailer wheel channels.
    for (uint32_t wheel_index = 0; wheel_index < MAX_WHEELS; wheel_index++) {
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_susp_deflection, wheel_index, SCS_VALUE_TYPE_float});
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_on_ground, wheel_index, SCS_VALUE_TYPE_bool});
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_substance, wheel_index, SCS_VALUE_TYPE_u32});
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_velocity, wheel_index, SCS_VALUE_TYPE_float});
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_steering, wheel_index, SCS_VALUE_TYPE_float});
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_rotation, wheel_index, SCS_VALUE_TYPE_float});
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_lift, wheel_index, SCS_VALUE_TYPE_float});
        register_trailer_handler(init_params, {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_lift_offset, wheel_index, SCS_VALUE_TYPE_float});
    }

    INFO("TruckTel loaded");
    return SCS_RESULT_ok;
)

/// SCS API cleanup handler.
SCSAPI_VOID scs_telemetry_shutdown() API_CATCH (
    INFO("TruckTel shutdown");
    logger.reset();
    recorder.reset();
)
