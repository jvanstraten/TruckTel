#include "server.h"

#include <sstream>

#include "info.h"
#include "landing_data.h"
#include "os_utils.h"

void LandingServer::on_http(const wspp::connection_hdl &hdl) {
    // Upgrade our connection handle to a full connection_ptr.
    const wspp::Server::connection_ptr con = endpoint.get_con_from_hdl(hdl);

    // Get resource path.
    auto path = con->get_resource();

    // Normalize the resource path.
    if (path.empty() || path.at(0) != '/') {
        path = '/' + path;
    }
    if (path.at(path.size() - 1) == '/') {
        path += LANDING_INDEX;
    }

    // If the path maps to info.json, serve that.
    if (path == LANDING_API_PATH) {
        con->set_body(info);
        con->append_header("content-type", LANDING_API_CONTENT_TYPE);
        con->set_status(wspp::http::status_code::value::ok);
        return;
    }

    // Find the appropriate resource.
    const landing::Resource *resource = landing::resources;
    while (resource->path && strcmp(resource->path, path.c_str())) {
        resource++;
    }

    // Build the content.
    std::ostringstream ss{};
    auto part = resource->parts;
    while (part->length && part->data) {
        ss << std::string(part->data, part->length);
        part++;
    }

    // Return the response to WebsocketPP.
    con->set_body(ss.str());
    con->append_header("content-type", resource->content_type);
    con->set_status(wspp::http::status_code::value::ok);
}

void LandingServer::on_open(const wspp::connection_hdl &hdl) {
    const wspp::Server::connection_ptr con = endpoint.get_con_from_hdl(hdl);
    con->close(wspp::close::status::normal, "invalid endpoint");
}

void LandingServer::on_shutdown() {
    endpoint.stop_listening();
}

LandingServer::LandingServer(const uint16_t port, const LandingInfo &info)
    : port(port), info(serialize_landing_info(info).dump()) {}

void LandingServer::init() {
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
    endpoint.listen(port);
    endpoint.start_accept();

    // (Try to) open the landing page.
    open_browser("http://localhost:" + std::to_string(port));
}

void LandingServer::run() {
    endpoint.run();
}

void LandingServer::stop() {
    asio::post(endpoint.get_io_service(), [this]() { on_shutdown(); });
}
