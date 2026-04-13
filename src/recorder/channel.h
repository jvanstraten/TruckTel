#pragma once

// Standard libraries.
#include <mutex>
#include <string>
#include <vector>

// Dependencies.
#include <nlohmann/json.hpp>
#include <scssdk_telemetry_event.h>
#include <scssdk_value.h>

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
    /// Creates a ChannelRecorder.
    ChannelRecorder();

    /// Registers a channel. This must only be done during initialization,
    /// before any other calls are made from any thread. The return value is
    /// the index of the new channel.
    size_t register_channel(ChannelMetadata metadata);

    /// Sets the game simulation state to paused.
    void pause();

    /// Sets the game simulation state to unpaused.
    void unpause();

    /// Starts a frame.
    void start(const scs_telemetry_frame_start_t &info);

    /// Updates the given key of back buffer to the given value.
    void push(const size_t index, const scs_value_t &value);

    /// Flips the buffers. To be called by the SCS API side when a frame is
    /// complete. The type codes of all values are set to invalid in the new
    /// buffer, to indicate that they have not been set.
    void end();

    /// Returns const access to the channel metadata structure.
    [[nodiscard]] const std::vector<ChannelMetadata> &channels() const;

    /// Returns a copy of the most recent frame. The indices in the returned
    /// array correspond to those returned by channels(), though the returned
    /// array may be smaller than the array returned by channels(). Channels
    /// that did not receive a value in the most recent frame will have their
    /// type code set to SCS_VALUE_TYPE_INVALID and must be ignored by the
    /// caller.
    std::vector<scs_value_t> poll();

    /// Generates JSON data of everything in the most recent frame.
    nlohmann::json poll_json_scs();

    /// Generates JSON data in roughly the format used by Funbit's telemetry
    /// server. This doesn't include static config data yet, and some post-
    /// conversions are necessary for compatibility.
    nlohmann::json poll_json_funbit();
};
