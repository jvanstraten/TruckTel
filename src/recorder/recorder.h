#pragma once

// Standard libraries.
#include <memory>

// Dependencies.
#include <scssdk_telemetry.h>

// Private headers.
#include "versioned.h"
#include "event.h"
#include "channel.h"

/// Singleton class that records data received from the SCS API via events and
/// channels and provides asynchronous and thread-safe access to this data.
class Recorder {
private:
    /// Maximum number of wheels for a truck or trailer queried.
    static constexpr uint32_t MAX_WHEELS = 14;

    /// Initialization parameters passed to the plugin initialization function
    /// by the game.
    const scs_telemetry_init_params_v101_t *const init_params;

    /// Basic game and API information.
    VersionedRecorder game;

    /// Game configuration data as received via the configuration event.
    VersionedRecorder config;

    /// Gameplay event recorder. This also records paused/started events.
    EventRecorder gameplay;

    /// Frame data from channels.
    ChannelRecorder frame;

    /// Current recorder instance used by the static callbacks from the SCS
    /// API.
    static std::unique_ptr<Recorder> instance;

    /// Event handler.
    static SCSAPI_VOID scs_event(scs_event_t event, const void *event_info, scs_context_t context);

    /// Channel data handler. The context value is used to keep track of the raw
    /// channel indices, so we don't need to spend time messing with strings for
    /// basic recording of values.
    static SCSAPI_VOID scs_channel(scs_string_t name, scs_u32_t index, const scs_value_t *value, scs_context_t context);

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
    Recorder(scs_u32_t version, const scs_telemetry_init_params_v101_t *init_params);

public:
    /// Call from the SCS API telemetry initialization hook to initialize the
    /// recorder.
    static void init(scs_u32_t version, const scs_telemetry_init_params_v101_t *init_params);

    /// Call from the SCS API telemetry shutdown hook to clean up recorder
    /// memory.
    static void shutdown();
};
