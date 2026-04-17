#include "http.h"

#include <regex>

#include "input.h"
#include "server.h"

HttpResponse HttpResponse::from_json(const nlohmann::json &json) {
    return {
        wspp::http::status_code::ok,
        CONTENT_TYPE_JSON,
        json.dump(),
    };
}

HttpResponse HttpHandler::handle_static(const Url &url) const {
    // If we don't have a document root for some reason, always return file not
    // found.
    if (document_root.empty()) throw FileNotFound(url.join_path());

    // Serve a static file from the document root.
    const auto path = url.as_filesystem_path(document_root);

    // Open the file.
    std::ifstream file;
    file.open(path.string().c_str(), std::ios::in);
    if (!file) throw FileNotFound(url.join_path());

    // Read the file to a string.
    HttpResponse response{};
    file.seekg(0, std::ios::end);
    response.body.reserve(file.tellg());
    file.seekg(0, std::ios::beg);
    response.body.assign(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
    );

    // Determine the content type.
    auto filename = path.filename().string();
    response.content_type = CONTENT_TYPE_UNKNOWN;
    for (const auto &[regex, content_type] : content_types) {
        if (std::regex_match(filename, regex)) {
            response.content_type = content_type;
            break;
        }
    }

    return response;
}

HttpResponse HttpHandler::handle_rest(const Url &url) const {
    const auto query = url.get_api_path_elements();

    // Handle input endpoints.
    auto json = Input::run_query(query);
    if (!json.is_null()) return HttpResponse::from_json(json);

    // Handle database endpoints.
    if (!database) throw std::runtime_error("missing database");
    json = database->get_data(query);
    return HttpResponse::from_json(json);
}

HttpResponse HttpHandler::handle_websocket(const Url &url) const {
    std::stringstream ss;
    ss << "The requested URL " << url.join_path() << " is a websocket API. ";
    ss << "Protocol upgrade to websocket is required.";
    return handle_error(
        wspp::http::status_code::upgrade_required, url.join_path(), ss.str()
    );
}

HttpResponse HttpHandler::handle_error(
    const wspp::http::status_code::value code,
    const std::string &raw_message,
    const std::string &pretty_message
) const {
    try {
        auto response =
            handle_static(Url("/error_" + std::to_string(code) + ".html"));
        response.code = code;
        response.body = std::regex_replace(
            response.body, std::regex("%%MESSAGE%%"), raw_message
        );
        return response;
    } catch (std::exception &e) {
        std::ostringstream ss{};
        ss << "<!doctype html><html><head>"
           << "<title>Error " << code << " ("
           << wspp::http::status_code::get_string(code) << ")</title><body>"
           << "<h1>Error " << code << "</h1>"
           << "<p>" << pretty_message << "</p>"
           << "</body></head></html>";
        return {code, CONTENT_TYPE_HTML, ss.str()};
    }
}

void HttpHandler::configure(
    const std::filesystem::path &new_document_root,
    const std::vector<std::pair<std::regex, std::string>> &new_content_types,
    Database &new_database
) {
    document_root = new_document_root;
    content_types = new_content_types;
    database = &new_database;
}

HttpResponse HttpHandler::handle_http(const std::string &resource) const {
    try {
        Url url(resource);
        if (!url.is_api()) return handle_static(url);
        if (url.is_ws()) return handle_websocket(url);
        if (url.is_rest()) return handle_rest(url);
        throw FileNotFound(url.join_path());
    } catch (MalformedUrl &e) {
        std::stringstream ss;
        ss << "Bad request: " << e.what() << ".";
        return handle_error(
            wspp::http::status_code::bad_request, e.what(), ss.str()
        );
    } catch (FileNotFound &e) {
        std::stringstream ss;
        ss << "The requested URL " << e.what()
           << " was not found on this server.";
        return handle_error(
            wspp::http::status_code::not_found, e.what(), ss.str()
        );
    } catch (std::exception &e) {
        std::stringstream ss;
        ss << "Internal server error: " << e.what() << ".";
        return handle_error(
            wspp::http::status_code::internal_server_error, e.what(), ss.str()
        );
    }
}