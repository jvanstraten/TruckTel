#pragma once

#include <string>

/// Tries to tell the operating system to "open" the given thing. Should work
/// at least for opening URLs in web browsers.
void os_open(const std::string &url);
