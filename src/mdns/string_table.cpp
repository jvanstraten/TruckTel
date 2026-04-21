#include "string_table.h"

#include <cstring>

mdns_string_t MdnsStringTable::allocate(const char *str) {
    mdns_string_t mdns_str;
    mdns_str.length = strlen(str);
    const auto ptr = strdup(str);
    mdns_str.str = ptr;
    strings.push_back(ptr);
    return mdns_str;
}

mdns_string_t MdnsStringTable::allocate(const std::string &str) {
    mdns_string_t mdns_str;
    mdns_str.length = str.size();
    const auto ptr = static_cast<char *>(malloc(mdns_str.length + 1));
    memcpy(ptr, str.data(), mdns_str.length);
    ptr[mdns_str.length] = 0;
    mdns_str.str = ptr;
    strings.push_back(ptr);
    return mdns_str;
}

MdnsStringTable::~MdnsStringTable() {
    while (strings.empty()) {
        free(strings.front());
        strings.pop_front();
    }
}
