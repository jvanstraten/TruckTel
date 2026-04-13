#include "recorder.h"

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
            if (!event_info) return;
            frame.start(
                *static_cast<const scs_telemetry_frame_start_t *>(event_info)
            );
            break;
        case SCS_TELEMETRY_EVENT_frame_end:
            frame.end();
            break;
        case SCS_TELEMETRY_EVENT_paused:
            frame.pause();
            gameplay.push({{"_", "paused"}});

            // TODO: remove me
            Logger::verbose("Simulation paused!");
            Logger::verbose("Yes, this is the latest version!");
            Logger::verbose(
                "Last frame before pause: %s",
                frame.poll_json_scs().dump().c_str()
            );
            Logger::verbose("Configuration: %s", config.poll().dump().c_str());
            break;
        case SCS_TELEMETRY_EVENT_started:
            frame.unpause();
            gameplay.push({{"_", "started"}});

            // TODO: remove me
            Logger::verbose("Simulation started!");
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
    nlohmann::json json_data = scs_attributes_to_json(scs_data->attributes);
    config.push(scs_data->id, std::move(json_data));
}

void Recorder::record_gameplay_event(
    const scs_telemetry_gameplay_event_t *scs_data
) {
    nlohmann::json json_data = scs_attributes_to_json(scs_data->attributes);
    json_data["_"] = scs_data->id;
    Logger::verbose("Gameplay event: %s", json_data.dump().c_str());
    gameplay.push(std::move(json_data));
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
    frame.push(channel_index, value_copy);
}

void Recorder::register_event_handler(const scs_event_t event) const {
    const auto result =
        init_params->register_for_event(event, scs_event, nullptr);
    if (result != SCS_RESULT_ok) {
        Logger::warn("failed to register event %d: code %d", event, result);
    }
}

void Recorder::register_channel_handler(const ChannelMetadata &metadata) {
    const auto channel_index = frame.register_channel(metadata);
    const auto context = reinterpret_cast<scs_context_t>(channel_index);
    const auto result = init_params->register_for_channel(
        metadata.scs_name.c_str(), metadata.scs_index, metadata.scs_type,
        SCS_TELEMETRY_CHANNEL_FLAG_each_frame, scs_channel, context
    );
    if (result != SCS_RESULT_ok) {
        auto name = metadata.scs_name;
        if (metadata.scs_index != SCS_U32_NIL) {
            name += "[" + std::to_string(metadata.scs_index) + "]";
        }
        Logger::warn(
            "failed to register channel %s (type %d): code %d", name.c_str(),
            metadata.scs_type, result
        );
    }
}

void Recorder::register_trailer_handler(ChannelMetadata metadata) {
    register_channel_handler(metadata);

    // Support multiple trailers.
    metadata.funbit_name.clear();
    if (!metadata.scs_name.starts_with("trailer.")) return;
    const auto remainder = metadata.scs_name.substr(7); // including period
    for (uint32_t trailer_index = 0;
         trailer_index < SCS_TELEMETRY_trailers_count; trailer_index++) {
        metadata.scs_name =
            "trailer." + std::to_string(trailer_index) + remainder;
        register_channel_handler(metadata);
    }
}

