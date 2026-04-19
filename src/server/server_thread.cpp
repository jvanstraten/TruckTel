#include "server_thread.h"

#include "logger.h"
#include "server.h"

void ServerThread::main() const {
    try {
        server->run(configuration);
    } catch (std::exception &e) {
        Logger::error("fatal error in server: %s", e.what());
    }
}

ServerThread::ServerThread(const std::filesystem::path &app_path)
    : configuration(load_app_config(app_path)) {}

uint16_t ServerThread::port() const {
    return configuration.port;
}

const InputChannelDescriptors &ServerThread::get_input_descriptors() const {
    return configuration.input_channel_descriptors;
}

void ServerThread::start() {
    server = std::make_unique<Server>();
    thread = std::thread(&ServerThread::main, this);
}

void ServerThread::update() {
    if (!server) return;
    server->update();
}

void ServerThread::stop() {
    if (!server) return;
    server->stop();
}

void ServerThread::join() {
    if (!server) return;
    thread.join();
    server.reset();
}

ServerThread::~ServerThread() {
    if (server) server->stop();
    if (thread.joinable()) thread.join();
}

/*ServerThread::ServerThread(const Configuration &config)
    : server(std::make_unique<Server>()),
      thread(&ServerThread::main, this, config) {}

ServerThread::~ServerThread() {
    server->shutdown();
    thread.join();
}

std::unique_ptr<ServerThread> ServerThread::instance;

void ServerThread::init(const Configuration &config) {
    if (instance)
        throw std::runtime_error("can only have one telemetry server at once");
    instance.reset(new ServerThread(config));
    Logger::info("Note: log messages from the server thread may not show");
    Logger::info("up in the game log immediately while in the main menu");
    Logger::info("and not focused. Track the plugin log file if necessary!");
    Recorder::set_update_server_callback([]() {
        if (instance) instance->server->update();
    });
}

void ServerThread::shutdown() {
    if (!instance) return;
    Logger::info("Shutting down server...");
    instance.reset();
    Logger::info("Server has shut down");
}*/