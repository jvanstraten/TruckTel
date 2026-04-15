#include "database.h"

#include <sstream>

#include "api.h"
#include "logger.h"
#include "recorder/recorder.h"

nlohmann::json Database::get_json_for(const ValueIndex &value_index) const {
    switch (value_index.source) {
        case ValueSource::CHANNEL:
            return scs_value_to_json(channel_data.at(value_index.index));
        case ValueSource::CONFIGURATION:
            return configuration_data.at(value_index.index);
        default:
            return nullptr;
    }
}

nlohmann::json Database::get_json_for(const ValueIndices &value_indices) const {
    if (value_indices.scalar.source != ValueSource::NONE) {
        return get_json_for(value_indices.scalar);
    }
    nlohmann::json result = nlohmann::json::array();
    for (size_t i = 0; i < value_indices.vector.size(); i++) {
        auto j = get_json_for(value_indices.vector[i]);
        if (j.is_null()) continue;
        while (result.size() < i)
            result.emplace_back(nullptr);
        result.emplace_back(j);
    }
    if (result.empty()) return nullptr;
    return result;
}

Database::ValueIndex &Database::get_value_index_for(
    const std::string &key, const scs_u32_t array_index
) {
    // Make sure the key exists.
    auto [it, inserted] = index.emplace(key, ValueIndices());

    // Handle scalar values.
    if (array_index == SCS_U32_NIL) {
        return it->second.scalar;
    }

    // For array value, make sure the array is large enough.
    while (it->second.vector.size() <= array_index) {
        it->second.vector.emplace_back();
    }

    // Handle array values.
    return it->second.vector[array_index];
}

void Database::initialize_channel_index() {
    const auto &metadata = Recorder::channel_metadata();
    for (size_t i = 0; i < metadata.size(); i++) {
        auto &value_index =
            get_value_index_for(metadata[i].name, metadata[i].index);
        value_index.source = ValueSource::CHANNEL;
        value_index.index = i;
    }
}

void Database::update_channels() {
    // If we don't have channel data yet, build the index.
    if (channel_data.empty()) initialize_channel_index();

    // Poll the data.
    Recorder::channel_poll(channel_data);
}

void Database::update_configuration_item(
    const std::string &key,
    const scs_u32_t array_index,
    const nlohmann::json &value
) {
    // Make sure the logical value exists in the index, and get a pointer to the
    // value index.
    auto &value_index = get_value_index_for(key, array_index);

    // Extend the configuration data vector if necessary.
    switch (value_index.source) {
        case ValueSource::NONE:
            // New configuration item: extend the flattened vector.
            value_index.source = ValueSource::CONFIGURATION;
            value_index.index = configuration_data.size();
            configuration_data.emplace_back();
            break;
        case ValueSource::CHANNEL:
            // Conflict between configuration and channel, apparently?
            // Ignore the configuration value, channel takes precedence.
            return;
        case ValueSource::CONFIGURATION:
            // Configuration item already exists, don't need to add it.
            break;
    }

    // Actually save the configuration value.
    configuration_data[value_index.index] = value;
}

void Database::update_configuration() {
    // Fetch configuration data, exiting early if nothing has changed since the
    // previous call.
    std::map<std::string, std::vector<NamedValue>> data;
    if (!Recorder::configuration_poll(data, configuration_version)) return;

    // Reset all the configuration keys before rebuilding the array. This
    // ensures that configuration data that was invalidated by the game is also
    // invalidated here.
    for (auto &item : configuration_data) {
        item = nullptr;
    }

    // Iterate over all the configuration values.
    for (const auto &[configuration_key, attributes] : data) {
        for (const auto &attribute : attributes) {
            std::string key = configuration_key + "." + attribute.name;
            update_configuration_item(key, attribute.index, attribute.value);
        }
    }
}

void Database::update() {
    update_channels();
    update_configuration();
}

nlohmann::json Database::get_data_single(const std::string &key) const {
    auto it = index.find(key);
    if (it == index.end()) return nullptr;
    return get_json_for(it->second);
}

nlohmann::json Database::get_data_multi(
    const std::string &prefix, const bool flatten
) const {
    nlohmann::json result;
    const std::string ext_prefix = prefix.empty() ? "" : prefix + ".";
    for (const auto &[key, indices] : index) {
        // Check if the key matches the prefix.
        if (key != prefix && key.substr(0, ext_prefix.size()) != ext_prefix) {
            continue;
        }

        // Get the value and check that it isn't null.
        auto value = get_data_single(key);
        if (value.is_null()) continue;

        // Add the value to the result.
        json_assign_path(result, key, value, flatten);
    }
    return result;
}

nlohmann::json Database::get_data(const std::vector<std::string> &query) const {
    // The first element of the query specifies the structuring of the data.
    if (query.empty()) return "missing structure";
    const auto structure = query.front();

    // The remaining elements are joined with periods to form a period-separated
    // path.
    std::ostringstream prefix{};
    for (size_t i = 1; i < query.size(); i++) {
        if (i > 1) prefix << ".";
        prefix << query[i];
    }

    // Handle methods.
    if (structure == API_STRUCTURE_SINGLE) {
        return get_data_single(prefix.str());
    }
    if (structure == API_STRUCTURE_STRUCT) {
        return get_data_multi(prefix.str(), false);
    }
    if (structure == API_STRUCTURE_FLAT) {
        return get_data_multi(prefix.str(), true);
    }
    return "unrecognized method " + structure;
}
