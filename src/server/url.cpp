#include "url.h"

#include <optional>
#include <sstream>

/// Returns whether the given character is printable ASCII. Not using the
/// standard library to avoid locale shenanigans; this doesn't need to be
/// complicated.
static constexpr bool is_print(const int c) {
    return c >= ' ' && c <= '~';
}

/// Returns whether the given character separates query elements.
static constexpr bool is_query_sep(const int c) {
    return c == '&' || c == ';';
}

/// Scans a single character from s and returns it if it's a printable
/// character. If it's not printable, an exception is thrown. The pointer is
/// advanced if and only if we're not at the end of the string; if we are, 0 is
/// returned.
static char scan(const char *&s) {
    if (const char c = *s) {
        s++;
        if (!is_print(c)) throw MalformedUrl("non-ASCII character in URL");
        return c;
    }
    return 0;
}

/// Decodes a hex digit to its numeric value. Throws if the character is not a
/// valid hex digit.
static int decode_hex(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw MalformedUrl("invalid escape sequence in URL");
}

/// Decodes an escape sequence. The leader % must already have been scanned.
/// Returns the decoded character only if it is printable ASCII, otherwise an
/// exception is thrown.
static char decode_escape(const char *&s) {
    // Scan the hex digits.
    const int h = decode_hex(scan(s));
    const int l = decode_hex(scan(s));

    // Form the character.
    const int dec_c = h << 4 | l;

    // Check if this character is printable.
    if (!is_print(dec_c))
        throw MalformedUrl("non-ASCII decoded character in URL");

    return static_cast<char>(dec_c);
}

Url::Url(const std::string &resource) {
    const char *s = resource.c_str();
    std::ostringstream ss{};

    // Check leading slash.
    if (scan(s) != '/') throw MalformedUrl("URL does not start with a slash");

    // Scan path elements.
    while (true) {
        char c = scan(s);

        // Handle characters that interrupt the current path element.
        if (!c || c == '/' || c == '?' || c == '#') {
            // Incoming character is no longer part of the current path element.
            // Get the completed path element.
            auto element = ss.str();
            ss.str("");

            // Empty path elements are appended only if more elements follow.
            if (!element.empty() || c == '/') {
                path_elements.emplace_back(std::move(element));
            }

            // Continue to the next path element if the element was interrupted
            // by a slash.
            if (c == '/') continue;

            // If we're interrupted by a ?, continue with parsing the query
            // string.
            if (c == '?') break;

            // Either we're at the end of the URL or we received a fragment for
            // some reason. We just ignore the latter, so in either case, we're
            // done.
            return;
        }

        // Handle escape sequences.
        if (c == '%') {
            c = decode_escape(s);
        }

        // Append the character to our buffer.
        ss << c;
    }

    // Scan query elements.
    std::optional<std::string> key;
    while (true) {
        char c = scan(s);

        // Handle equals signs to separate key from value.
        if (c == '=' && !key) {
            key = ss.str();
            ss.str("");
            continue;
        }

        // Handle characters that interrupt the current path element.
        if (!c || is_query_sep(c) || c == '#') {
            // Incoming character is no longer part of the current query
            // element.
            auto element = ss.str();
            ss.str("");

            // Determine key and value.
            std::string value{};
            if (!key) {
                key = std::move(element);
            } else {
                value = std::move(element);
            }

            // Empty query elements are appended only if more elements follow.
            if (!key->empty() || is_query_sep(c)) {
                query[*key] = std::move(value);
                key.reset();
                value.clear();
            }

            // Continue to the next query element if the element was interrupted
            // by a slash.
            if (is_query_sep(c)) continue;

            // Either we're at the end of the URL or we received a fragment for
            // some reason. We just ignore the latter, so in either case, we're
            // done.
            return;
        }

        // Handle escape sequences.
        if (c == '%') {
            c = decode_escape(s);
        }

        // Append the character to our buffer.
        ss << c;
    }
}

std::string Url::join_path() const {
    std::ostringstream ss{};
    for (const auto &element : path_elements) {
        ss << "/" << element;
    }
    return ss.str();
}

bool Url::match_path_element(
    const size_t idx, const std::string &expected
) const {
    return path_elements.size() > idx && path_elements[idx] == expected;
}

bool Url::is_api() const {
    return match_path_element(0, "api");
}

bool Url::is_ws() const {
    return match_path_element(1, "ws");
}

bool Url::is_rest() const {
    return match_path_element(1, "rest");
}

std::vector<std::string> Url::get_api_path_elements() const {
    return std::vector(path_elements.begin() + 2, path_elements.end());
}

/// Returns whether the given character is probably not okay in a filesystem
/// path element.
static bool is_illegal_path_char(const int c) {
    for (const char ill : "<>:\"/\\|?*") {
        if (c == ill) return true;
    }
    return false;
}

/// Returns whether the given path element is probably not okay to blindly use
/// in a filesystem path. Windows is most restrictive with which characters are
/// allowed. In addition, we flag empty elements and elements with only periods
/// in them as illegal, to prevent breaking out of the document root.
static bool is_illegal_path_element(const std::string &element) {
    bool only_periods = true;
    for (const char c : element) {
        if (is_illegal_path_char(c)) return true;
        if (c != '.') only_periods = false;
    }
    return only_periods;
}

std::filesystem::path Url::as_filesystem_path(
    std::filesystem::path path
) const {
    for (const auto &element : path_elements) {
        if (is_illegal_path_element(element))
            throw MalformedUrl("URL seems unsafe to use as a filesystem path");
        path /= element;
    }
    if (std::filesystem::is_directory(path)) {
        path /= "index.html";
    }
    return path;
}
