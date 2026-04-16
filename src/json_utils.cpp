#include "json_utils.h"

#include <ctime>

#include "api.h"
#include "config.h"
#include "logger.h"

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
            *json_ptr = {{API_CONFLICT_KEY, *json_ptr}};
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
                (*json_ptr)[element][API_CONFLICT_KEY] = data;
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

nlohmann::json json_resolve_path(
    const nlohmann::json &data, const std::string &path
) {
    size_t start = 0;
    auto data_ptr = &data;
    while (true) {
        // See if there is another separator.
        const auto pos = path.find('.', start);
        const auto element = path.substr(start, pos - start);

        // Try to resolve the path element.
        try {
            if (data_ptr->is_object()) {
                data_ptr = &data_ptr->at(element);
            } else {
                data_ptr = &data_ptr->at(std::stoul(element));
            }
        } catch (const std::exception &e) {
            Logger::verbose("path %s -> failure %s", e.what());
            Logger::verbose("in data %s", data_ptr->dump().c_str());
            return nullptr;
        }

        // Stop once we've completed iterating over the path.
        if (pos == std::string::npos) {
            Logger::verbose(
                "path %s -> %s", path.c_str(), data_ptr->dump().c_str()
            );
            return *data_ptr;
        }

        // Advance iteration of the path elements beyond the separator we found.
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
    return NamedValue{std::move(name), SCS_U32_NIL, std::move(value)};
}

NamedValue NamedValue::event_id(const std::string &event_id) {
    return NamedValue{API_EVENT_ID_KEY, SCS_U32_NIL, event_id};
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

nlohmann::json named_values_to_json(
    const std::vector<NamedValue> &data, bool flatten
) {

    // Named values on the SCS telemetry API that represent arrays are sent
    // itemwise. To convert this to a sane JSON format, we first need to group
    // items by their corresponding array.
    std::map<std::string, std::pair<bool, std::vector<nlohmann::json>>>
        unflattened;
    for (const auto &item : data) {
        if (item.value.is_null()) continue;
        auto [it, _] = unflattened.emplace(
            item.name, std::pair<bool, std::vector<nlohmann::json>>()
        );
        auto &[is_array, items] = it->second;
        is_array = item.index != SCS_U32_NIL;
        const auto index = item.index != SCS_U32_NIL ? item.index : 0;
        while (items.size() <= index)
            items.emplace_back();
        items[index] = item.value;
    }

    // Now build the JSON object.
    nlohmann::json result;
    for (const auto &[key, value] : unflattened) {
        const auto &[is_array, items] = value;
        auto json_value =
            (is_array || items.empty()) ? nlohmann::json(items) : items[0];
        json_assign_path(result, key, json_value, flatten);
    }
    return result;
}

nlohmann::json json_delta_encode(
    const nlohmann::json &new_data, const nlohmann::json &previous_data
) {
    // Handle non-object types.
    if (!new_data.is_object() || !previous_data.is_object()) {

        // Return null if the data has not changed. The caller must interpret
        // the null as a removal of the corresponding key.
        if (new_data == previous_data) return nullptr;

        return new_data;
    }

    // Both new_data and previous_data must now be objects. Handle new data
    // first.
    nlohmann::json result = {};
    for (const auto &[key, new_value] : new_data.items()) {

        // If this key does not exist in the previous object, copy the new value
        // into the result if it is not trivial (null, empty object, or empty
        // array). Otherwise, perform delta-encoding recursively.
        const auto previous_value_it = previous_data.find(key);
        nlohmann::json delta;
        if (previous_value_it == previous_data.end()) {
            delta = new_value;
        } else {
            delta = json_delta_encode(new_value, *previous_value_it);
        }

        // Insert the item only if it carries data (not null, an empty array,
        // or an empty object).
        if (!delta.empty()) {
            result[key] = delta;
        }
    }

    // Check for keys that exist in previous_data but not in new_data. Insert
    // nulls for those to signal the removal.
    for (const auto &[key, previous_value] : previous_data.items()) {
        if (!new_data.count(key)) {
            result[key] = nullptr;
        }
    }

    return result;
}

/// Applies some special post-processing operator to JSON data.
static nlohmann::json json_apply_operator(
    const nlohmann::json &data, const std::string &op
) {

    // What would Python bool() do? Except null stays null, to indicate invalid
    // data.
    if (op == CONFIG_OPERATOR_BOOL || op == CONFIG_OPERATOR_ZERO) {
        bool boolified;
        switch (data.type()) {
            case nlohmann::detail::value_t::null:
                return nullptr;
            case nlohmann::detail::value_t::boolean:
                boolified = data.get<nlohmann::json::boolean_t>();
                break;
            case nlohmann::detail::value_t::number_integer:
                boolified = data.get<nlohmann::json::number_integer_t>() != 0;
                break;
            case nlohmann::detail::value_t::number_unsigned:
                boolified = data.get<nlohmann::json::number_unsigned_t>() != 0;
                break;
            case nlohmann::detail::value_t::number_float:
                boolified =
                    round(data.get<nlohmann::json::number_float_t>()) != 0.0;
                break;
            case nlohmann::detail::value_t::string:
            case nlohmann::detail::value_t::binary:
                boolified = !data.get<std::string>().empty();
                break;
            default:
                boolified = !data.empty();
        }
        if (op == CONFIG_OPERATOR_ZERO) return !boolified;
        return boolified;
    }

    // Rounding operator. Returns a float instead of an integer because floats
    // can be considerably larger than integers.
    if (op == CONFIG_OPERATOR_ROUND) {
        if (!data.is_number()) {
            return nullptr;
        }
        return round(data.get<double>());
    }

    // Formats a game time value in minutes as an ISO 8601 date/time value
    // (0001-01-01T10:11:00Z).
    if (op == CONFIG_OPERATOR_DATE) {
        if (!data.is_number_unsigned()) {
            return nullptr;
        }
        const auto minutes = data.get<nlohmann::json::number_unsigned_t>();
        auto timestamp = static_cast<time_t>(minutes * 60);

        // The C library time functions don't like dates before 1970, yet other
        // plugins start counting game time at year 1 for some reason. I guess
        // that makes some level of sense. But it is very annoying. We'll start
        // our count at 2001 to make leap year calculation work (ish) for year
        // 1, and then subtract two from the leading zero.
        static constexpr auto BEGINNING_OF_2001 = 978307200;
        timestamp += BEGINNING_OF_2001;

        // Format the date...
        struct tm datetime = {};
#if defined(_MSC_VER)
        gmtime_s(&datetime, &timestamp);
#else
        gmtime_r(&timestamp, &datetime);
#endif
        char buf[32];
        strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &datetime);

        // ...and decrement the leading two.
        buf[0] -= 2;

        return buf;
    }

    return "unknown operator " + op;
}

/// Applies formatting logic to obtain a single value. Throws if the format is
/// invalid.
static nlohmann::json json_format(
    const nlohmann::json &format, const nlohmann::json &data
) {

    // Handle static values.
    auto it = format.find(CONFIG_FORMAT_STATIC);
    if (it != format.end()) return *it;

    // Anything that isn't a static value must take data from some key.
    const auto key = format.at(CONFIG_FORMAT_KEY).get<std::string>();
    auto resolved = json_resolve_path(data, key);

    // If the struct key is present and true, we yield the data as we found it.
    // Otherwise, we'll resolve the _ key if the resolved value is an object.
    // This ensures that we only return data for exact matches of the given key
    // when data was generated with json_assign_path(..., flatten=false).
    if (resolved.is_object()) {
        it = format.find(CONFIG_FORMAT_STRUCT);
        if (it == format.end() || !it->get<bool>()) {
            it = resolved.find("_");
            if (it == resolved.end()) {
                resolved = nullptr;
            } else {
                resolved = *it;
            }
        }
    }

    // Optionally, numeric values can be offset or scaled, e.g. to convert m/s
    // to km/h, rotations to degrees, or ratios to percentages.
    if (resolved.is_number()) {
        it = format.find(CONFIG_FORMAT_OFFSET);
        if (it != format.end()) {
            resolved = resolved.get<double>() + it->get<double>();
        }
        it = format.find(CONFIG_FORMAT_SCALE);
        if (it != format.end()) {
            resolved = resolved.get<double>() * it->get<double>();
        }
    }

    // Optionally, certain operators can be applied. These can help with
    // compatibility with other plugins or with reducing traffic of delta-coded
    // streams.
    it = format.find(CONFIG_FORMAT_OPERATOR);
    if (it != format.end()) {
        const auto op = it->get<std::string>();
        resolved = json_apply_operator(resolved, op);
    }

    return resolved;
}

void json_restructure(nlohmann::json &format, const nlohmann::json &data) {

    // Arrays are handled recursively.
    if (format.is_array()) {
        for (auto &format_item : format) {
            json_restructure(format_item, data);
        }
        return;
    }

    // Anything that isn't an object at this point is left alone, though this
    // shouldn't really happen.
    if (!format.is_object()) return;

    // JSON objects can be *either* structure or a format object. Format objects
    // are identified by having only scalar keys.
    bool is_format = true;
    for (const auto &format_item : format) {
        if (format_item.is_structured()) {
            is_format = false;
            break;
        }
    }

    // If our object is not a format object, handle the object recursively.
    if (!is_format) {
        for (auto &format_item : format) {
            json_restructure(format_item, data);
        }
        return;
    }

    // We now have ourselves a format description object. Try to format it. If
    // this fails, emit the error message.
    try {
        format = json_format(format, data);
    } catch (const nlohmann::json::exception &e) {
        format = e.what();
    }
}

nlohmann::json yaml_to_json(const fkyaml::node &yaml) {
    switch (yaml.get_type()) {
        case fkyaml::node_type::NULL_OBJECT:
            return nullptr;
        case fkyaml::node_type::BOOLEAN:
            return yaml.get_value<fkyaml::node::boolean_type>();
        case fkyaml::node_type::INTEGER:
            return yaml.get_value<fkyaml::node::integer_type>();
        case fkyaml::node_type::FLOAT:
            return yaml.get_value<fkyaml::node::float_number_type>();
        case fkyaml::node_type::STRING:
            return yaml.get_value<fkyaml::node::string_type>();
        case fkyaml::node_type::SEQUENCE: {
            nlohmann::json result = nlohmann::json::array();
            for (const auto &yaml_item : yaml) {
                result.emplace_back(yaml_to_json(yaml_item));
            }
            return result;
        }
        case fkyaml::node_type::MAPPING: {
            nlohmann::json result = nlohmann::json::object();
            for (const auto &yaml_item : yaml.map_items()) {
                result[yaml_item.key().get_value<std::string>()] =
                    yaml_to_json(*yaml_item);
            }
            return result;
        }
    }
    return nullptr;
}
