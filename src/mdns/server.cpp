#include "server.h"

#include <cstring>
#include <string>

#include "logger.h"

// NOTE: some code in this file was lifted from
// https://github.com/gocarlos/mdns_cpp. mDNS-cpp was not used directly because
// it lacks A/AAAA support, because the lack of mutexes in shutting down the
// thread is questionable, and because we don't need the query side. Code that
// was taken or modified from mDNS-cpp will be marked as such with comments.
//
// License of applicable code:
//
// Copyright (c) 2020 Carlos Gomes Martinho
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

static std::string sockaddr_to_string(
    const sockaddr &addr, const socklen_t addrlen
) {
    char host[NI_MAXHOST] = {0};
    const int ret = getnameinfo(
        &addr, addrlen, host, NI_MAXHOST, nullptr, NI_MAXSERV,
        NI_NUMERICHOST | NI_NUMERICSERV
    );
    if (ret != 0) throw std::runtime_error("getnameinfo() failed");
    return host;
}

static std::string sockaddr_to_string(const sockaddr_in &addr) {
    return sockaddr_to_string(
        reinterpret_cast<const sockaddr &>(addr), sizeof(sockaddr_in)
    );
}

static std::string sockaddr_to_string(const sockaddr_in6 &addr) {
    return sockaddr_to_string(
        reinterpret_cast<const sockaddr &>(addr), sizeof(sockaddr_in6)
    );
}

void MdnsServer::find_local_ips() {
    // Based on mDNS-cpp mDNS::openClientSockets(), with the actual socket
    // opening removed. We're just interested in finding our IP address.

#ifdef _WIN32

    IP_ADAPTER_ADDRESSES *adapter_addresses = nullptr;
    ULONG address_size = 15000;
    unsigned int ret{};
    unsigned int num_retries = 4;
    do {
        adapter_addresses =
            static_cast<IP_ADAPTER_ADDRESSES *>(malloc(address_size));
        ret = GetAdaptersAddresses(
            AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, nullptr,
            adapter_addresses, &address_size
        );
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(adapter_addresses);
            adapter_addresses = nullptr;
        } else {
            break;
        }
    } while (num_retries-- > 0);

    if (!adapter_addresses || (ret != NO_ERROR)) {
        free(adapter_addresses);
        Logger::error("Failed to get network adapter addresses");
        return;
    }

    for (PIP_ADAPTER_ADDRESSES adapter = adapter_addresses; adapter;
         adapter = adapter->Next) {
        if (adapter->TunnelType == TUNNEL_TYPE_TEREDO) continue;
        if (adapter->OperStatus != IfOperStatusUp) continue;
        for (const IP_ADAPTER_UNICAST_ADDRESS *unicast =
                 adapter->FirstUnicastAddress;
             unicast; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                if (local_ipv4) continue;
                const auto address = reinterpret_cast<struct sockaddr_in *>(
                    unicast->Address.lpSockaddr
                );
                if (address->sin_addr.S_un.S_un_b.s_b1 == 127 &&
                    address->sin_addr.S_un.S_un_b.s_b2 == 0 &&
                    address->sin_addr.S_un.S_un_b.s_b3 == 0 &&
                    address->sin_addr.S_un.S_un_b.s_b4 == 1) {
                    continue;
                }
                local_ipv4 = *address;
            } else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
                if (local_ipv6) continue;
                if (unicast->DadState != NldsPreferred) continue;
                const auto address = reinterpret_cast<struct sockaddr_in6 *>(
                    unicast->Address.lpSockaddr
                );
                if (!memcmp(
                        address->sin6_addr.s6_addr, IPV6_LOCALHOST, IPV6_LEN
                    ))
                    continue;
                if (!memcmp(
                        address->sin6_addr.s6_addr, IPV6_LOCALHOST_MAPPED,
                        IPV6_LEN
                    ))
                    continue;
                local_ipv6 = *address;
            }
        }
    }

    free(adapter_addresses);

#else

    struct ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) < 0) {
        Logger::error("Failed to get network adapter addresses");
        return;
    }

    for (auto ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_MULTICAST))
            continue;
        if ((ifa->ifa_flags & IFF_LOOPBACK) ||
            (ifa->ifa_flags & IFF_POINTOPOINT))
            continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (local_ipv4) continue;
            const auto address =
                reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            if (address->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) continue;
            local_ipv4 = *address;
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            if (local_ipv6) continue;
            const auto address =
                reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr);
            if (!memcmp(address->sin6_addr.s6_addr, IPV6_LOCALHOST, 16))
                continue;
            if (!memcmp(address->sin6_addr.s6_addr, IPV6_LOCALHOST_MAPPED, 16))
                continue;
            local_ipv6 = *address;
        }
    }

    freeifaddrs(ifaddr);

