#pragma once

// Standard libraries.
#include <cstdint>
#include <mutex>

// Dependencies.
#include <nlohmann/json.hpp>

/// Recorder for data that does not change often, and is reported to clients
/// only when it is updated. The basic data structure is a JSON map. Whenever
/// the SCS API side updates a key, a version ID is incremented to indicate
/// that the structure has changed.
class VersionedRecorder {
private:
    /// The current state of the data structure.
    nlohmann::json data;

    /// The current version.
    uint64_t current_version = 0;

    /// The mutex that protects all of the above.
    std::mutex mutex;

public:
    /// Updates the given key of the toplevel JSON structure to the given value,
    /// while incrementing the version number. If value is null, the key is
    /// removed if it previously existed.
    void push(const std::string &key, nlohmann::json value);

    /// Polls for data. The client doing the polling should keep track of the
    /// version variable; it is used to avoid sending the structure when nothing
    /// has changed since the previous call. The result is the JSON structure
    /// associated with this recorder if anything has changed, or null if the
    /// data has not changed since the previous call.
    nlohmann::json poll(uint64_t &version);

    /// Polls for data without a version check. This always returns a copy of the
    /// latest data.
    nlohmann::json poll();
};
