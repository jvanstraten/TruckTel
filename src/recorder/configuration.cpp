#include "configuration.h"

void ConfigurationRecorder::push(
    const std::string &key, std::vector<NamedValue> values
) {
    std::lock_guard guard(mutex);
    data[key] = std::move(values);
    current_version++;
}

bool ConfigurationRecorder::poll(
    std::map<std::string, std::vector<NamedValue>> &buffer, uint64_t &version
) {
    std::lock_guard guard(mutex);
    if (version == current_version) return false;
    buffer = data;
    version = current_version;
    return true;
}
