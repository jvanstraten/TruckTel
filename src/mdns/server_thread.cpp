#include "server_thread.h"

#include "logger.h"
#include "server.h"

MdnsServerThread::MdnsServerThread(MdnsConfiguration config)
    : config(std::move(config)) {}

void MdnsServerThread::start() {
    mdns.start(std::make_unique<MdnsServer>(config));
}

std::string MdnsServerThread::get_local_ip_address() const {
    if (auto worker = mdns.get_worker()) {
        return worker->get_local_ip_address();
    }
    return "";
}

std::string MdnsServerThread::get_hostname() const {
    if (auto worker = mdns.get_worker()) {
        return worker->get_hostname();
    }
    return "";
}

void MdnsServerThread::stop() {
    if (const auto worker = mdns.get_worker()) {
        worker->stop();
    }
}

void MdnsServerThread::join() {
    mdns.join();
}

MdnsServerThread::~MdnsServerThread() {
    stop();
}