#endif
}

void MdnsServer::open_sockets() {
    // Based on mDNS-cpp mDNS::openServiceSockets().
    if (local_ipv4) {
        sockaddr_in sock_addr{};
        sock_addr.sin_family = AF_INET;
#ifdef _WIN32
        sock_addr.sin_addr = in4addr_any;
#else
        sock_addr.sin_addr.s_addr = INADDR_ANY;
#endif
        sock_addr.sin_port = htons(MDNS_PORT);
#ifdef __APPLE__
        sock_addr.sin_len = sizeof(struct sockaddr_in);
#endif
        const int sock = mdns_socket_open_ipv4(&sock_addr);
        if (sock >= 0) {
            sockets.emplace_back(sock);
        }
    }

    if (local_ipv6) {
        sockaddr_in6 sock_addr{};
        sock_addr.sin6_family = AF_INET6;
        sock_addr.sin6_addr = in6addr_any;
        sock_addr.sin6_port = htons(MDNS_PORT);
#ifdef __APPLE__
        sock_addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
        int sock = mdns_socket_open_ipv6(&sock_addr);
        sockets.emplace_back(sock);
    }
}

void MdnsServer::close_sockets() {
    for (const auto socket : sockets) {
        mdns_socket_close(socket);
    }
    sockets.clear();
}

/// Converts an mdns_string_t to a std::string.
static std::string mdns_string_to_std(const mdns_string_t &mdns_string) {
    return {mdns_string.str, mdns_string.length};
}

