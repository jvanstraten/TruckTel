#pragma once

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

/// Configuration for the server.
struct ServerConfig {
    /// Port to listen on.
    uint16_t port;

    /// Document root to serve from. Empty disables serving static files.
    std::filesystem::path document_root;

    /// Filename to content-type mapping.
    std::vector<std::pair<std::regex, std::string>> content_types;
};
