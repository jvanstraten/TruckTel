#include "server_thread.h"

#include "logger.h"
#include "recorder/recorder.h"
#include "server.h"

void ServerThread::main(std::string server_root) const {
    try {
        server->run(server_root, 8080);
    } catch (std::exception &e) {
        Logger::error("fatal error in server: %s", e.what());
    }
}

ServerThread::ServerThread(const std::string &server_root)
    : server(std::make_unique<Server>()),
      thread(&ServerThread::main, this, server_root) {}

ServerThread::~ServerThread() {
    server->shutdown();
    thread.join();
}

std::unique_ptr<ServerThread> ServerThread::instance;

void ServerThread::init(const std::string &server_root) {
    if (instance)
        throw std::runtime_error("can only have one telemetry server at once");
    instance.reset(new ServerThread(server_root));
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
