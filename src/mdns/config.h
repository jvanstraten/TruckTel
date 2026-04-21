#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

/// Configures the mDNS server.
class MdnsConfiguration {
private:
    /// mDNS configuration file name.
    static constexpr auto CONFIG_FILENAME = "mdns.yaml";

    /// Whether to enable the mDNS subsystem.
    static constexpr auto CONFIG_KEY_ENABLE = "enable";

    /// Whether to enable the redirection HTTP server on port 80.
    static constexpr auto CONFIG_KEY_LANDING = "landing";

    /// Which hostname to advertise via mDNS.
    static constexpr auto CONFIG_KEY_HOSTNAME = "hostname";

    /// Whether to enable verbose logging.
    static constexpr auto CONFIG_KEY_VERBOSE = "verbose";

    /// Domain name to use.
    static constexpr auto MDNS_DOMAIN = "local";

    /// App prefix to use.
    static constexpr auto PREFIX = "_trucktel-";

    /// Protocol to announce.
    static constexpr auto PROTOCOL = "_tcp";

    /// Whether to enable mDNS.
    bool enable_mdns;

    /// Whether to enable the landing server.
    bool enable_landing;

    /// Whether verbose logging is enabled.
    bool verbose;

    /// Hostname to use.
    std::string hostname;

    /// Qualified hostname (= "<hostname>.local.").
    std::string qualified_hostname;

    /// Map from port to app name.
    std::map<uint16_t, std::string> port_to_app;

    /// Map from service type (= "_trucktel-<app>._tcp.local.") to port number.
    std::map<std::string, uint16_t> service_to_port;

    /// Map from service instance (= "<hostname>._trucktel-<app>._tcp.local.")
    /// to port number.
    std::map<std::string, uint16_t> service_instance_to_port;

public:
    /// Constructs an mDNS configuration by loading the configuration file in
    /// the trucktel_path, or generating a default one if none exists.
    explicit MdnsConfiguration(const std::filesystem::path &trucktel_path);

    /// Registers an app. Both name and port must be unique. Anything in the app
    /// name that isn't a dash, underscore, or ASCII alphanumeric character is
    /// converted to a dash in the construction of the app's service type.
    void register_app(const std::string &app, uint16_t port);

    /// Whether mDNS should be enabled.
    [[nodiscard]] bool is_mdns_enabled() const;

    /// Whether the landing server should be enabled.
    [[nodiscard]] bool is_landing_enabled() const;

    /// Whether verbose logging is enabled.
    [[nodiscard]] bool is_verbose() const;

    /// Map from port to app name.
    [[nodiscard]] const std::map<uint16_t, std::string> &get_port_to_app() const;

    /// Convert app name to its service type.
    [[nodiscard]] std::string app_to_service(const std::string &app) const;

    /// Convert service type to instance.
    [[nodiscard]] std::string service_to_instance(
        const std::string &service
    ) const;

    /// Map from service type (= "_trucktel-<app>._tcp.local.") to port number.
    [[nodiscard]] const std::map<std::string, uint16_t> &
    get_service_to_port() const;

    /// Map from service instance (= "<hostname>._trucktel-<app>._tcp.local.")
    /// to port number.
    [[nodiscard]] const std::map<std::string, uint16_t> &
    get_service_instance_to_port() const;

    /// Returns the hostname.
    [[nodiscard]] const std::string &get_hostname() const;

    /// Returns the qualified hostname (= "<hostname>.local.").
    [[nodiscard]] const std::string &get_qualified_hostname() const;
};
