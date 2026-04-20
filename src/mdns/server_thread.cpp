#include "server_thread.h"

#include <mdns.h>

#include "logger.h"
#include "server.h"

void MdnsServerThread::main() {
    try {
        server->init(services);
        {
            std::unique_lock lock(state_mutex);
            init_success.store(true);
        }
        state_cv.notify_all();
        server->run();
    } catch (std::exception &e) {
        Logger::error("fatal error in mDNS server: %s", e.what());
        {
            std::unique_lock lock(state_mutex);
            server_crashed.store(true);
        }
        state_cv.notify_all();
    }
}

MdnsServerThread::MdnsServerThread(std::map<uint16_t, std::string> services)
    : services(std::move(services)) {}

void MdnsServerThread::start() {
    server = std::make_unique<MdnsServer>();
    thread = std::thread(&MdnsServerThread::main, this);
    std::unique_lock lk(state_mutex);
    state_cv.wait(lk, [this] {
        return init_success.load() || server_crashed.load();
    });
}

void MdnsServerThread::stop() {
    if (!server) return;
    if (server_crashed.load()) {
        thread.join();
        server.reset();
        return;
    }
    server->stop();
}

void MdnsServerThread::join() {
    if (!server) return;
    thread.join();
    server.reset();
}

MdnsServerThread::~MdnsServerThread() {
    if (server) server->stop();
    if (thread.joinable()) thread.join();
}
