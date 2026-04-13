#include "versioned.h"

void VersionedRecorder::push(const std::string &key, nlohmann::json value) {
    std::lock_guard guard(mutex);

    // Update the data structure.
    if (value.is_null()) {
        data.erase(key);
    } else {
        data[key] = std::move(value);
    }

    // Increment the version number.
    current_version++;
}

nlohmann::json VersionedRecorder::poll(uint64_t &version) {
    std::lock_guard guard(mutex);
    if (version == current_version) return nullptr;
    version = current_version;
    return data;
}

nlohmann::json VersionedRecorder::poll() {
    std::lock_guard guard(mutex);
    return data;
}
