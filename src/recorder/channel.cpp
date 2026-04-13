#include "channel.h"

#include "json_utils.h"

ChannelRecorder::ChannelRecorder() {
    // Register "pseudochannels" for the information carried with the
    // start-frame event.
    idx_render_time = register_channel({"frame.render_time", SCS_U32_NIL, SCS_VALUE_TYPE_u64, ""});
    idx_simulation_time = register_channel({"frame.simulation_time", SCS_U32_NIL, SCS_VALUE_TYPE_u64, ""});
    idx_paused_simulation_time = register_channel({"frame.paused_simulation_time", SCS_U32_NIL, SCS_VALUE_TYPE_u64, ""});
    idx_paused = register_channel({"frame.paused", SCS_U32_NIL, SCS_VALUE_TYPE_bool, "game.paused"});
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

[[nodiscard]] const std::vector<ChannelMetadata> &ChannelRecorder::channels() const {
    return channel_metadata;
}

std::vector<scs_value_t> ChannelRecorder::poll() {
    std::lock_guard guard(mutex);
    return buffers[front];
}

nlohmann::json ChannelRecorder::poll_json_scs() {
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

nlohmann::json ChannelRecorder::poll_json_funbit() {
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
