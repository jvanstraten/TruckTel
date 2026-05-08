#pragma once

#include <nlohmann/json.hpp>

#include "config.h"
#include "info.h"
#include "server/wspp_config.h"
#include "worker.h"

/// WebsocketPP server for the landing page.
class LandingServer final : public AbstractWorker {
private:
    /// WebsocketPP server endpoint.
    wspp::Server endpoint;

    /// Configuration object for the landing page server.
    const LandingConfiguration config;

    /// Stringified information object served to the landing page.
    const std::string info;

    /// Called by WebsocketPP when a request is made that the client does not
    /// mean to upgrade to a websocket connection.
    void on_http(const wspp::connection_hdl &hdl);

    /// Called by WebsocketPP when a request is made that the client wants to
    /// upgrade to a websocket connection.
    void on_open(const wspp::connection_hdl &hdl);

    /// Called from Asio context when the server is to be shut down.
    void on_shutdown();

public:
    /// Constructor.
    LandingServer(const LandingConfiguration &config, const LandingInfo &info);

    /// Initializes the server. Call from a worker thread.
    void init() override;

    /// Starts the server. Call from the same worker thread that called init().
    /// This will not return until the server is shut down.
    void run() override;

    /// Call from any thread to tell the server to stop accepting connections
    /// and close all open connections.
    void stop();
};
