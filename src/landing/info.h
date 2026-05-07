#pragma once

#include <cinttypes>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

/// Data structure with information about an app that loaded successfully.
struct LandingAppInfo {
    /// Friendly title from the app's config.yaml, or empty if undefined.
    std::string title;

    /// Friendly subtitle from the app's config.yaml, or empty if undefined.
    std::string subtitle;

    /// Link specified in the app's config.yaml (e.g. github or SCS forums), or
    /// empty if undefined.
    std::string link;

    /// The port that the app is configured to run from.
    uint16_t port = 0;

    /// Error message if the app's server thread has failed to start, or empty
    /// if there is no issue.
    std::string error_message;
};

/// Data structure with information about TruckTel as a whole.
struct LandingInfo {
    /// Full name and version of the host.
    std::string game_version;

    /// Detected local IP address. Prefers IPv4, then uses IPv6. Empty if
    /// neither address type was detected.
    std::string local_ip_address;

    /// Hostname including .local suffix announced via mDNS. Empty if mDNS is
    /// disabled.
    std::string mdns_hostname;

    /// Map from app working directory to information object.
    std::map<std::string, LandingAppInfo> apps;
};

/// Default page served by the landing server for a directory.
static constexpr auto LANDING_INDEX = "index.html";

/// Path of the JSON information resource.
static constexpr auto LANDING_API_PATH = "/api/info.json";

/// Content type of the JSON information resource.
static constexpr auto LANDING_API_CONTENT_TYPE =
    "application/json; charset=utf-8";

// Keys used by the JSON serialization of LandingPluginInfo.
static constexpr auto LANDING_API_PLUGIN_VERSION = "pluginVersion";
static constexpr auto LANDING_API_GAME_VERSION = "gameVersion";
static constexpr auto LANDING_API_LOCAL_IP_ADDRESS = "localIpAddress";
static constexpr auto LANDING_API_MDNS_HOSTNAME = "mdnsHostname";
static constexpr auto LANDING_API_APPS = "apps";
static constexpr auto LANDING_API_APP_DIRECTORY = "appDirectory";
static constexpr auto LANDING_API_TITLE = "title";
static constexpr auto LANDING_API_SUBTITLE = "subtitle";
static constexpr auto LANDING_API_LINK = "link";
static constexpr auto LANDING_API_PORT = "port";
static constexpr auto LANDING_API_ERROR_MESSAGE = "errorMessage";

/// Serializes the landing plugin information structure to JSON.
nlohmann::json serialize_landing_info(const LandingInfo &info);