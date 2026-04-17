#include "input.h"

#include "logger.h"

Input::Channel::Channel(std::string name, ChannelType channel_type)
    : channel_type(channel_type), name(std::move(name)) {
    // Initialize float channels to midscale, but binary channels to
    // MIN = false.
    state = channel_type == ChannelType::FLOAT ? API_INPUT_VALUE_MID
                                               : API_INPUT_VALUE_MIN;
    game_state = state;
}

void Input::Channel::reset_press_count() {
    if (!button_presses_pending) return;
    button_presses_pending = 0;

    // Set the state to 0, because that's what would have happened if the press
    // had been processed.
    state = API_INPUT_VALUE_MIN;
}

bool Input::Channel::get_event(scs_input_event_t &event) {
    // Handle button press logic. We'll assert the input for one frame, then
    // release it for (at least) one frame for every simulated button press.
    if (button_presses_pending) {
        if (state < API_INPUT_VALUE_MID) {
            state = API_INPUT_VALUE_MAX;
        } else {
            state = API_INPUT_VALUE_MIN;
            button_presses_pending--;
        }
    }

    // Binarize the state if necessary. Avoids redundant input events for binary
    // channels if the state changes, but not past the threshold.
    if (channel_type == ChannelType::BINARY) {
        state = state < API_INPUT_VALUE_MID ? API_INPUT_VALUE_MIN
                                            : API_INPUT_VALUE_MAX;
    }

    // If the new state does not differ from the game state, we don't have to
    // send the game an event.
    if (state == game_state) {
        return false;
    }

    // State has changed, update the game.
    // Logger::info("setting input channel %d to %f", event.input_index, state);
    game_state = state;
    switch (channel_type) {
        case ChannelType::BINARY:
            event.value_bool.value = state >= API_INPUT_VALUE_MID;
            return true;
        case ChannelType::FLOAT:
            event.value_float.value = state;
            return true;
    }
    return false;
}

void Input::Channel::send_input(const float action) {
    if (action == API_INPUT_VALUE_PRESS) {
        button_presses_pending++;
    } else {
        state = action;
    }
}

void Input::start_of_frame(bool activated) {
    // Flush pending log messages to the game.
    Logger::periodic();

    // See if it's been at least two seconds since the previous frame.
    const auto now = std::chrono::system_clock::now();
    if (!activated) {
        if (now - last_frame > std::chrono::seconds(2)) {
            activated = true;
        }
    }
    last_frame = now;

    // If it has been, reset the button press logic for all channels.
    if (activated) {
        Logger::info("Input logic reactivated; flushing button presses");
        for (auto &channel : channels) {
            channel.reset_press_count();
        }
    }

    // Restart update polling from channel 0.
    poll_index = 0;

    // Play back events from the server.
    std::lock_guard guard(instance->event_queue_mutex);
    while (!instance->event_queue.empty()) {
        const auto &event = instance->event_queue.front();
        if (event.channel < channels.size()) {
            channels[event.channel].send_input(event.action);
        }
        instance->event_queue.pop();
    }
}

bool Input::get_event(scs_input_event_t &event) {

    // Look for the first channel that has an event ready.
    while (poll_index < channels.size()) {
        event.input_index = poll_index++;
        if (channels[event.input_index].get_event(event)) {
            return true;
        }
    }

    // No event found.
    return false;
}

std::unique_ptr<Input> Input::instance = {};

SCSAPI_RESULT Input::scs_input_event(
    scs_input_event_t *event_info, const scs_u32_t flags, scs_context_t context
) {
    if (!instance) return SCS_RESULT_not_found;
    if (!event_info) return SCS_RESULT_invalid_parameter;
    try {
        bool first = flags & SCS_INPUT_EVENT_CALLBACK_FLAG_first_in_frame;
        const bool activated =
            flags & SCS_INPUT_EVENT_CALLBACK_FLAG_first_after_activation;

        // The first-in-frame flag doesn't behave like it should per my
        // understanding of the documentation: I'd expect that for repeated
        // queries due to a preceding query yielding SCS_RESULT_ok the flag
        // would be cleared. It's not though, so...
        first &= instance->poll_index >= instance->channels.size();

        // Do things that need to be done on a per-frame basis.
        if (first) {
            instance->start_of_frame(activated);
        }

        // Find the next event to be sent to the game.
        if (instance->get_event(*event_info)) {
            return SCS_RESULT_ok;
        }

        // No event found. Game will query again next frame.
        return SCS_RESULT_not_found;
    } catch (const std::exception &e) {
        Logger::error("input error: %s", e.what());
    }
    return SCS_RESULT_generic_error;
}

void Input::register_channel(
    const char *name, const char *display_name, const ChannelType type
) {
    // Create an input channel object for this channel.
    channel_map[name] = {static_cast<scs_u32_t>(channels.size()), type};
    channels.emplace_back(name, type);

    // Create a descriptor for the game.
    scs_input_device_input_t descriptor = {};
    descriptor.name = name;
    descriptor.display_name = display_name;
    switch (type) {
        case ChannelType::BINARY:
            descriptor.value_type = SCS_VALUE_TYPE_bool;
            break;
        case ChannelType::FLOAT:
            descriptor.value_type = SCS_VALUE_TYPE_float;
            break;
    }
    game_input_descriptors.emplace_back(descriptor);
}

