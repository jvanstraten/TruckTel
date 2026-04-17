#include "websocket.h"

#include "api.h"
#include "input.h"
#include "json_utils.h"
#include "recorder/recorder.h"
#include "server.h"

void WebSocket::send(const nlohmann::json &data) {
    if (con->send(data.dump())) {
        Logger::error("failed to send websocket message");
    }
    last_message = std::chrono::system_clock::now();
}

/// Handles an incoming websocket message.
nlohmann::json WebSocket::receive(const std::string &message) {
    try {
        const auto json = nlohmann::json::parse(message);

        // Incoming messages are interpreted as input commands regardless of
        // websocket endpoint type. These must be query arrays for the input
        // subsystem.
        if (!json.is_array()) return "array expected";

        // Flatten the JSON array into strings for the query. This also means
        // that we're converting value numbers to a string only to then parse
        // them as a float again... yuck.
        std::vector<std::string> query;
        query.emplace_back(API_INPUT_QUERY);
        for (const auto &json_element : json) {
            if (json_element.is_string()) {
                query.emplace_back(json_element.get<std::string>());
            } else {
                query.emplace_back(json_element.dump());
            }
        }
        return Input::run_query(query);

    } catch (const std::exception &e) {
        return e.what();
    }
}

void WebSocket::update_internal(const bool first) {

    // Input websockets don't need to be updated, because they only have to
    // send messages in response to received messages.
    if (data_type == DataType::INPUT) {
        return;
    }

    // Handle throttling.
    auto now = std::chrono::system_clock::now();
    if (!first && now - last_message < std::chrono::milliseconds(throttle)) {
        return;
    }

    // Handle event sockets.
    if (data_type == DataType::EVENT_STRUCT ||
        data_type == DataType::EVENT_FLAT) {

        // Initialize event polling if this is the first update.
        if (first) {
            next_event_id = Recorder::event_poll_init();
        }

        // Poll for events since the last poll.
        const auto raw_data = Recorder::event_poll(next_event_id);
        if (raw_data.empty()) return;

        // Convert the event data to JSON and send it.
        const bool flatten = data_type == DataType::EVENT_FLAT;
        for (const auto &event : raw_data) {
            send(named_values_to_json(event, flatten));
        }
        return;
    }

    // Get data from the database.
    const auto new_data = database.get_data(database_query);

    // The data from the database is already in the right form for
    // non-delta-coded sockets.
    if (data_type == DataType::DATA) {
        send(new_data);
        return;
    }

    // Perform delta encoding.
    const auto delta = json_delta_encode(new_data, previous_data);
    previous_data = new_data;

    // Send the delta-coded data if it's nontrivial.
    if (!delta.empty()) send(delta);
}

WebSocket::WebSocket(
    wspp::Server::connection_ptr con,
    const DataType data_type,
    const Database &database,
    std::vector<std::string> database_query,
    const unsigned long throttle
)
    : con(std::move(con)), data_type(data_type),
      database_query(std::move(database_query)), database(database),
      throttle(throttle) {
    update_internal(true);
}

std::optional<WebSocket> WebSocket::handle_request(
    const wspp::Server::connection_ptr &con, const Database &database
) {
    try {
        // Check that this is a websocket API URL.
        Url url(con->get_resource());
        if (!url.is_api() || !url.is_ws()) {
            con->close(
                wspp::close::status::normal,
                "invalid endpoint " + url.join_path()
            );
            return {};
        }

        // Parse API command.
        auto api_command = url.get_api_path_elements();
        unsigned long throttle = 1000;

        // The first element selects the data source, the remainder is a
        // database query (if the data source requires it).
        if (api_command.empty()) {
            con->close(
                wspp::close::status::normal,
                "missing data type in " + url.join_path()
            );
            return {};
        }
        const auto data_source = api_command.front();
        api_command.erase(api_command.begin());

        // Parse data type.
        DataType data_type;
        if (data_source == API_WS_EVENT) {

            // The event data type has one extra command that specifies the
            // data structuring.
            if (api_command.empty()) {
                con->close(
                    wspp::close::status::normal,
                    "missing structure in " + url.join_path()
                );
                return {};
            }
            const std::string &structure = api_command[0];
            if (structure == API_STRUCTURE_STRUCT) {
                data_type = DataType::EVENT_STRUCT;
            } else if (structure == API_STRUCTURE_FLAT) {
                data_type = DataType::EVENT_FLAT;
            } else {
                con->close(
                    wspp::close::status::normal,
                    "unsupported structure in " + url.join_path()
                );
                return {};
            }

            // Event streams are naturally slow, so don't have throttling
            // enabled by default.
            throttle = 0;

            // No database query expected for event streams.
            if (api_command.size() > 1) {
                con->close(
                    wspp::close::status::normal,
                    "unexpected arguments in " + url.join_path()
                );
                return {};
            }

        } else if (data_source == API_WS_DATA) {
            data_type = DataType::DATA;
        } else if (data_source == API_WS_DELTA) {
            data_type = DataType::DELTA;
        } else if (data_source == API_WS_INPUT) {
            data_type = DataType::INPUT;
            if (!api_command.empty()) {
                con->close(
                    wspp::close::status::normal,
                    "unexpected arguments in " + url.join_path()
                );
                return {};
            }
        } else {
            con->close(
                wspp::close::status::normal,
                "unsupported data type in " + url.join_path()
            );
            return {};
        }

        // Parse update period/throttle command.
        const auto it = url.query.find(API_WS_THROTTLE);
        if (it != url.query.end()) {
            try {
                throttle = std::stoul(it->second);
            } catch (const std::exception &e) {
                con->close(
                    wspp::close::status::normal,
                    std::string("invalid throttle command: ") + e.what()
                );
                return {};
            }
        }

        return WebSocket(con, data_type, database, api_command, throttle);
    } catch (MalformedUrl &e) {
        con->close(wspp::close::status::normal, e.what());
        return {};
    } catch (std::exception &e) {
        con->close(wspp::close::status::internal_endpoint_error, e.what());
        return {};
    }
}

void WebSocket::update() {
    try {
        update_internal(false);
    } catch (std::exception &e) {
        con->close(wspp::close::status::internal_endpoint_error, e.what());
    }
}

void WebSocket::on_message(const std::string &message) {
    const auto response = receive(message);

    // Only websockets of type INPUT actually send responses.
    if (data_type == DataType::INPUT) {
        send(response);
    }
}

void WebSocket::shutdown() {
    con->close(wspp::close::status::going_away, "TruckTel plugin unload");
}
