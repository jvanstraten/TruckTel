#pragma once

#include <chrono>
#include <optional>

#include "database.h"
#include "url.h"
#include "wspp_config.h"

/// Class managing a single websocket connection.
class WebSocket {

    /// Connection handle.
    const wspp::Server::connection_ptr con;

    /// Requested websocket data type.
    const enum struct DataType {
        /// (Gameplay) event stream in structured form.
        EVENT_STRUCT,

        /// (Gameplay) event stream in flattened form.
        EVENT_FLAT,

        /// Normal database output sent repeatedly.
        DATA,

        /// Delta-encoded database output. Only items of JSON objects that
        /// changed since the previous message are sent. Item removal is
        /// signaled with a null replacement. If nothing changes at all (above
        /// would yield an empty object), no message is sent at all.
        DELTA,

        /// Input-only websocket. Instead of sending data, the server will send
        /// acknowledgements of input actions.
        INPUT,
    } data_type;

    /// Database handle.
    const Database &database;

    /// The query command for getting data from the database. Not used for
    /// event websockets.
    const std::vector<std::string> database_query;

    /// Throttle value. Updates are suppressed until at least this many
    /// milliseconds have passed since the previous completed update.
    const unsigned long throttle;

    /// The last time we sent a message.
    std::chrono::system_clock::time_point last_message;

    /// For event websockets, the ID of the next event that is to be delivered
    /// to the client.
    uint64_t next_event_id = 0;

    /// Previous data for constructing delta-coded streams.
    nlohmann::json previous_data;

    /// Sends a JSON object to the client.
    void send(const nlohmann::json &data);

    /// Handles an incoming websocket message.
    static nlohmann::json receive(const std::string &message);

    /// Internal update handler. Does not catch exceptions.
    void update_internal(bool first);

    /// Constructor.
    WebSocket(
        wspp::Server::connection_ptr con,
        DataType data_type,
        const Database &database,
        std::vector<std::string> database_query,
        unsigned long throttle
    );

public:
    /// Main function for handling a websocket upgrade request. An instance is
    /// returned if the connection was accepted.
    [[nodiscard]] static std::optional<WebSocket> handle_request(
        const wspp::Server::connection_ptr &con, const Database &database
    );

    /// Call from Asio context to update the websocket connection.
    void update();

    /// Call from Asio context when a message is received on the websocket.
    void on_message(const std::string &message);

    /// Call from Asio context to close the websocket from the server side, in
    /// response to plugin unload.
    void shutdown();
};
