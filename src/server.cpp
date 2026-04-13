#include "server.h"

#include "logger.h"

void Server::main() {
    server.set_logger([](const httplib::Request &req,
                         const httplib::Response &rep) {
        Logger::info(
            "httplib: serving %s for %s:%d -> HTTP %d %s", req.path.c_str(),
            req.remote_addr.c_str(), req.remote_port, rep.status,
            rep.reason.c_str()
        );
    });
    server.set_error_logger([](const httplib::Error &err,
                               const httplib::Request *req) {
        if (!req) {
            Logger::error("httplib error %s", httplib::to_string(err).c_str());
        } else {
            Logger::error(
                "httplib error %s for %s from %s:%d",
                httplib::to_string(err).c_str(), req->path.c_str(),
                req->remote_addr.c_str(), req->remote_port
            );
        }
    });
    server.WebSocket(
        "/ws", [](const httplib::Request &req, httplib::ws::WebSocket &ws) {
            std::string msg;
            while (ws.read(msg)) {
                Logger::info("echo %s", msg.c_str());
                ws.send(msg); // Send back the received message as-is
            }
        }
    );
    server.listen("0.0.0.0", 8080);
}

Server::Server() : thread(&Server::main, this) {}

Server::~Server() {
    shutdown_request.store(true);
    server.stop();
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
