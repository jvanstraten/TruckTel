#pragma once

#include <cstdint>
#include <mutex>

#include <scssdk.h>

#include <nlohmann/json.hpp>

#include "json_utils.h"

/// Recorder for configuration data. This does not change often, and is
/// reported to clients only when it is updated. The basic data structure is a
/// JSON map. Whenever the SCS API side updates a key, a version ID is
/// incremented to indicate that the structure has changed.
class ConfigurationRecorder {
private:
    /// The current configuration state.
    std::map<std::string, std::vector<NamedValue>> data;

    /// The current version.
    uint64_t current_version = 0;

    /// The mutex that protects all of the above.
    std::mutex mutex;

public:
    /// Updates the given configuration key.
    void push(const std::string &key, std::vector<NamedValue> values);

    /// Polls for data. The client doing the polling should keep track of the
    /// version variable; it is used to avoid updating the structure when
    /// nothing has changed since the previous call. Returns whether the buffer
    /// was updated.
    bool poll(
        std::map<std::string, std::vector<NamedValue>> &buffer,
        uint64_t &version
    );
};
