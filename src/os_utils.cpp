#include "os_utils.h"

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#else
#include <spawn.h>
#include <unistd.h>
#endif

void os_open(const std::string &url) {

#ifdef _WIN32
    ShellExecuteA(
        nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL
    );
#else
    for (const auto executable : {"xdg-open", "open"}) {
        pid_t pid;
        std::string executable_buf = executable;
        std::string url_buf = url;
        char *argv[3] = {executable_buf.data(), url_buf.data(), nullptr};
        if (!posix_spawnp(&pid, executable, nullptr, nullptr, argv, environ)) {
            break;
        }
    }
#endif
}
