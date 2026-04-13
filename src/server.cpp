#include "server.h"

#include "logger.h"

void Server::main() {
    int i = 0;
    while (!shutdown_requested.load()) {
        Logger::info("hello from thread x%d!", ++i);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

Server::Server() : thread(&Server::main, this) {}

Server::~Server() {
    shutdown_requested.store(true);
    thread.join();
}

std::unique_ptr<Server> Server::instance;

void Server::init() {
    if (instance)
        throw std::runtime_error("can only have one telemetry server at once");
    instance.reset(new Server());
    Logger::info("Note: log messages from the server thread may not");
    Logger::info("show up in the game log immediately while in the");
    Logger::info("main menu. Track the plugin log file if necessary!");
}

void Server::shutdown() {
    if (!instance) return;
    instance.reset();
}
