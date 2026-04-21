#include "server_thread.h"

#include "logger.h"
#include "mdns.h"
#include "server.h"

void MdnsServerThread::main() {
    try {
        server->init(configuration);
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

MdnsServerThread::MdnsServerThread(const MdnsConfiguration &configuration)
    : configuration(configuration) {}

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
