#include "server.h"

void Server::set_timer() {
    timer = endpoint.set_timer(
        MIN_UPDATE_PERIOD_MILLIS,
        [this](wspp::lib::error_code const &ec) { on_timer(ec); }
    );
}

void Server::on_update() {
    last_update = std::chrono::system_clock::now();
    database.update();
    for (auto &[_, websocket] : connections) {
        websocket.update();
    }
}

void Server::on_timer(wspp::lib::error_code const &ec) {
    if (ec) return;

    // Update websocket connections if we haven't recently received a frame from
    // the game.
    if (std::chrono::system_clock::now() - last_update >
        std::chrono::milliseconds(MIN_UPDATE_PERIOD_MILLIS)) {
        update();
    }

    // Reset the timer.
    set_timer();
}

void Server::on_http(const wspp::connection_hdl &hdl) {
    // Upgrade our connection handle to a full connection_ptr.
    const wspp::Server::connection_ptr con = endpoint.get_con_from_hdl(hdl);

    // Get the response for the requested resource.
    const auto response = http_handler.handle_http(con->get_resource());

    // Return the response to WebsocketPP.
    con->set_body(response.body);
    con->append_header("content-type", response.content_type);
    con->set_status(response.code);
}

void Server::on_open(const wspp::connection_hdl &hdl) {
    if (auto result = WebSocket::handle_request(endpoint, hdl, database)) {
        connections.insert(std::make_pair(hdl, *result));
    }
}

void Server::on_close(const wspp::connection_hdl &hdl) {
    connections.erase(hdl);
}

void Server::on_shutdown() {
    Logger::info("on_shutdown() start");
    endpoint.stop_listening();
    for (auto &[_, websocket] : connections) {
        Logger::info("shutting down socket");
        websocket.shutdown();
    }
    Logger::info("cancel timer");
    if (timer) {
        timer->cancel();
    }
    Logger::info("on_shutdown() complete");
}

void Server::run(const ServerConfig &config) {
    // Configure the HTTP handler.
    http_handler.configure(
        config.document_root, config.content_types, database
    );

    // Set up masks for logging.
    endpoint.clear_access_channels(wspp::alevel::all);
    endpoint.set_access_channels(wspp::alevel::access_core);
    endpoint.set_access_channels(wspp::alevel::app);

    // Initialize the Asio transport policy.
    endpoint.init_asio();

    // Bind the handlers we are using.
    endpoint.set_open_handler([this](const wspp::connection_hdl &hdl) {
        on_open(hdl);
    });
    endpoint.set_close_handler([this](const wspp::connection_hdl &hdl) {
        on_close(hdl);
    });
    endpoint.set_http_handler([this](const wspp::connection_hdl &hdl) {
        on_http(hdl);
    });

    // Enable SO_REUSE_ADDRESS so the user restarting the game quickly doesn't
    // cause the plugin to stop working. This is technically a security risk,
    // see e.g.
    // https://github.com/zaphoyd/websocketpp/issues/803#issuecomment-912953104.
    // However, if the user is expecting a webserver in a game mod plugin to be
    // a properly secured thing, they have bigger problems.
    endpoint.set_reuse_addr(true);

    // Start listening.
    endpoint.listen(config.port);
    endpoint.start_accept();

    // Set the initial timer to start telemetry
    set_timer();

    // Start the ASIO io_service run loop
    endpoint.run();
}

void Server::shutdown() {
    asio::post(endpoint.get_io_service(), [this]() { on_shutdown(); });
}

void Server::update() {
    asio::post(endpoint.get_io_service(), [this]() { on_update(); });
}