#include "info.h"

#include "version.h"

static nlohmann::json serialize_string_or_null(const std::string &s) {
    if (s.empty()) return nullptr;
    return s;
}

static nlohmann::json serialize_app_info(
    const std::string &app_directory, const LandingAppInfo &info
) {
    nlohmann::json data = {};
    data[LANDING_API_APP_DIRECTORY] = app_directory;
    data[LANDING_API_TITLE] = serialize_string_or_null(info.title);
    data[LANDING_API_SUBTITLE] = serialize_string_or_null(info.subtitle);
    data[LANDING_API_LINK] = serialize_string_or_null(info.link);
    data[LANDING_API_PORT] = info.port;
    data[LANDING_API_ERROR_MESSAGE] =
        serialize_string_or_null(info.error_message);
    return data;
}

nlohmann::json serialize_landing_info(const LandingInfo &info) {
    nlohmann::json apps = nlohmann::json::array();
    for (const auto &[app_directory, app_info] : info.apps) {
        apps.emplace_back(serialize_app_info(app_directory, app_info));
    }

    nlohmann::json data = {};
    data[LANDING_API_PLUGIN_VERSION] = TRUCKTEL_FULL_NAME;
    data[LANDING_API_GAME_VERSION] = info.game_version;
    data[LANDING_API_LOCAL_IP_ADDRESS] =
        serialize_string_or_null(info.local_ip_address);
    data[LANDING_API_MDNS_HOSTNAME] =
        serialize_string_or_null(info.mdns_hostname);
    data[LANDING_API_APPS] = std::move(apps);
    return data;
}
