#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

#include <eurotrucks2/scssdk_eut2.h>
#include <scssdk_input.h>
#include <scssdk_telemetry.h>

/// API callback for logging.
void log(const scs_log_type_t type, const scs_string_t message) {
    std::cout << "log " << type << ": " << message << std::endl;
}

/// Structure for storing an event handler registered by the plugin.
struct EventHandler {
    scs_telemetry_event_callback_t callback;
    scs_context_t context;
};

/// All the registered event handlers.
static std::map<scs_event_t, EventHandler> event_handlers{};

/// API callback for registering events.
scs_result_t register_for_event(
    const scs_event_t event,
    const scs_telemetry_event_callback_t callback,
    const scs_context_t context
) {
    // std::cout << "register for event " << event << std::endl;
    event_handlers[event] = {callback, context};
    return SCS_RESULT_ok;
}

/// API callback for unregistering events.
scs_result_t unregister_from_event(const scs_event_t event) {
    // std::cout << "unregister from event " << event << std::endl;
    event_handlers.erase(event);
    return SCS_RESULT_ok;
}

/// Sends an event to the plugin.
void send_event(const scs_event_t event, const void *event_info) {
    const auto it = event_handlers.find(event);
    if (it == event_handlers.end()) return;
    it->second.callback(event, event_info, it->second.context);
}

/// Structure for storing a channel handler registered by the plugin.
struct ChannelHandler {
    std::string name;
    scs_u32_t index;
    scs_value_t value;
    bool value_changed;
    bool value_present;
    scs_u32_t flags;
    scs_telemetry_channel_callback_t callback;
    scs_context_t context;
};

/// All the registered event handlers.
static std::map<std::string, ChannelHandler> channel_handlers{};

/// Returns a channel name that includes the requested array index, if any.
std::string channel_name(const scs_string_t name, const scs_u32_t index) {
    std::ostringstream full_name{};
    full_name << name;
    if (index != SCS_U32_NIL) {
        full_name << "[" << index << "]";
    }
    return full_name.str();
}

/// API callback for registering channels.
scs_result_t register_for_channel(
    const scs_string_t name,
    const scs_u32_t index,
    const scs_value_type_t type,
    const scs_u32_t flags,
    const scs_telemetry_channel_callback_t callback,
    const scs_context_t context
) {
    const auto full_name = channel_name(name, index);
    // std::cout << "register for channel " << full_name << " with type " <<
    // type << std::endl;
    ChannelHandler handler{};
    handler.name = name;
    handler.index = index;
    memset(&handler.value, 0, sizeof(handler.value));
    handler.value.type = type;
    handler.value_changed = false;
    handler.value_present = false;
    handler.flags = flags;
    handler.callback = callback;
    handler.context = context;
    channel_handlers[std::move(full_name)] = std::move(handler);
    return SCS_RESULT_ok;
}

/// API callback for unregistering channels.
scs_result_t unregister_from_channel(
    const scs_string_t name, const scs_u32_t index, const scs_value_type_t type
) {
    const auto full_name = channel_name(name, index);
    // std::cout << "unregister from channel " << full_name << " with type " <<
    // type << std::endl;
    channel_handlers.erase(full_name);
    return SCS_RESULT_ok;
}

/// Sends dummy channel data to all channels.
void send_channel_data() {
    for (auto &[_, handler] : channel_handlers) {
        bool send = true;
        if (!handler.value_changed) {
            if (!(handler.flags & SCS_TELEMETRY_CHANNEL_FLAG_each_frame)) {
                send = false;
            }
        }
        if (!handler.value_present) {
            if (!(handler.flags & SCS_TELEMETRY_CHANNEL_FLAG_no_value)) {
                send = false;
            }
        }
        if (send) {
            handler.callback(
                handler.name.c_str(), handler.index,
                handler.value_present ? &handler.value : nullptr,
                handler.context
            );
        }
        handler.value_changed = false;
    }
}

/// Sends a frame to the plugin.
void send_frame() {
    static constexpr scs_telemetry_frame_start_t frame_start_data = {};
    send_event(SCS_TELEMETRY_EVENT_frame_start, &frame_start_data);
    send_channel_data();
    send_event(SCS_TELEMETRY_EVENT_frame_end, nullptr);
}

/// Gets a line of input from stdin.
static std::string get_command() {
    std::string command;
    std::cin >> command;
    return command;
}

/// No-op input device registration function.
scs_result_t register_device(const scs_input_device_t *device_info) {
    return SCS_RESULT_ok;
}

/// Entry point for the dummy app.
int main(int argc, char *argv[]) {
    bool interactive = true;
    for (size_t i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--non-interactive")) {
            interactive = false;
        }
    }

    scs_sdk_init_params_v100_t common_params{};
    common_params.game_name = "Euro Truck Simulator Simulator";
    common_params.game_id = SCS_GAME_ID_EUT2;
    common_params.game_version = SCS_MAKE_VERSION(1, 18);
    common_params.log = log;

    scs_telemetry_init_params_v101_t telemetry_params{};
    telemetry_params.common = common_params;
    telemetry_params.register_for_event = register_for_event;
    telemetry_params.unregister_from_event = unregister_from_event;
    telemetry_params.register_for_channel = register_for_channel;
    telemetry_params.unregister_from_channel = unregister_from_channel;

    scs_input_init_params_v100_t input_params{};
    input_params.common = common_params;
    input_params.register_device = register_device;

    // Order undefined by API:
    scs_telemetry_init(SCS_TELEMETRY_VERSION_1_01, &telemetry_params);
    scs_input_init(SCS_INPUT_VERSION_1_00, &input_params);

    while (interactive) {
        std::future<std::string> future = std::async(get_command);
        while (future.wait_for(std::chrono::milliseconds(1000 / 60)) !=
               std::future_status::ready) {
            send_frame();
        }
        const auto command = future.get();
        if (command == "q") {
            break;
        } else {
            std::cout << "unknown command " << command << std::endl;
        }
    }

    // Order undefined by API:
    scs_telemetry_shutdown();
    scs_input_shutdown();

    return 0;
}
