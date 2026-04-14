#include "database.h"

#include "recorder/recorder.h"

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
