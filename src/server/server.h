#pragma once

#include <chrono>
#include <filesystem>
#include <map>

#include "config.h"
#include "database.h"
#include "http.h"
#include "websocket.h"
#include "wspp_config.h"

/// Exception class used to return 404 errors.
struct FileNotFound : std::runtime_error {
    explicit FileNotFound(const std::string &path) : std::runtime_error(path) {}
};

/// WebsocketPP server, initially derived from the telemetry_server example
/// class.
class Server {
private:
    /// WebsocketPP server endpoint.
    wspp::Server endpoint;

    /// Open websocket connections.
    std::map<
        wspp::connection_hdl,
        WebSocket,
        std::owner_less<wspp::connection_hdl>>
        connections;

    /// Amount of milliseconds of silence from the game after which the server
    /// thread will poll on its own.
    static constexpr int MIN_UPDATE_PERIOD_MILLIS = 1000;

    /// Watchdog timer used to poll the game thread periodically even when the
    /// game is not generating frames. This is the case in the main menu at
    /// least. Used to keep reporting events and configuration updates in this
    /// case.
    wspp::Server::timer_ptr timer;

    /// "Database" that stores the most recent state of all configuration and
    /// channel data from the game synchronized to the Asio thread.
    Database database;

    /// The last time that the database was updated.
    std::chrono::system_clock::time_point last_update;

    /// Document root for serving static files.
    HttpHandler http_handler;

    // Telemetry data
    uint64_t count = 0;

    /// Restarts the timer for periodic calls.
    void set_timer();

    /// Called periodically to poll from the game thread data storage and
    /// maintain websocket connections.
    void on_update();

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
    void run(const ServerConfig &config);

    /// Call from any thread to tell the server to stop accepting connections
    /// and close all open connections.
    void shutdown();

    /// Hint from the game thread that new data is available and the server
    /// thread should poll again soon.
    void update();
};
