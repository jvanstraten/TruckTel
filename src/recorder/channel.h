#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <scssdk_value.h>

#include <scssdk_telemetry_event.h>

#include <nlohmann/json.hpp>

/// Metadata for channels.
struct ChannelMetadata {
    /// SCS API name for this channel.
    std::string name;

    /// SCS API index for this channel.
    scs_u32_t index;

    /// SCS API data type for this channel.
    scs_value_type_t type;
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

    /// Updates the given vector such that it contains a copy of the most recent
    /// frame. The indices in the vector correspond to those returned by
    /// channels().
    void poll(std::vector<scs_value_t> &data);

    /// Generates JSON data of everything in the most recent frame.
    /// TODO: move this
    nlohmann::json poll_json();
};
