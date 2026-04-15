#include "recorder.h"

#include <csignal>

#include <common/scssdk_telemetry_common_channels.h>
#include <common/scssdk_telemetry_common_configs.h>
#include <common/scssdk_telemetry_job_common_channels.h>
#include <common/scssdk_telemetry_trailer_common_channels.h>
#include <common/scssdk_telemetry_truck_common_channels.h>

#include "json_utils.h"
#include "logger.h"

std::unique_ptr<Recorder> Recorder::instance{};

SCSAPI_VOID Recorder::scs_event(
    const scs_event_t event,
    const void *const event_info,
    const scs_context_t context
) {
    if (!instance) return;
    try {
        instance->record_event(event, event_info);
    } catch (std::exception &e) {
        Logger::error("exception in scs_event (%d): %s", event, e.what());
    }
}

SCSAPI_VOID Recorder::scs_channel(
    const scs_string_t name,
    const scs_u32_t index,
    const scs_value_t *const value,
    const scs_context_t context
) {
    if (!instance) return;
    try {
        instance->record_channel(reinterpret_cast<uintptr_t>(context), value);
    } catch (std::exception &e) {
        Logger::error("exception in scs_channel (%s): %s", name, e.what());
    }
}

void Recorder::record_event(const scs_event_t event, const void *event_info) {
    switch (event) {
        case SCS_TELEMETRY_EVENT_frame_start:
            Logger::periodic();
            Logger::periodic();
            if (!event_info) return;
            channels.start(
                *static_cast<const scs_telemetry_frame_start_t *>(event_info)
            );
            break;
        case SCS_TELEMETRY_EVENT_frame_end:
            channels.end();
            if (update_server) update_server();
            break;
        case SCS_TELEMETRY_EVENT_paused:
            channels.pause();
            gameplay.push({NamedValue::event_id("paused")});
            break;
        case SCS_TELEMETRY_EVENT_started:
            channels.unpause();
            gameplay.push({NamedValue::event_id("started")});
            break;
        case SCS_TELEMETRY_EVENT_configuration:
            if (!event_info) return;
            record_configuration(
                static_cast<const scs_telemetry_configuration_t *>(event_info)
            );
            break;
        case SCS_TELEMETRY_EVENT_gameplay:
            if (!event_info) return;
            record_gameplay_event(
                static_cast<const scs_telemetry_gameplay_event_t *>(event_info)
            );
            break;
        default:
            Logger::warn("Unknown event %d", event);
            break;
    }
}

void Recorder::record_configuration(
    const scs_telemetry_configuration_t *scs_data
) {
    auto data = copy_scs_attributes(scs_data->attributes);
    configuration.push(scs_data->id, std::move(data));
}

void Recorder::record_gameplay_event(
    const scs_telemetry_gameplay_event_t *scs_data
) {
    auto data = copy_scs_attributes(scs_data->attributes);
    data.emplace_back(NamedValue::event_id(scs_data->id));
    gameplay.push(std::move(data));
}

void Recorder::record_channel(
    const size_t channel_index, const scs_value_t *value
) {
    scs_value_t value_copy{};
    if (value) {
        value_copy = *value;
    } else {
        value_copy.type = SCS_VALUE_TYPE_INVALID;
    }
    channels.push(channel_index, value_copy);
}

void Recorder::register_event_handler(const scs_event_t event) const {
    const auto result =
        init_params->register_for_event(event, scs_event, nullptr);
    if (result != SCS_RESULT_ok) {
        Logger::warn("failed to register event %d: code %d", event, result);
    }
}

void Recorder::register_channel_handler(const ChannelMetadata &metadata) {
    const auto channel_index = channels.register_channel(metadata);
    const auto context = reinterpret_cast<scs_context_t>(channel_index);
    const auto result = init_params->register_for_channel(
        metadata.name.c_str(), metadata.index, metadata.type,
        SCS_TELEMETRY_CHANNEL_FLAG_each_frame, scs_channel, context
    );
    if (result != SCS_RESULT_ok) {
        auto name = metadata.name;
        if (metadata.index != SCS_U32_NIL) {
            name += "[" + std::to_string(metadata.index) + "]";
        }
        Logger::warn(
            "failed to register channel %s (type %d): code %d", name.c_str(),
            metadata.type, result
        );
    }
}

void Recorder::register_trailer_handler(ChannelMetadata metadata) {
    register_channel_handler(metadata);

    // Support multiple trailers.
    if (metadata.name.substr(0, 8) != "trailer.") return;
    const auto remainder = metadata.name.substr(7); // including period
    for (uint32_t trailer_index = 0;
         trailer_index < SCS_TELEMETRY_trailers_count; trailer_index++) {
        metadata.name = "trailer." + std::to_string(trailer_index) + remainder;
        register_channel_handler(metadata);
    }
}

