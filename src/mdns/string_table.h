#pragma once

#include <list>
#include <string>

#include "mdns.h"

/// Class managing storage for mdns_string_t items.
class MdnsStringTable {
    /// All strings currently stored.
    std::list<void *> strings;

public:
    /// Allocates an mDNS string for the given C string.
    mdns_string_t allocate(const char *str);

    /// Allocates an mDNS string for the given C++ string. Supports embedded
    /// null characters.
    mdns_string_t allocate(const std::string &str);

    /// Destructor.
    ~MdnsStringTable();
};
