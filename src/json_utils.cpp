#include "json_utils.h"

void json_assign_path(
    nlohmann::json &json,
    const std::string &path,
    const nlohmann::json &data,
    const bool flatten
) {
    size_t start = 0;
    auto json_ptr = &json;
    while (true) {
        // If there was already a nontrivial and non-dict value for the level
        // we're about to index, punt it to a "_" key wrapped inside a
        // surrounding object. For example, if "cargo" already exists, and
        // we're trying to set "cargo.mass", the "cargo" key will become
        // "cargo._".
        if (!json_ptr->is_object() && !json_ptr->is_null()) {
            *json_ptr = {{"_", *json_ptr}};
        }

        // See if there is another separator.
        const auto pos = flatten ? std::string::npos : path.find('.', start);
        const auto element = path.substr(start, pos - start);
        if (pos == std::string::npos) {

            // This is the last path element. If the path already exists and
            // is an object, put the data in a _ key. This is the complement
            // of the above: if "cargo.mass" already exists, and we're trying
            // to set "cargo", the "cargo" key will become "cargo._".
            if ((*json_ptr)[element].is_object()) {
                (*json_ptr)[element]["_"] = data;
            } else {
                (*json_ptr)[element] = data;
            }
            return;
        }

        // Index the next path element in our JSON structure and continue
        // with the remainder of the path.
        json_ptr = &(*json_ptr)[element];
        start = pos + 1;
    }
}

nlohmann::json scs_version_to_json(const scs_u32_t version) {
    return {SCS_GET_MAJOR_VERSION(version), SCS_GET_MINOR_VERSION(version)};
}

nlohmann::json scs_value_to_json(const scs_value_t &value) {
    switch (value.type) {
        case SCS_VALUE_TYPE_INVALID:
            return nullptr;
        case SCS_VALUE_TYPE_bool:
            return value.value_bool.value ? true : false;
        case SCS_VALUE_TYPE_s32:
            return value.value_s32.value;
        case SCS_VALUE_TYPE_s64:
            return value.value_s64.value;
        case SCS_VALUE_TYPE_u32:
            return value.value_u32.value;
        case SCS_VALUE_TYPE_u64:
            return value.value_u64.value;
        case SCS_VALUE_TYPE_float:
            return value.value_float.value;
        case SCS_VALUE_TYPE_double:
            return value.value_double.value;
        case SCS_VALUE_TYPE_fvector:
            return {
                value.value_fvector.x, value.value_fvector.y,
                value.value_fvector.z
            };
        case SCS_VALUE_TYPE_dvector:
            return {
                value.value_dvector.x, value.value_dvector.y,
                value.value_dvector.z
            };
        case SCS_VALUE_TYPE_euler:
            return {
                value.value_euler.heading, value.value_euler.pitch,
                value.value_euler.roll
            };
        case SCS_VALUE_TYPE_fplacement:
            return {
                value.value_fplacement.position.x,
                value.value_fplacement.position.y,
                value.value_fplacement.position.z,
                value.value_fplacement.orientation.heading,
                value.value_fplacement.orientation.pitch,
                value.value_fplacement.orientation.roll
            };
        case SCS_VALUE_TYPE_dplacement:
            return {
                value.value_dplacement.position.x,
                value.value_dplacement.position.y,
                value.value_dplacement.position.z,
                value.value_dplacement.orientation.heading,
                value.value_dplacement.orientation.pitch,
                value.value_dplacement.orientation.roll
            };
        case SCS_VALUE_TYPE_string:
            return value.value_string.value;
        default:
            return "unknown type " + std::to_string(value.type);
    }
}

NamedValue NamedValue::scalar(std::string name, nlohmann::json value) {
    return NamedValue{
        .name = std::move(name), .index = SCS_U32_NIL, .value = std::move(value)
    };
}

NamedValue NamedValue::event_id(const std::string &event_id) {
    return NamedValue{.name = "_", .index = SCS_U32_NIL, .value = event_id};
}

std::vector<NamedValue> copy_scs_attributes(
    const scs_named_value_t *attributes
) {
    if (!attributes) return {};
    std::vector<NamedValue> result;
    while (attributes->name) {
        result.emplace_back(
            NamedValue{
                attributes->name, attributes->index,
                scs_value_to_json(attributes->value)
            }
        );
        ++attributes;
    }
    return result;
}
