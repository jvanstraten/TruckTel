#pragma once

#include <cstdint>
#include <filesystem>

/// Configures the landing server.
class LandingConfiguration {
private:
    /// mDNS configuration file name.
    static constexpr auto CONFIG_FILENAME = "landing.yaml";

    /// Whether to enable the landing page subsystem.
    static constexpr auto CONFIG_KEY_ENABLE = "enable";

    /// Which port to listen on.
    static constexpr auto CONFIG_KEY_PORT = "port";

    /// Which port to listen on by default.
    static constexpr auto CONFIG_DEFAULT_PORT = 8079;

    /// Whether to open the landing page in a browser on plugin load.
    static constexpr auto CONFIG_KEY_AUTO_OPEN = "auto-open";

    /// TruckTel installation directory.
    const std::filesystem::path trucktel_path;

    /// Whether to enable the landing subsystem.
    bool enabled = false;

    /// Which port to listen on.
    uint16_t port = 0;

    /// Whether to open the landing page in a browser on plugin load.
    bool auto_open = false;

public:
    /// Constructs a landing server configuration by loading the configuration
    /// file in the trucktel_path, or generating a default one if none exists.
    explicit LandingConfiguration(const std::filesystem::path &trucktel_path);

    /// TruckTel installation directory.
    [[nodiscard]] const std::filesystem::path &get_trucktel_path() const;

    /// Whether the landing server should be enabled.
    [[nodiscard]] bool is_enabled() const;

    /// Which port the landing server should listen on.
    [[nodiscard]] uint16_t get_port() const;

    /// Whether to open the landing page in a browser on plugin load.
    [[nodiscard]] bool is_auto_open_enabled() const;
};
