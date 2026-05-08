#include "server_thread.h"

#include "server.h"

LandingServerThread::LandingServerThread(
    const LandingConfiguration &config, const LandingInfo &info
)
    : config(config), info(info) {}

void LandingServerThread::start() {
    thread.start(std::make_unique<LandingServer>(config, info));
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
