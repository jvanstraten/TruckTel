#pragma once

#include <map>
#include <vector>

#include <scssdk_value.h>

#include <nlohmann/json.hpp>

#include "recorder/channel.h"

/// Unified view of the current data provided by the game, tracked centrally in
/// the server thread.
class Database {
private:
    /// The dataset in which a value is stored.
    enum struct ValueSource {
        /// For values that logically should exist, but we don't have a source
        /// for. This can happen in theory because the SCS telemetry API returns
        /// indexed values individually; it could e.g. report only the tenth
        /// element of an indexed value, in which case we need to insert nine
        /// null items into the array in JSON to make things make sense.
        NONE,

        /// The value should be taken from the channel dataset.
        CHANNEL,

        /// The value should be taken from the configuration dataset.
        CONFIGURATION,
    };

    /// The place where a value is stored.
    struct ValueIndex {
        /// Which dataset the value comes from.
        ValueSource source = ValueSource::NONE;

        /// The index in the flattened array corresponding to the dataset.
        size_t index = 0;
    };

    /// Indices in the flattened arrays for a single value.
    struct ValueIndices {
        /// Index for non-indexed values.
        ValueIndex scalar = {};

        /// Indices for indexed values.
        std::vector<ValueIndex> vector;
    };

    /// Map for indices in the associated data array. The first element in the
    /// value is for non-indexed values, the second is for array data. Any index
    /// that is unknown is set to UNSET.
    std::map<std::string, ValueIndices> index;

    /// Raw values of all the channels monitored by TruckTel. Updated via
    /// Recorder::channel_poll().
    std::vector<scs_value_t> channel_data;

    /// Raw values of all configuration attributes.
    std::vector<nlohmann::json> configuration_data;

    /// Version of the configuration data.
    uint64_t configuration_version = 0;

    /// Custom data structures configured by the user.
    nlohmann::json custom_structures;

    /// Builds JSON for a scalar based on its indix.
    [[nodiscard]] nlohmann::json get_json_for(
        const ValueIndex &value_index
    ) const;

    /// Builds JSON for a single key based on its indices.
    [[nodiscard]] nlohmann::json get_json_for(
        const ValueIndices &value_indices
    ) const;

    /// Returns a reference to the ValueIndex in the index that corresponds to
    /// the given key and value, adding to the index if necessary.
    ValueIndex &get_value_index_for(
        const std::string &key, scs_u32_t array_index
    );

    /// Initializes the index for channel data.
    void initialize_channel_index();

    /// Polls channel data from the recorder to update our copy of the data.
    void update_channels();

    /// Updates a single configuration value.
    void update_configuration_item(
        const std::string &key,
        scs_u32_t array_index,
        const nlohmann::json &value
    );

    /// Polls configuration data from the recorder to update our copy of the
    /// data.
    void update_configuration();

public:
    /// Poll the recorder to update the data in the database.
    void update();

    /// Returns JSON data for a single value. Returns null if the value is not
    /// in the database.
    [[nodiscard]] nlohmann::json get_data_single(const std::string &key) const;

    /// Returns JSON data for values matching the given prefix. Returns an empty
    /// object if nothing matches. If flatten is false, an attempt is made to
    /// return the data in a sensible structured form, but there might be
    /// conflicts in that case if SCS defines a key to refer both to a value and
    /// a node containing subkeys. In that case, the value of non-leaf nodes is
    /// punted to a leaf node with a _ key. To avoid all these shenanigans,
    /// flatten can be set to true to get a single JSON object with all the
    /// keys in it directly, irrespective of hierarchy. But this costs more
    /// bandwidth and might be more annoying to work with.
    [[nodiscard]] nlohmann::json get_data_multi(
        const std::string &prefix, bool flatten
    ) const;

    /// Configures custom data structures.
    void set_custom_structures(nlohmann::json new_custom_structures);

    /// Returns data for the given API query.
    [[nodiscard]] nlohmann::json get_data(
        const std::vector<std::string> &query
    ) const;
};
