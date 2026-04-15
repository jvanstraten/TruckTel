#pragma once

#include <filesystem>
#include <regex>
#include <stdexcept>
#include <string>

#include "database.h"
#include "url.h"
#include "wspp_config.h"

/// MIME type for HTML results.
static constexpr auto CONTENT_TYPE_HTML = "text/html; charset=utf-8";

/// MIME type for JSON results.
static constexpr auto CONTENT_TYPE_JSON = "application/json; charset=utf-8";

/// Default MIME type when no content-type regex matches.
static constexpr auto CONTENT_TYPE_UNKNOWN = "application/octet-stream";

/// A complete HTTP response, insofar as we support HTTP.
struct HttpResponse {
    /// Response code.
    wspp::http::status_code::value code = wspp::http::status_code::ok;

    /// Content type.
    std::string content_type = CONTENT_TYPE_HTML;

    /// Response body.
    std::string body;

    /// Constructs a JSON response.
    static HttpResponse from_json(const nlohmann::json &json);
};

/// Class for handling HTTP requests.
class HttpHandler {
private:
    /// Exception class used to return 404 errors.
    struct FileNotFound : std::runtime_error {
        explicit FileNotFound(const std::string &path)
            : std::runtime_error(path) {}
    };

    /// Document root for serving static files.
    std::filesystem::path document_root;

    /// Filename regex to content-type mapping.
    std::vector<std::pair<std::regex, std::string>> content_types;

    /// Database to serve REST values from.
    Database *database = nullptr;

    /// Handles serving static pages from the document root.
    [[nodiscard]] HttpResponse handle_static(const Url &url) const;

    /// Handles serving REST requests.
    [[nodiscard]] HttpResponse handle_rest(const Url &url) const;

    /// Handles HTTP requests to websocket endpoints.
    [[nodiscard]] HttpResponse handle_websocket(const Url &url) const;

    /// Handles serving error pages.
    [[nodiscard]] HttpResponse handle_error(
        wspp::http::status_code::value code,
        const std::string &raw_message,
        const std::string &pretty_message
    ) const;

public:
    /// Configures the server.
    void configure(
        const std::filesystem::path &new_document_root,
        const std::vector<std::pair<std::regex, std::string>>
            &new_content_types,
        Database &new_database
    );

    /// Main function for handling an HTTP request.
    [[nodiscard]] HttpResponse handle_http(const std::string &resource) const;
};