int MdnsServer::service_callback(
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
) {
    // Ignore anything that isn't a question.
    if (entry != MDNS_ENTRYTYPE_QUESTION) return 0;

    // Get instance pointer via user data.
    if (!user_data) return 0;
    MdnsServer &instance = *static_cast<MdnsServer *>(user_data);

    // Get the requested name from the query.
    char name_buffer[256];
    const auto name_mdns = mdns_string_extract(
        data, size, &name_offset, name_buffer, sizeof(name_buffer)
    );
    const auto name = mdns_string_to_std(name_mdns);

    // Answers to send in mDNS library form.
    std::vector<mdns_record_t> answers;

    // Handle service discovery queries.
    if (name == DNS_SD_SERVICE) {
        if (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY) {

            // The PTR query was for the DNS-SD domain, send answers with a PTR
            // record for each of the services we advertise.
            answers = instance.dns_sd_ptr_records;
        }
    }

    // Handle service name to instance queries.
    if (answers.empty()) {
        const auto &services = instance.configuration->get_service_to_port();
        const auto service_it = services.find(name);
        if (service_it != services.end()) {
            const auto port = service_it->second;
            if (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY) {

                // The PTR query was for one of our services. Answer a PTR
                // record reverse mapping the queried service name to our
                // service instance name, and add additional records containing
                // the SRV record mapping the service instance name to our
                // qualified hostname and port, as well as any IPv4/IPv6 address
                // for the hostname as A/AAAA records.
                answers.emplace_back(instance.app_ptr_records[port]);
                answers.emplace_back(instance.app_srv_records[port]);
                if (instance.record_a) {
                    answers.emplace_back(*instance.record_a);
                }
                if (instance.record_aaaa) {
                    answers.emplace_back(*instance.record_aaaa);
                }
            }
        }
    }

    // Handle service instance queries.
    if (answers.empty()) {
        const auto &instances =
            instance.configuration->get_service_instance_to_port();
        const auto instance_it = instances.find(name);
        if (instance_it != instances.end()) {
            const auto port = instance_it->second;
            if (rtype == MDNS_RECORDTYPE_SRV || rtype == MDNS_RECORDTYPE_ANY) {

                // The SRV query was for one of our service instances. Answer an
                // SRV record mapping the service instance name to our qualified
                // hostname and port, as well as any IPv4/IPv6 address for the
                // hostname as A/AAAA records.
                answers.emplace_back(instance.app_srv_records[port]);
                if (instance.record_a) {
                    answers.emplace_back(*instance.record_a);
                }
                if (instance.record_aaaa) {
                    answers.emplace_back(*instance.record_aaaa);
                }
                answers.emplace_back(instance.app_txt_records[port]);
            }
        }
    }

    // Handle domain queries.
    if (answers.empty()) {
        if (name == instance.configuration->get_qualified_hostname()) {
            if ((rtype == MDNS_RECORDTYPE_A || rtype == MDNS_RECORDTYPE_ANY) &&
                instance.record_a) {

                // The A query was for our qualified hostname, and we have an
                // IPv4 address. Answer with an A record mapping the hostname to
                // an IPv4 address, as well as any IPv6 address for the
                // hostname.
                answers.emplace_back(*instance.record_a);
                if (instance.record_aaaa) {
                    answers.emplace_back(*instance.record_aaaa);
                }

            } else if ((rtype == MDNS_RECORDTYPE_AAAA ||
                        rtype == MDNS_RECORDTYPE_ANY) &&
                       instance.record_aaaa) {

                // The AAAA query was for our qualified hostname, and we have an
                // IPv6 address. Answer with an AAAA record mapping the hostname
                // to an IPv6 address, as well as any IPv4 address for the
                // hostname.
                answers.emplace_back(*instance.record_aaaa);
                if (instance.record_a) {
                    answers.emplace_back(*instance.record_a);
                }
            }
        }
    }

    // If we don't have anything to answer, exit here.
    if (answers.empty()) {
        return 0;
    }

    // Whether we've been requested to answer as unicast or as multicast.
    const bool unicast = (rclass & MDNS_UNICAST_RESPONSE) != 0;

    // Print the answers we're sending.
    if (instance.configuration->is_verbose()) {
        // Log the query if verbose logging is enabled.
        const char *rtype_str;
        switch (rtype) {
            case MDNS_RECORDTYPE_A:
                rtype_str = "A";
                break;
            case MDNS_RECORDTYPE_PTR:
                rtype_str = "PTR";
                break;
            case MDNS_RECORDTYPE_TXT:
                rtype_str = "TXT";
                break;
            case MDNS_RECORDTYPE_AAAA:
                rtype_str = "AAAA";
                break;
            case MDNS_RECORDTYPE_SRV:
                rtype_str = "SRV";
                break;
            case MDNS_RECORDTYPE_ANY:
                rtype_str = "ANY";
                break;
            default:
                rtype_str = nullptr;
                break;
        }
        if (rtype_str) {
            Logger::verbose("mDNS query %s %s", rtype_str, name.c_str());
        } else {
            Logger::verbose("mDNS query %d %s", rtype, name.c_str());
        }
        Logger::verbose(
            "mDNS answer unicast%s:", unicast ? "" : " and multicast"
        );
        for (const auto &answer : answers) {
            switch (answer.type) {
                case MDNS_RECORDTYPE_A:
                    Logger::verbose(
                        " - A: %s",
                        sockaddr_to_string(answer.data.a.addr).c_str()
                    );
                    break;
                case MDNS_RECORDTYPE_AAAA:
                    Logger::verbose(
                        " - AAAA: %s",
                        sockaddr_to_string(answer.data.aaaa.addr).c_str()
                    );
                    break;
                case MDNS_RECORDTYPE_PTR:
                    Logger::verbose(
                        " - PTR: %.*s", MDNS_STRING_FORMAT(answer.data.ptr.name)
                    );
                    break;
                case MDNS_RECORDTYPE_TXT:
                    Logger::verbose(
                        " - TXT: %.*s = %.*s",
                        MDNS_STRING_FORMAT(answer.data.txt.key),
                        MDNS_STRING_FORMAT(answer.data.txt.value)
                    );
                    break;
                case MDNS_RECORDTYPE_SRV:
                    Logger::verbose(
                        " - SRV: %.*s:%d",
                        MDNS_STRING_FORMAT(answer.data.srv.name),
                        answer.data.srv.port
                    );
                    break;
                default:
                    Logger::verbose(" - unknown record type");
                    break;
            }
        }
    }

    // Send the actual response.
    char buffer[4096] = {};
    if (!unicast) {
        mdns_query_answer_multicast(
            sock, buffer, sizeof(buffer), answers.front(), 0, 0,
            &answers.front() + 1, answers.size() - 1
        );
    }

    // Multicast packets are often dropped by routers, firewalls, whatever. So
    // even if a unicast response was not requested, send one anyway.
    mdns_query_answer_unicast(
        sock, from, addrlen, buffer, sizeof(buffer), query_id,
        static_cast<mdns_record_type_t>(rtype), name_mdns.str, name_mdns.length,
        answers.front(), 0, 0, &answers.front() + 1, answers.size() - 1
    );

    return 0;
}

