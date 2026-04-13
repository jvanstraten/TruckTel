#include "server.h"

#include <asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "logger.h"

using WebsocketServer = websocketpp::server<websocketpp::config::asio>;

// TODO:
//  - implement a logging policy that connects to Logger
//  - add http handler for testing
//  - make sure this doesn't catch fire on windows
//  - implement basically everything from the telemetry_server example
//  - find a way to terminate the event loop gracefully
//  - ???

// Define a callback to handle incoming messages
void on_message(
    WebsocketServer *s,
    websocketpp::connection_hdl hdl,
    WebsocketServer::message_ptr msg
) {
    std::cout << "on_message called with hdl: " << hdl.lock().get()
              << " and message: " << msg->get_payload() << std::endl;

    // check for a special command to instruct the server to stop listening so
    // it can be cleanly exited.
    if (msg->get_payload() == "stop-listening") {
        s->stop_listening();
        return;
    }

    try {
        s->send(hdl, msg->get_payload(), msg->get_opcode());
    } catch (websocketpp::exception const &e) {
        std::cout << "Echo failed because: "
                  << "(" << e.what() << ")" << std::endl;
    }
}

void Server::main() {
    /*int i = 0;
    while (!shutdown_requested.load()) {
        Logger::info("hello from thread x%d!", ++i);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }*/

    // Create a server endpoint
    WebsocketServer echo_server;

    try {
        // Set logging settings
        echo_server.set_access_channels(websocketpp::log::alevel::all);
        // echo_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize Asio
        echo_server.init_asio();

        // Register our message handler
        using websocketpp::lib::bind;
        using websocketpp::lib::placeholders::_1;
        using websocketpp::lib::placeholders::_2;
        echo_server.set_message_handler(
            bind(&on_message, &echo_server, _1, _2)
        );

        // Listen on port 9002
        echo_server.listen(8080);

        // Start the server accept loop
        echo_server.start_accept();

        // Start the ASIO io_service run loop
        while (!shutdown_requested.load()) {
            echo_server.poll();
        }
        echo_server.stop();
        echo_server.run();
    } catch (websocketpp::exception const &e) {
        std::cout << e.what() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
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
