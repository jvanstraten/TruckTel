#include "config.h"

#include <fstream>
#include <sstream>

#include <fkYAML/node.hpp>

#include "version.h"

/// Returns whether a given character is a valid hostname character.
static bool is_valid_hostname(const char c) {
    return c == '_' || c == '-' || (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/// Maps a given character to a valid hostname character.
static char map_to_valid_hostname(char c) {
    if (!is_valid_hostname(c)) return '-';
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return c;
}

MdnsConfiguration::MdnsConfiguration(
    const std::filesystem::path &trucktel_path
) {
    // Get configuration file path.
    const auto config_path = trucktel_path / CONFIG_FILENAME;

    // Generate a default configuration file if no configuration file exists
    // yet.
    if (!std::filesystem::exists(config_path)) {
        std::ofstream ofs;
        ofs.open(config_path.string().c_str());
        ofs << "---" << std::endl;
        ofs << "# Whether the mDNS service should be enabled. This is what "
               "allows you to"
            << std::endl;
        ofs << "# navigate to e.g. 'trucktel.local' in your browser, instead "
               "of to an IP"
            << std::endl;
        ofs << "# address. But you can disable it if your firewall screams "
               "bloody murder,"
            << std::endl;
        ofs << "# or you know what you're doing and just don't want mDNS."
            << std::endl;
        ofs << CONFIG_KEY_ENABLE << ": true" << std::endl;
        ofs << std::endl;
        ofs << "# The hostname that TruckTel should respond to. This is the "
               "'trucktel' part in"
            << std::endl;
        ofs << "# 'trucktel.local'. It must be unique on your network, so "
               "change this if you're"
            << std::endl;
        ofs << "# doing a LAN party or something." << std::endl;
        ofs << CONFIG_KEY_HOSTNAME << ": " << TRUCKTEL_DIRECTORY_NAME
            << std::endl;
        ofs << std::endl;
        ofs << "# Enable verbose logging for the mDNS subsystem." << std::endl;
        ofs << CONFIG_KEY_VERBOSE << ": false" << std::endl;
        ofs.close();
    }

    // Load the configuration file.
    std::ifstream ifs;
    ifs.open(config_path.string().c_str());
    if (!ifs.is_open()) {
        throw std::runtime_error("failed to load default configuration file");
    }
    try {
        auto yaml = fkyaml::node::deserialize(ifs);
        enable_mdns = yaml[CONFIG_KEY_ENABLE].get_value<bool>();
        hostname = yaml[CONFIG_KEY_HOSTNAME].get_value<std::string>();
        qualified_hostname = hostname + "." + MDNS_DOMAIN + ".";
        verbose = yaml[CONFIG_KEY_VERBOSE].get_value<bool>();
        for (const auto c : hostname) {
            if (!is_valid_hostname(c)) {
                throw std::runtime_error(
                    std::string("invalid character in mDNS hostname: '") + c +
                    "'"
                );
            }
        }
    } catch (std::exception &e) {
        throw std::runtime_error(
            "failed to parse mDNS configuration file: " + std::string(e.what())
        );
    }
}

void MdnsConfiguration::register_app(
    const std::string &app, const uint16_t port
) {

    // Form service and service instance strings.
    const auto service = app_to_service(app);
    const auto service_instance = service_to_instance(service);

    // Check uniqueness.
    if (port_to_app.count(port) || service_to_port.count(service) ||
        service_instance_to_port.count(service_instance)) {
        throw std::runtime_error("app name or port is not unique");
    }

    // Register.
    port_to_app[port] = app;
    service_to_port[service] = port;
    service_instance_to_port[service_instance] = port;
}

bool MdnsConfiguration::is_mdns_enabled() const {
    return enable_mdns;
}

bool MdnsConfiguration::is_verbose() const {
    return verbose;
}

const std::map<uint16_t, std::string> &MdnsConfiguration::
    get_port_to_app() const {
    return port_to_app;
}

std::string MdnsConfiguration::app_to_service(const std::string &app) const {
    std::ostringstream ss{};
    ss << PREFIX;
    for (const auto c : app) {
        ss << map_to_valid_hostname(c);
    }
    ss << '.' << PROTOCOL << '.' << MDNS_DOMAIN << '.';
    return ss.str();
}

std::string MdnsConfiguration::service_to_instance(
    const std::string &service
) const {
    return hostname + '.' + service;
}

const std::map<std::string, uint16_t> &MdnsConfiguration::
    get_service_to_port() const {
    return service_to_port;
}

const std::map<std::string, uint16_t> &MdnsConfiguration::
    get_service_instance_to_port() const {
    return service_instance_to_port;
}

const std::string &MdnsConfiguration::get_hostname() const {
    return hostname;
}

const std::string &MdnsConfiguration::get_qualified_hostname() const {
    return qualified_hostname;
}