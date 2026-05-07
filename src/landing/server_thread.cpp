#include "server_thread.h"

#include "server.h"

LandingServerThread::LandingServerThread(uint16_t port, const LandingInfo &info)
    : port(port), info(info) {}

void LandingServerThread::start() {
    thread.start(std::make_unique<LandingServer>(port, info));
}

void LandingServerThread::stop() {
    if (const auto worker = thread.get_worker()) {
        worker->stop();
    }
}

void LandingServerThread::join() {
    thread.join();
}

LandingServerThread::~LandingServerThread() {
    stop();
}
