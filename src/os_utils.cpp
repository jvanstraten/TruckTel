#include "os_utils.h"

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#else
#include <cstring>
#include <unistd.h>
#endif

void open_browser(const std::string &url) {

#ifdef _WIN32
    ShellExecuteA(
        nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL
    );
#elif __linux__
    if (fork() == 0) {
        const char *open = "/usr/bin/xdg-open";
        static char *argv[] = {strdup(open), strdup(url.c_str()), nullptr};
        execve(open, argv, environ);
        free(argv[0]);
        free(argv[1]);
        _exit(1);
    }
#endif
}