Recorder::Recorder(
    const scs_u32_t version, const scs_telemetry_init_params_v101_t *init_params
)
    : init_params(init_params), gameplay(std::chrono::seconds(10)) {
    // Push basic game information.
    game.push("game_id", init_params->common.game_id);
    game.push("game_name", init_params->common.game_name);
    game.push(
        "game_version", scs_version_to_json(init_params->common.game_version)
    );
    game.push("api_version", scs_version_to_json(version));

    // Register event handlers.
    for (const scs_event_t event :
         {SCS_TELEMETRY_EVENT_frame_start, SCS_TELEMETRY_EVENT_frame_end,
          SCS_TELEMETRY_EVENT_paused, SCS_TELEMETRY_EVENT_started,
          SCS_TELEMETRY_EVENT_configuration, SCS_TELEMETRY_EVENT_gameplay}) {
        register_event_handler(event);
    }

    // Register common channels.
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_local_scale, SCS_U32_NIL, SCS_VALUE_TYPE_float,
         "game.timeScale"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_game_time, SCS_U32_NIL, SCS_VALUE_TYPE_u32,
         "game.time"}
    ); // TODO convert date time
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_multiplayer_time_offset, SCS_U32_NIL,
         SCS_VALUE_TYPE_s32}
    );
    register_channel_handler(
        {SCS_TELEMETRY_CHANNEL_next_rest_stop, SCS_U32_NIL, SCS_VALUE_TYPE_s32,
         "game.nextRestStopTime"}
    ); // TODO convert date time

    // Register job channels.
    register_channel_handler(
        {SCS_TELEMETRY_JOB_CHANNEL_cargo_damage, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );

    // Register truck movement channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_world_placement, SCS_U32_NIL,
         SCS_VALUE_TYPE_dplacement, "truck.placement"}
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
         SCS_VALUE_TYPE_fvector, "truck.acceleration"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_local_angular_acceleration, SCS_U32_NIL,
         SCS_VALUE_TYPE_fvector}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_cabin_offset, SCS_U32_NIL,
         SCS_VALUE_TYPE_fplacement, "truck.cabin"}
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
         SCS_VALUE_TYPE_fplacement, "truck.head"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float,
         "truck.speed"}
    );

    // Register truck powertrain channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.engineRpm"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear, SCS_U32_NIL,
         SCS_VALUE_TYPE_s32, "truck.gear"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_displayed_gear, SCS_U32_NIL,
         SCS_VALUE_TYPE_s32, "truck.displayedGear"}
    );

    // Register truck driving channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_steering, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.userSteer"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_throttle, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.userThrottle"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.userBrake"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_input_clutch, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.userClutch"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.gameSteer"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_throttle, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.gameThrottle"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.gameBrake"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_effective_clutch, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.gameClutch"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_cruise_control, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.cruiseControlSpeed"}
    ); // TODO also convert to boolean

    // Register truck gearbox channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_hshifter_slot, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32, "truck.shifterSlot"}
    );
    // not implemented: SCS_TELEMETRY_TRUCK_CHANNEL_hshifter_selector

    // Register truck braking channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.parkBrakeOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_motor_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.motorBrakeOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_retarder_level, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32, "truck.retarderBrake"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.airPressure"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.airPressureWarningOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_air_pressure_emergency, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.airPressureEmergencyOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_brake_temperature, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.brakeTemperature"}
    );

    // Register truck consumable channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel, SCS_U32_NIL, SCS_VALUE_TYPE_float,
         "truck.fuel"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.fuelWarningOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_average_consumption, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.fuelAverageConsumption"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_fuel_range, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_adblue, SCS_U32_NIL, SCS_VALUE_TYPE_float,
         "truck.adblue"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_adblue_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.adblueWarningOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_adblue_average_consumption, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.adblueAverageConsumpton"}
    );

    // Register truck oil channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_oil_pressure, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.oilPressure"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_oil_pressure_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.oilPressureWarningOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_oil_temperature, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.oilTemperature"}
    );

    // Register truck cooling channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_water_temperature, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.waterTemperature"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_water_temperature_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.waterTemperatureWarningOn"}
    );

    // Register truck battery channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_battery_voltage, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.batteryVoltage"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_battery_voltage_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.batteryVoltageWarningOn"}
    );

    // Register enabled state of various truck elements.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_electric_enabled, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.electricOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_engine_enabled, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.engineOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_lblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool,
         "truck.blinkerLeftActive"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_rblinker, SCS_U32_NIL, SCS_VALUE_TYPE_bool,
         "truck.blinkerRightActive"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_hazard_warning, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_lblinker, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.blinkerLeftOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_rblinker, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.blinkerRightOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_parking, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.lightsParkingOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_low_beam, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.lightsBeamLowOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_high_beam, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.lightsBeamHighOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_aux_front, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32, "truck.lightsAuxFrontOn"}
    ); // TODO convert to boolean
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_aux_roof, SCS_U32_NIL,
         SCS_VALUE_TYPE_u32, "truck.lightsAuxRoofOn"}
    ); // TODO convert to boolean
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_beacon, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.lightsBeaconOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_brake, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.lightsBrakeOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_light_reverse, SCS_U32_NIL,
         SCS_VALUE_TYPE_bool, "truck.lightsReverseOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wipers, SCS_U32_NIL, SCS_VALUE_TYPE_bool,
         "truck.wipersOn"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_dashboard_backlight, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.lightsDashboardValue"}
    ); // TODO also convert to boolean
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
         SCS_VALUE_TYPE_float, "truck.wearEngine"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_transmission, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.wearTransmission"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_cabin, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.wearCabin"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_chassis, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.wearChassis"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_wear_wheels, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.wearWheels"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_odometer, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "truck.odometer"}
    );

    // Register navigation channels.
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "navigation.estimatedDistance"}
    );
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_time, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "navigation.estimatedTime"}
    ); // TODO convert date time
    register_channel_handler(
        {SCS_TELEMETRY_TRUCK_CHANNEL_navigation_speed_limit, SCS_U32_NIL,
         SCS_VALUE_TYPE_float, "navigation.speedLimit"}
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
         SCS_VALUE_TYPE_bool, "trailer.attached"}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_cargo_damage, SCS_U32_NIL,
         SCS_VALUE_TYPE_float}
    );
    register_trailer_handler(
        {SCS_TELEMETRY_TRAILER_CHANNEL_world_placement, SCS_U32_NIL,
         SCS_VALUE_TYPE_dplacement, "trailer.placement"}
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
         SCS_VALUE_TYPE_float, "trailer.wear"}
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
    const scs_u32_t version, const scs_telemetry_init_params_v101_t *init_params
) {
    if (instance)
        throw std::runtime_error("can only have one recorder at once");
    instance.reset(new Recorder(version, init_params));
}

void Recorder::shutdown() {
    instance.reset();
}
