#pragma once

// If it wasn't obvious, this is a generated file! See the "landing" directory.
// Do not modify manually.

namespace landing {

/// We need to split the data for large resources into multiple parts, because
/// MSVC imposes an unreasonably short maximum string literal limit of 16k.
/// Come on guys, the past century called, they want their static allocations
/// back.
struct Part {
    /// Length of this data part, in case there are embedded nulls in the
    /// string.
    unsigned int length;

    /// Resource data.
    const char *data;
};

/// Data for a landing page resource to be served from memory.
struct Resource {
    /// Path for which this resource should be served, or nullptr for the
    /// fallback page. The fallback page will always be the last entry in the
    /// array.
    const char *path;

    /// Content type to serve the resource with.
    const char *content_type;

    /// Array of content parts that form the data.
    Part *parts;
};

static const Part parts[] = {{10, "<html><bod"}, {10, "y>Placehol"},
                             {10, "der for la"}, {10, "nding page"},
                             {10, "!</body></"}, {5, "html>"},
                             {0, nullptr}};

static const Resource resources[] = {
    {nullptr, "text/html; charset=utf-8", parts + 0}
};

} // namespace landing