void MdnsServer::init(const MdnsConfiguration &new_configuration) {
    configuration = &new_configuration;

    // Find our own IP address(es).
    find_local_ips();
    if (local_ipv4) {
        Logger::info(
            "Local IPv4 address is %s", sockaddr_to_string(*local_ipv4).c_str()
        );
    }
    if (local_ipv6) {
        Logger::info(
            "Local IPv6 address is %s", sockaddr_to_string(*local_ipv6).c_str()
        );
    }

    // Stop initializing here if mDNS is disabled by the user.
    if (!configuration->is_mdns_enabled()) {
        Logger::info("mDNS is disabled by the user.");
        return;
    }

    // Stop initialization if we didn't find any IP addresses to advertise.
    if (!local_ipv4 && !local_ipv6) {
        Logger::warn("mDNS disabled: could not determine local IP address.");
        return;
    }

    // Pre-construct all the response records.
    mdns_record_t record;
    auto dns_sd_service_mdns = mdns_strings.allocate(DNS_SD_SERVICE);
    auto qualified_hostname_mdns =
        mdns_strings.allocate(configuration->get_qualified_hostname());
    auto app_key_mdns = mdns_strings.allocate("app");
    for (const auto &[port, app] : configuration->get_port_to_app()) {
        auto app_name_mdns = mdns_strings.allocate(app);
        auto service = configuration->app_to_service(app);
        auto service_mdns = mdns_strings.allocate(service);
        auto service_instance = configuration->service_to_instance(service);
        auto service_instance_mdns = mdns_strings.allocate(service_instance);

        // Service-discovery PTR records.
        record.name = dns_sd_service_mdns;
        record.type = MDNS_RECORDTYPE_PTR;
        record.data.ptr.name = service_mdns;
        record.rclass = 0;
        record.ttl = 0;
        dns_sd_ptr_records.emplace_back(record);

        // PTR record reverse mapping "<_service-name>._tcp.local." to
        // "<hostname>.<_service-name>._tcp.local.".
        record.name = service_mdns;
        record.type = MDNS_RECORDTYPE_PTR;
        record.data.ptr.name = service_instance_mdns;
        record.rclass = 0;
        record.ttl = 0;
        app_ptr_records[port] = record;

        // SRV record mapping "<hostname>.<_service-name>._tcp.local." to
        // "<hostname>.local." with port. Set weight & priority to 0.
        record.name = service_instance_mdns;
        record.type = MDNS_RECORDTYPE_SRV;
        record.data.srv.name = qualified_hostname_mdns;
        record.data.srv.port = port;
        record.data.srv.priority = 0;
        record.data.srv.weight = 0;
        record.rclass = 0;
        record.ttl = 0;
        app_srv_records[port] = record;

        // TXT record.
        record.name = service_instance_mdns;
        record.type = MDNS_RECORDTYPE_TXT;
        record.data.txt.key = app_key_mdns;
        record.data.txt.value = app_name_mdns;
        record.rclass = 0;
        record.ttl = 0;
        app_txt_records[port] = record;
    }
    if (local_ipv4) {
        record.name = qualified_hostname_mdns;
        record.type = MDNS_RECORDTYPE_A;
        record.data.a.addr = *local_ipv4;
        record.rclass = 0;
        record.ttl = 0;
        record_a = record;
    }
    if (local_ipv6) {
        record.name = qualified_hostname_mdns;
        record.type = MDNS_RECORDTYPE_AAAA;
        record.data.aaaa.addr = *local_ipv6;
        record.rclass = 0;
        record.ttl = 0;
        record_aaaa = record;
    }

    // Open sockets.
    open_sockets();
    if (sockets.empty()) {
        Logger::warn("mDNS disabled: could not open any socket.");
    }
}

void MdnsServer::run() {
    if (!configuration) return;
    if (sockets.empty()) return;

    // Based on mDNS-cpp mDNS::runMainLoop().
    static constexpr size_t BUF_CAPACITY = 2048u;
    std::shared_ptr<void> buf(malloc(BUF_CAPACITY), free);

    while (!shutdown_requested.load()) {
        int nfds = 0;
        fd_set readfs{};
        FD_ZERO(&readfs);
        for (const auto socket : sockets) {
            if (socket >= nfds) nfds = socket + 1;
            FD_SET(socket, &readfs);
        }

        timeval TIMEOUT = {0, 500000};
        const auto ret = select(nfds, &readfs, nullptr, nullptr, &TIMEOUT);
        if (ret < 0) {
            Logger::error("mDNS select() failed, stopping mDNS service");
            return;
        }
        if (ret > 0) {
            for (const auto socket : sockets) {
                if (FD_ISSET(socket, &readfs)) {
                    mdns_socket_listen(
                        socket, buf.get(), BUF_CAPACITY, service_callback, this
                    );
                }
                FD_SET(socket, &readfs);
            }
        }
    }
}

void MdnsServer::stop() {
    shutdown_requested.store(true);
}

MdnsServer::~MdnsServer() {
    close_sockets();
}
