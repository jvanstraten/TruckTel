#pragma once

#include <functional>
#include <memory>

#include <scssdk_telemetry.h>

#include "channel.h"
#include "configuration.h"
#include "event.h"

/// Singleton class that records data received from the SCS API via events and
/// channels and provides asynchronous and thread-safe access to this data.
class Recorder {
private:
    /// Initialization parameters passed to the plugin initialization function
    /// by the game.
    const scs_telemetry_init_params_v101_t *const init_params;

    /// Game configuration data as received via the configuration event.
    ConfigurationRecorder configuration;

    /// Gameplay event recorder. This also records paused/started events.
    EventRecorder gameplay;

    /// Frame data from channels.
    ChannelRecorder channels;

    /// Function to call whenever data changes to instruct the server to poll
    /// from us.
    std::function<void()> update_server;

    /// Current recorder instance used by the static callbacks from the SCS
    /// API.
    static std::unique_ptr<Recorder> instance;

    /// Event handler.
    static SCSAPI_VOID scs_event(
        scs_event_t event, const void *event_info, scs_context_t context
    );

    /// Channel data handler. The context value is used to keep track of the raw
    /// channel indices, so we don't need to spend time messing with strings for
    /// basic recording of values.
    static SCSAPI_VOID scs_channel(
        scs_string_t name,
        scs_u32_t index,
        const scs_value_t *value,
        scs_context_t context
    );

    /// Records a generic event.
    void record_event(scs_event_t event, const void *event_info);

    /// Records a configuration change.
    void record_configuration(const scs_telemetry_configuration_t *scs_data);

    /// Records a gameplay event.
    void record_gameplay_event(const scs_telemetry_gameplay_event_t *scs_data);

    /// Records channel data.
    void record_channel(size_t channel_index, const scs_value_t *value);

    /// Registers an event handler, warning if this failed.
    void register_event_handler(scs_event_t event) const;

    /// Registers a channel handler, warning if this failed.
    void register_channel_handler(const ChannelMetadata &metadata);

    /// Registers a channel handler, warning if this failed.
    void register_trailer_handler(ChannelMetadata metadata);

    /// Constructor.
    Recorder(
        scs_u32_t version,
        const scs_telemetry_init_params_v101_t *init_params,
        const std::string &game_dir
    );

public:
    /// Call from the SCS API telemetry initialization hook to initialize the
    /// recorder.
    static void init(
        scs_u32_t version,
        const scs_telemetry_init_params_v101_t *init_params,
        const std::string &game_dir
    );

    /// Call from the SCS API telemetry shutdown hook to clean up recorder
    /// memory.
    static void shutdown();

    /// Sets a callback to be called when data has recently changed.
    static void set_update_server_callback(
        const std::function<void()> &callback
    );

    /// Returns const access to the channel metadata structure. Thread-safe-ish
    /// (relies on the vector not changing while the plugin is multithreaded).
    [[nodiscard]] static const std::vector<ChannelMetadata> &channel_metadata();

    /// Update the given vector with the latest channel data. Thread-safe.
    static void channel_poll(std::vector<scs_value_t> &data);

    /// Update the given JSON structure with the latest configuration data, if
    /// the version has changed since the previous call. Thread-safe.
    static bool configuration_poll(
        std::map<std::string, std::vector<NamedValue>> &data, uint64_t &version
    );

    /// Initializes the next_id value for poll() when a client first connects.
    /// Thread-safe.
    static uint64_t event_poll_init();

    /// Polls for events. The client doing the polling should keep track of the
    /// next_id variable; it is used to avoid sending duplicates. next_id will
    /// be incremented by the number of events returned by the function if and
    /// only if no events were dropped; it will be incremented by N more than
    /// the number of events returned if N events were dropped. The result is a
    /// JSON array of events. Thread-safe.
    static std::vector<std::vector<NamedValue>> event_poll(uint64_t &next_id);
};
