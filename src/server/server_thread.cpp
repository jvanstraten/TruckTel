#include "server_thread.h"

#include "logger.h"
#include "server.h"

ServerThread::ServerThread(const std::filesystem::path &app_path)
    : configuration(load_app_config(app_path)) {}

uint16_t ServerThread::port() const {
    return configuration.port;
}

const Metadata &ServerThread::metadata() const {
    return configuration.metadata;
}

const InputChannelDescriptors &ServerThread::get_input_descriptors() const {
    return configuration.input_channel_descriptors;
}

void ServerThread::start() {
    thread.start(std::make_unique<Server>(configuration));
}

std::string ServerThread::get_error() {
    return thread.get_error();
}

void ServerThread::update() {
    if (const auto worker = thread.get_worker()) {
        worker->update();
    }
}

void ServerThread::stop() {
    if (const auto worker = thread.get_worker()) {
        worker->stop();
    }
}

void ServerThread::join() {
    thread.join();
}

ServerThread::~ServerThread() {
    stop();
}
