#include "channel.h"

#include "api.h"
#include "json_utils.h"

ChannelRecorder::ChannelRecorder() {
    // Register "pseudochannels" for the information carried with the
    // start-frame event.
    idx_render_time = register_channel(
        {API_FRAME_CHANNEL_RENDER_TIME, SCS_U32_NIL, SCS_VALUE_TYPE_u64}
    );
    idx_fps = register_channel(
        {API_FRAME_CHANNEL_FPS, SCS_U32_NIL, SCS_VALUE_TYPE_float}
    );
    idx_fps_filtered = register_channel(
        {API_FRAME_CHANNEL_FPS_FILTERED, SCS_U32_NIL, SCS_VALUE_TYPE_float}
    );
    idx_simulation_time = register_channel(
        {API_FRAME_CHANNEL_SIMULATION_TIME, SCS_U32_NIL, SCS_VALUE_TYPE_u64}
    );
    idx_paused_simulation_time = register_channel(
        {API_FRAME_CHANNEL_PAUSED_SIMULATION_TIME, SCS_U32_NIL,
         SCS_VALUE_TYPE_u64}
    );
    idx_paused = register_channel(
        {API_FRAME_CHANNEL_PAUSED, SCS_U32_NIL, SCS_VALUE_TYPE_bool}
    );
    idx_paused_simulation_time = register_channel(
        {API_FRAME_CHANNEL_PAUSED_SIMULATION_TIME, SCS_U32_NIL,
         SCS_VALUE_TYPE_u64}
    );
}

size_t ChannelRecorder::register_channel(ChannelMetadata metadata) {
    const auto index = channel_metadata.size();
    channel_metadata.emplace_back(std::move(metadata));
    return index;
}

void ChannelRecorder::pause() {
    paused = true;
}

void ChannelRecorder::unpause() {
    paused = false;
}

void ChannelRecorder::start(const scs_telemetry_frame_start_t &info) {
    // Determine render time delta since previous frame.
    const int64_t render_time = static_cast<int64_t>(info.render_time);
    const int64_t time_delta = render_time - prev_render_time;

    // If this time delta seems reasonable, determine current frames per second.
    constexpr auto ONE_SECOND = 1000 * 1000;
    if (!paused && prev_render_time_valid && time_delta >= 0 &&
        time_delta < 10 * ONE_SECOND) {
        // Do not invalidate FPS data when the game skips rendering for a
        // simulation frame.
        if (time_delta != 0) {
            fps = ONE_SECOND / static_cast<float>(time_delta);
            if (!fps_valid) fps_filtered = fps;
            fps_filtered += (fps - fps_filtered) * API_FPS_FILTER_CONSTANT;
            fps_valid = true;
        }
    } else {
        fps_valid = false;
    }

    // Store previous timestamp for the next FPS computation.
    prev_render_time = render_time;
    prev_render_time_valid = !paused;

    // Clear all items in the frame.
    for (auto &item : buffers[1 - front]) {
        item.type = SCS_VALUE_TYPE_INVALID;
    }

    // Set frame information.
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

    // Set FPS data only when the FPS computation is valid.
    if (fps_valid) {
        val.type = SCS_VALUE_TYPE_float;
        val.value_float.value = fps;
        push(idx_fps, val);
        val.value_float.value = fps_filtered;
        push(idx_fps_filtered, val);
    }
}

void ChannelRecorder::push(const size_t index, const scs_value_t &value) {
    auto &buffer = buffers[1 - front];
    while (index >= buffer.size()) {
        buffer.emplace_back();
        buffer.back().type = SCS_VALUE_TYPE_INVALID;
    }
    buffer[index] = value;
}

void ChannelRecorder::end() {
    std::lock_guard guard(mutex);
    front = 1 - front;
}

[[nodiscard]] const std::vector<ChannelMetadata> &ChannelRecorder::
    channels() const {
    return channel_metadata;
}

void ChannelRecorder::poll(std::vector<scs_value_t> &data) {
    // Make sure the vector has the right size before claiming the lock.
    const size_t size = channel_metadata.size();
    if (data.size() != size) {
        data.resize(size);
        for (auto &datum : data) {
            datum.type = SCS_VALUE_TYPE_INVALID;
        }
    }

    // Only do a memcpy while the lock is held, so the lock is held for a
    // minimal amount of time. This is "important", because it can block the
    // game thread. It's also premature optimization.
    std::lock_guard guard(mutex);
    memcpy(
        data.data(), buffers[front].data(),
        buffers[front].size() * sizeof(scs_value_t)
    );
}
