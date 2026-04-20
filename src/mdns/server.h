#pragma once

#include <array>
#include <atomic>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <mdns.h>

/// Class managing the mDNS server in its worker thread.
class MdnsServer {
private:
    /// Number of bytes in an IPv6 address.
    static constexpr auto IPV6_LEN = 16;

    /// IPv6 localhost address.
    static constexpr unsigned char IPV6_LOCALHOST[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                       0, 0, 0, 0, 0, 0, 0, 1};

    /// IPv6 mapped localhost address.
    static constexpr unsigned char IPV6_LOCALHOST_MAPPED[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1
    };

    /// Service map from port number to app name.
    std::map<uint16_t, std::string> services;

    /// Local IPv4 address. If there are multiple, the first enumerated address
    /// is assumed to be the primary one.
    std::optional<std::pair<uint32_t, std::string>> local_ipv4;

    /// Local IPv6 address. If there are multiple, the first enumerated address
    /// is assumed to be the primary one.
    std::optional<std::pair<std::array<uint8_t, IPV6_LEN>, std::string>>
        local_ipv6;

    /// Finds out what our (primary) IP and IPv6 address is.
    void find_local_ips();

    /// Open socket descriptors.
    std::vector<int> sockets;

    /// Opens the sockets we use to listen for queries.
    void open_sockets();

    /// Closes open sockets.
    void close_sockets();

    /// Used to signal shutdown.
    std::atomic<bool> shutdown_requested = false;

    /// Called by mdns.h with all the arguments in the world when a service
    /// request is made.
    static int service_callback(
        int sock,
        const struct sockaddr *from,
        size_t addrlen,
        mdns_entry_type entry,
        uint16_t query_id,
        uint16_t rtype,
        uint16_t rclass,
        uint32_t ttl,
        const void *data,
        size_t size,
        size_t name_offset,
        size_t name_length,
        size_t record_offset,
        size_t record_length,
        void *user_data
    );

public:
    /// Initializes the server.
    void init(const std::map<uint16_t, std::string> &new_services);

    /// Starts listening. This doesn't return until stop() is called from
    /// another thread.
    void run();

    /// Stops listening. Call from another thread to get run() to return.
    void stop();

    /// Destructor.
    ~MdnsServer();
};