Input::Input(
    const scs_input_init_params_v100_t *init_params, const Configuration &config
) {
    // Leave only an empty input system if no channels are configured.
    if (config.input_configuration.is_null()) {
        Logger::info("No input channels configured. Input system disabled.");
        return;
    }
    try {
        auto it = config.input_configuration.find(CONFIG_INPUT_FLOAT);
        if (it != config.input_configuration.end()) {
            for (const auto &item : it->items()) {
                register_channel(
                    item.key().c_str(), item.value().get<std::string>().c_str(),
                    ChannelType::FLOAT
                );
            }
        }
        it = config.input_configuration.find(CONFIG_INPUT_BINARY);
        if (it != config.input_configuration.end()) {
            for (const auto &item : it->items()) {
                register_channel(
                    item.key().c_str(), item.value().get<std::string>().c_str(),
                    ChannelType::BINARY
                );
            }
        }
        poll_index = channels.size();
        if (channels.empty()) {
            Logger::info(
                "No input channels configured. Input system disabled."
            );
            return;
        }
        Logger::info("%d input channel(s) configured.", channels.size());
    } catch (const std::exception &e) {
        Logger::error("Input system misconfigured: %s", e.what());
        return;
    }

    // Build device descriptor.
    game_device_descriptor.name = "trucktel_jvs";
    game_device_descriptor.display_name = "TruckTel virtual input";
    game_device_descriptor.type = SCS_INPUT_DEVICE_TYPE_semantical;
    game_device_descriptor.input_count = game_input_descriptors.size();
    game_device_descriptor.inputs = game_input_descriptors.data();
    game_device_descriptor.callback_context = nullptr;
    game_device_descriptor.input_active_callback = nullptr;
    game_device_descriptor.input_event_callback = scs_input_event;

    // Try to register ourselves.
    if (!init_params->register_device) {
        throw std::invalid_argument("missing register_device fptr");
    }
    const auto result = init_params->register_device(&game_device_descriptor);
    if (result != SCS_RESULT_ok) {
        // Registration may fail if some input is unknown by the game. Don't
        // throw an exception in that case. That way, the telemetry side will
        // still work.
        Logger::error("Failed to register virtual input device (%d)!", result);

        // Dump the channel mapping to the log file, so the user can debug which
        // channel was the problem by cross-referencing it with the
        // game-generated error message in the game console.
        for (size_t i = 0; i < channels.size(); i++) {
            Logger::verbose(
                "Input index %d is %s", i, channels[i].name.c_str()
            );
        }

        // Clear the channel map though, so get_inputs() for a misconfigured
        // input system will yield nothing.
        channel_map.clear();
    }
}

void Input::init(
    const scs_input_init_params_v100_t *init_params, const Configuration &config
) {
    if (instance)
        throw std::runtime_error("can only have one recorder at once");
    instance.reset(new Input(init_params, config));
}

void Input::shutdown() {
    instance.reset();
}

bool Input::send_input(const std::string &channel, const float new_state) {
    if (!instance) return false;
    std::lock_guard guard(instance->event_queue_mutex);
    const auto it = instance->channel_map.find(channel);
    if (it == instance->channel_map.end()) return false;
    instance->event_queue.emplace(InputEvent{it->second.first, new_state});
    return true;
}

nlohmann::json Input::get_inputs() {
    if (!instance) return nullptr;
    nlohmann::json result = {};
    for (const auto &[name, info] : instance->channel_map) {
        auto type = "?";
        switch (info.second) {
            case ChannelType::BINARY:
                type = API_INPUT_TYPE_BINARY;
                break;
            case ChannelType::FLOAT:
                type = API_INPUT_TYPE_FLOAT;
                break;
        }
        result[name] = type;
    }
    return result;
}

nlohmann::json Input::run_query(const std::vector<std::string> &query) {
    if (query.empty()) return nullptr;
    if (query[0] != API_INPUT_QUERY) return nullptr;
    if (query.size() < 2) return "missing input command";
    const auto &command = query[1];

    // Handle input-listing command.
    if (command == API_INPUT_COMMAND_LIST) {
        return get_inputs();
    }

    // The rest of the commands must have a name.
    if (query.size() < 3) return "missing input name";
    const auto &channel = query[2];

    // Handle commands without values.
    if (command == API_INPUT_COMMAND_PRESS) {
        return send_input(channel);
    }
    if (command == API_INPUT_COMMAND_HOLD) {
        return send_input(channel, API_INPUT_VALUE_MAX);
    }
    if (command == API_INPUT_COMMAND_RELEASE) {
        return send_input(channel, API_INPUT_VALUE_MIN);
    }

    // The only remaining supported command is "set".
    if (command != API_INPUT_COMMAND_SET) {
        return "invalid input command";
    }

    // Parse the input value.
    if (query.size() < 4) return "missing value to set";
    const auto &value_str = query[3];
    float value;
    try {
        // Parsing with nlohmann::json to avoid locale insanity.
        value = nlohmann::json::parse(value_str).get<float>();
    } catch (const std::exception &e) {
        return std::string("invalid value: ") + e.what();
    }
    if (value < API_INPUT_VALUE_MIN) {
        value = API_INPUT_VALUE_MIN;
    } else if (value > API_INPUT_VALUE_MAX) {
        value = API_INPUT_VALUE_MAX;
    }

    // Send the input.
    return send_input(channel, value);
}
