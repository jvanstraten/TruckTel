#pragma once

// Dependencies.
#include <nlohmann/json.hpp>
#include <scssdk_value.h>

/// Assigns data to the hierarchical key identified by path in json. The
/// path separator is a period. This tries to be smarter than it really
/// should be, because the paths used by the SCS API are not uniform enough to
/// work with a naive implementation: non-leaf paths can have data in them.
/// Whenever that happens, the data of a non-leaf node is put in a node with
/// key "_".
void json_assign_path(
    nlohmann::json &json, const std::string &path, const nlohmann::json &data
);

/// Converts an SCS version number to a JSON array.
nlohmann::json scs_version_to_json(scs_u32_t version);

/// Converts an scs_value_t variant into an equivalent JSON form.
nlohmann::json scs_value_to_json(const scs_value_t &value);

/// Converts an array of named attributes to a JSON structure.
nlohmann::json scs_attributes_to_json(const scs_named_value_t *attributes);
