#include "server_thread.h"

#include "logger.h"
#include "recorder/recorder.h"
#include "server.h"

void ServerThread::main(ServerConfig config) const {
    try {
        server->run(config);
    } catch (std::exception &e) {
        Logger::error("fatal error in server: %s", e.what());
    }
}

ServerThread::ServerThread(const ServerConfig &config)
    : server(std::make_unique<Server>()),
      thread(&ServerThread::main, this, config) {}

ServerThread::~ServerThread() {
    server->shutdown();
    thread.join();
}

std::unique_ptr<ServerThread> ServerThread::instance;

void ServerThread::init(const ServerConfig &config) {
    if (instance)
        throw std::runtime_error("can only have one telemetry server at once");
    instance.reset(new ServerThread(config));
    Logger::info("Note: log messages from the server thread may not");
    Logger::info("show up in the game log immediately while in the");
    Logger::info("main menu. Track the plugin log file if necessary!");
    Recorder::set_update_server_callback([]() {
        if (instance) instance->server->update();
    });
}

void ServerThread::shutdown() {
    if (!instance) return;
    instance.reset();
}
