#pragma once

#include <filesystem>
#include <set>

#include <nlohmann/json.hpp>

#include "http.h"
#include "url.h"
#include "wspp_config.h"

/// WebsocketPP server, initially derived from the telemetry_server example
/// class.
class Server {
private:
    wspp::Server endpoint;
    std::set<wspp::connection_hdl, std::owner_less<wspp::connection_hdl>>
        connections;
    wspp::Server::timer_ptr timer;

    /// Document root for serving static files.
    HttpHandler http_handler;

    // Telemetry data
    uint64_t count = 0;

    /// Restarts the timer for periodic calls.
    void set_timer();

    /// Called periodically in Asio context based on set_timer().
    void on_timer(wspp::lib::error_code const &ec);

    /// Called by WebsocketPP when a request is made that the client does not
    /// mean to upgrade to a websocket connection.
    void on_http(const wspp::connection_hdl &hdl);

    /// Called by WebsocketPP when a request is made that the client wants to
    /// upgrade to a websocket connection.
    void on_open(const wspp::connection_hdl &hdl);

    /// Called by WebsocketPP when a websocket connection is closed.
    void on_close(const wspp::connection_hdl &hdl);

    /// Called from Asio context when the server is to be shut down.
    void on_shutdown();

public:
    /// Starts the server. Call from a worker thread; this will not return until
    /// the server has shut down.
    void run(const std::filesystem::path &document_root, uint16_t port);

    /// Call from any thread to tell the server to stop accepting connections
    /// and close all open connections.
    void shutdown();
};
