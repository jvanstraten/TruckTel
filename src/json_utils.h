#pragma once

#include <scssdk_value.h>

#include <nlohmann/json.hpp>

/// Assigns data to the hierarchical key identified by path in json. The
/// path separator is a period. This tries to be smarter than it really
/// should be, because the paths used by the SCS API are not uniform enough to
/// work with a naive implementation: non-leaf paths can have data in them.
/// Whenever that happens, the data of a non-leaf node is put in a node with
/// key "_". Whenever this smartness doesn't work, the flatten flag can be set
/// to basically ignore separators in the path, making this function mostly
/// trivial.
void json_assign_path(
    nlohmann::json &json,
    const std::string &path,
    const nlohmann::json &data,
    bool flatten
);

/// Converts an SCS version number to a JSON array.
nlohmann::json scs_version_to_json(scs_u32_t version);

/// Converts an scs_value_t variant into an equivalent JSON form.
nlohmann::json scs_value_to_json(const scs_value_t &value);

/// Owned mirror of scs_named_value_t.
struct NamedValue {
    /// Owned attribute name.
    std::string name;

    /// Index for array values, or SCS_U32_NIL for scalars.
    scs_u32_t index;

    /// JSON representation of the scs_value_t value.
    nlohmann::json value;

    /// Returns a special NamedValue that represents the ID of an event.
    static NamedValue scalar(std::string name, nlohmann::json value);

    /// Returns a special NamedValue that represents the ID of an event.
    static NamedValue event_id(const std::string &event_id);
};

/// Converts an array of named values received from the SCS telemetry API to
/// an owned copy.
std::vector<NamedValue> copy_scs_attributes(
    const scs_named_value_t *attributes
);

/// Converts a vector of named values to JSON. This first unflattens indexed
/// values, and then uses json_assign_path(..., flatten) to construct the JSON
/// object.
nlohmann::json named_values_to_json(
    const std::vector<NamedValue> &data, bool flatten
);

/// Performs delta-encoding of JSON objects. The result will have the data from
/// new_data, except (recursively until a non-object is encountered):
///  - object items that are equal in both new_data and previous_data are
///    omitted;
///  - object items that exist in previous_data but not in new_data are returned
///    mapping to null.
nlohmann::json delta_encode(
    const nlohmann::json &new_data, const nlohmann::json &previous_data
);
