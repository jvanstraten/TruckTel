#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "url.h"
#include "wspp_config.h"

/// MIME type for HTML results.
static constexpr auto CONTENT_TYPE_HTML = "text/html; charset=utf-8";

/// MIME type for JSON results.
static constexpr auto CONTENT_TYPE_JSON = "application/json; charset=utf-8";

/// A complete HTTP response, insofar as we support HTTP.
struct HttpResponse {
    /// Response code.
    wspp::http::status_code::value code = wspp::http::status_code::ok;

    /// Content type.
    const char *content_type = CONTENT_TYPE_HTML;

    /// Response body.
    std::string body;
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
    /// Sets the document root for serving static files.
    void set_document_root(const std::filesystem::path &new_document_root);

    /// Main function for handling an HTTP request.
    [[nodiscard]] HttpResponse handle_http(const std::string &resource) const;
};
