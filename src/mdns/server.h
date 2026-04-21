#pragma once

#include <atomic>
#include <optional>
#include <vector>

#include "config.h"
#include "mdns.h"
#include "string_table.h"

// Must come after mdns.h...
#ifdef _WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

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

    /// Service discovery name.
    static constexpr auto DNS_SD_SERVICE = "_services._dns-sd._udp.local.";

    /// Service map from port number to app name.
    const MdnsConfiguration *configuration = nullptr;

    /// Local IPv4 address. If there are multiple, the first enumerated address
    /// is assumed to be the primary one.
    std::optional<sockaddr_in> local_ipv4;

    /// Local IPv6 address. If there are multiple, the first enumerated address
    /// is assumed to be the primary one.
    std::optional<sockaddr_in6> local_ipv6;

    /// Storage for mDNS strings. Ensures that such strings are freed on
    /// destruction.
    MdnsStringTable mdns_strings;

    /// PTR records sent in response to service discovery questions.
    std::vector<mdns_record_t> dns_sd_ptr_records;

    /// Response records for service PTR queries, indexed by port.
    std::map<uint16_t, mdns_record_t> app_ptr_records;

    /// SRV records for service instance queries, indexed by port.
    std::map<uint16_t, mdns_record_t> app_srv_records;

    /// TXT records for service instance queries, indexed by port.
    std::map<uint16_t, mdns_record_t> app_txt_records;

    /// A record for our hostname.
    std::optional<mdns_record_t> record_a;

    /// AAAA record for our hostname.
    std::optional<mdns_record_t> record_aaaa;

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
    void init(const MdnsConfiguration &new_configuration);

    /// Starts listening. This doesn't return until stop() is called from
    /// another thread.
    void run();

    /// Stops listening. Call from another thread to get run() to return.
    void stop();

    /// Destructor.
    ~MdnsServer();
};