Recorder::Recorder(
    const scs_u32_t version,
    const scs_telemetry_init_params_v101_t *init_params,
    const std::string &game_dir
)
    : init_params(init_params), gameplay(std::chrono::seconds(10)) {
    // Push basic game information.
    configuration.push(
        "game",
        {NamedValue::scalar("name", init_params->common.game_name),
         NamedValue::scalar("id", init_params->common.game_id),
         NamedValue::scalar(
             "version", scs_version_to_json(init_params->common.game_version)
         ),
         NamedValue::scalar("api", scs_version_to_json(version)),
         NamedValue::scalar("path", game_dir)}
    );

    // Register event handlers.
    for (const scs_event_t event :
         {SCS_TELEMETRY_EVENT_frame_start, SCS_TELEMETRY_EVENT_frame_end,
          SCS_TELEMETRY_EVENT_paused, SCS_TELEMETRY_EVENT_started,
          SCS_TELEMETRY_EVENT_configuration, SCS_TELEMETRY_EVENT_gameplay}) {
        register_event_handler(event);
    }

    // Register common channels.
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_local_scale, SCS_U32_NIL, SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_game_time, SCS_U32_NIL, SCS_VALUE_TYPE_u32}
    );
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_multiplayer_time_offset, SCS_U32_NIL,
         SCS_VALUE_TYPE_s32}
    );
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_next_rest_stop, SCS_U32_NIL, SCS_VALUE_TYPE_s32}
    );

    // Register job channels.
    register_channel_handler(
        {SCS_TELEMETRY_JOB_CHANNEL_cargo_damage, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register truck movement channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_world_placement, SCS_U32_NIL,
         SCS_VALUE_TYPE_dplacement}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_local_linear_velocity, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_local_angular_velocity, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_local_linear_acceleration, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_local_angular_acceleration, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_cabin_offset, SCS_U32_NIL,
         SCS_VALUE_TYPE_fplacement}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_cabin_angular_velocity, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_cabin_angular_acceleration, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_head_offset, SCS_U32_NIL,
         SCS_VALUE_TYPE_fplacement}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float}
    );

    // Register truck powertrain channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear, SCS_U32_NIL,
         SCS_VALUE_TYPE_s32}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_displayed_gear, SCS_U32_NIL,
         SCS_VALUE_TYPE_s32}
    );

    // Register truck driving channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_steering, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_throttle, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_clutch, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_throttle, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_clutch, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_cruise_control, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register truck gearbox channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_hshifter_slot, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32}
    );
    // not implemented: SCS_TELEMETRY_TRUCK_CHANNEL_hshifter_selector

    // Register truck braking channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_motor_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_retarder_level, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure_emergency, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_temperature, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register truck consumable channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel, SCS_U32_NIL, SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_average_consumption, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_range, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_adblue, SCS_U32_NIL, SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_adblue_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_adblue_average_consumption, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register truck oil channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_oil_pressure, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_oil_pressure_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_oil_temperature, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register truck cooling channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_water_temperature, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_water_temperature_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );

    // Register truck battery channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_battery_voltage, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_battery_voltage_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );

    // Register enabled state of various truck elements.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_electric_enabled, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_engine_enabled, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_lblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_rblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_hazard_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_lblinker, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_rblinker, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_parking, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_low_beam, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_high_beam, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_aux_front, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_aux_roof, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_beacon, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_reverse, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wipers, SCS_U32_NIL, SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_dashboard_backlight, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_differential_lock, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_lift_axle, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_lift_axle_indicator, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_trailer_lift_axle, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_trailer_lift_axle_indicator, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );

    // Register truck wear information channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_engine, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_transmission, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_cabin, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_chassis, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_wheels, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_odometer, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register navigation channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_time, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_speed_limit, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register wheel channels.
    for (uint32_t wheel_index = 0; wheel_index < MAX_WHEELS; wheel_index++) {
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_susp_deflection, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_on_ground, wheel_index,
             SCS_VALUE_TYPE_bool}
        );
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_substance, wheel_index,
             SCS_VALUE_TYPE_u32}
        );
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_velocity, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_steering, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_rotation, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_lift, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_channel_handler(
            {SCS_TELEMETRY_TRUCK_CHANNEL_wheel_lift_offset, wheel_index,
             SCS_VALUE_TYPE_float}
        );
    }

    // Register trailer channels.
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_connected, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_cargo_damage, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_world_placement, SCS_U32_NIL,
         SCS_VALUE_TYPE_dplacement}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_local_linear_velocity, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_local_angular_velocity, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_local_linear_acceleration, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_local_angular_acceleration, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );

    // Register trailer wear information channels.
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_wear_body, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_wear_chassis, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_wear_wheels, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register trailer wheel channels.
    for (uint32_t wheel_index = 0; wheel_index < MAX_WHEELS; wheel_index++) {
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_susp_deflection, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_on_ground, wheel_index,
             SCS_VALUE_TYPE_bool}
        );
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_substance, wheel_index,
             SCS_VALUE_TYPE_u32}
        );
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_velocity, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_steering, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_rotation, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_lift, wheel_index,
             SCS_VALUE_TYPE_float}
        );
        register_trailer_handler(
            {SCS_TELEMETRY_TRAILER_CHANNEL_wheel_lift_offset, wheel_index,
             SCS_VALUE_TYPE_float}
        );
    }
}

void Recorder::init(
    const scs_u32_t version,
    const scs_telemetry_init_params_v101_t *init_params,
    const std::string &game_dir
) {
    if (instance)
        throw std::runtime_error("can only have one recorder at once");
    instance.reset(new Recorder(version, init_params, game_dir));
}

void Recorder::shutdown() {
    instance.reset();
}

void Recorder::set_update_server_callback(
    const std::function<void()> &callback
) {
    if (!instance) throw std::runtime_error("no recorder");
    instance->update_server = callback;
}

const std::vector<ChannelMetadata> &Recorder::channel_metadata() {
    if (!instance) throw std::runtime_error("no recorder");
    return instance->channels.channels();
}

void Recorder::channel_poll(std::vector<scs_value_t> &data) {
    if (!instance) return;
    return instance->channels.poll(data);
}

bool Recorder::configuration_poll(
    std::map<std::string, std::vector<NamedValue>> &data, uint64_t &version
) {
    if (!instance) return false;
    return instance->configuration.poll(data, version);
}

uint64_t Recorder::event_poll_init() {
    if (!instance) return 0;
    return instance->gameplay.poll_init();
}

std::vector<std::vector<NamedValue>> Recorder::event_poll(uint64_t &next_id) {
    if (!instance) return {};
    return instance->gameplay.poll(next_id);
}
