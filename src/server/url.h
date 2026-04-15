#pragma once

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

/// Exception class used to report malformed URLs.
struct MalformedUrl final : std::runtime_error {
    explicit MalformedUrl(const std::string &reason)
        : std::runtime_error(reason) {}
};

/// Class for decoding URLs, since websocketpp lacks this functionality.
/// What could possibly go wrong with making yet another URL parser?
struct Url {
    /// Decoded path elements, excluding slashes.
    std::vector<std::string> path_elements;

    /// Query elements. Both ampersand and semicolon separators are accepted.
    /// For both an elements without an equals sign and elements with an equals
    /// sign but no content, the value is an empty string.
    std::map<std::string, std::string> query;

    /// Constructs a URL from the resource string given by websocketpp. This
    /// starts with the path (no scheme or authority). A MalformedUrl exception
    /// is thrown when the URL is malformed or has non-printable characters in
    /// it (assumed to be malicious; note that only low ASCII is supported).
    explicit Url(const std::string &resource);

    /// Returns a string representation of the path elements.
    [[nodiscard]] std::string join_path() const;

    /// Returns whether the path element with the given index matches expected.
    [[nodiscard]] bool match_path_element(
        size_t idx, const std::string &expected
    ) const;

    /// Returns whether this is a non-static URL.
    [[nodiscard]] bool is_api() const;

    /// Returns whether this is a websocket URL. Valid only if is_api() returns
    /// true.
    [[nodiscard]] bool is_ws() const;

    /// Returns whether this is a REST URL. Valid only if is_api() returns true.
    [[nodiscard]] bool is_rest() const;

    /// Returns the path elements that define the API call.
    [[nodiscard]] std::vector<std::string> get_api_path_elements() const;

    /// Constructs a filesystem path from the path elements in the URL. This
    /// will throw a MalformedUrl exception if the path is likely unsafe. If
    /// the resulting path points to a directory, index.html is appended.
    [[nodiscard]] std::filesystem::path as_filesystem_path(
        std::filesystem::path path
    ) const;
};
