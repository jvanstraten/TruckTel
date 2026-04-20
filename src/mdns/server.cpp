#include "server.h"

#include <cstring>

#ifdef _WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

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
    const sockaddr *addr, const socklen_t addrlen
) {
    char host[NI_MAXHOST] = {0};
    const int ret = getnameinfo(
        addr, addrlen, host, NI_MAXHOST, nullptr, NI_MAXSERV, NI_NUMERICHOST
    );
    if (ret != 0) throw std::runtime_error("getnameinfo() failed");
    return host;
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
                try {
                    local_ipv4 = std::make_pair<uint32_t, std::string>(
                        address->sin_addr.S_un.S_addr,
                        sockaddr_to_string(
                            unicast->Address.lpSockaddr, sizeof(sockaddr_in)
                        )
                    );
                } catch (const std::exception &e) {
                    Logger::error(
                        "Error getting local IPv4 address: %s", e.what()
                    );
                }
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
                try {
                    local_ipv6 = std::make_pair<
                        std::array<uint8_t, IPV6_LEN>, std::string>(
                        {},
                        sockaddr_to_string(
                            unicast->Address.lpSockaddr, sizeof(sockaddr_in6)
                        )
                    );
                    memcpy(
                        local_ipv6->first.data(), &address->sin6_addr, IPV6_LEN
                    );
                } catch (const std::exception &e) {
                    Logger::error(
                        "Error getting local IPv6 address: %s", e.what()
                    );
                }
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
        if (ifa->ifa_addr->sa_family == AF_INET) {
            const auto address =
                reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            if (address->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) continue;
            try {
                local_ipv4 = std::make_pair<uint32_t, std::string>(
                    std::move(address->sin_addr.s_addr),
                    sockaddr_to_string(ifa->ifa_addr, sizeof(sockaddr_in))
                );
            } catch (const std::exception &e) {
                Logger::error("Error getting local IPv4 address: %s", e.what());
            }
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            const auto address =
                reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr);
            if (!memcmp(address->sin6_addr.s6_addr, IPV6_LOCALHOST, 16))
                continue;
            if (!memcmp(address->sin6_addr.s6_addr, IPV6_LOCALHOST_MAPPED, 16))
                continue;
            try {
                local_ipv6 =
                    std::make_pair<std::array<uint8_t, IPV6_LEN>, std::string>(
                        {},
                        sockaddr_to_string(ifa->ifa_addr, sizeof(sockaddr_in6))
                    );
                memcpy(local_ipv6->first.data(), &address->sin6_addr, IPV6_LEN);
            } catch (const std::exception &e) {
                Logger::error("Error getting local IPv6 address: %s", e.what());
            }
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

    /*auto fromaddrstr = sockaddr_to_string(from, addrlen);
    const char* entrytype = (entry == MDNS_ENTRYTYPE_ANSWER) ?
                            "answer" :
                            ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" :
    "additional"); char namebuffer[256]; char entrybuffer[256]; mdns_string_t
    entrystr = mdns_string_extract(data, size, &name_offset, entrybuffer,
    sizeof(entrybuffer)); if (rtype == MDNS_RECORDTYPE_SRV) { mdns_record_srv_t
    srv = mdns_record_parse_srv(data, size, record_offset, record_length,
                                                      namebuffer,
    sizeof(namebuffer)); Logger::info("%s : %s %.*s SRV %.*s priority %d weight
    %d port %d", fromaddrstr.c_str(), entrytype, MDNS_STRING_FORMAT(entrystr),
               MDNS_STRING_FORMAT(srv.name), srv.priority, srv.weight,
    srv.port); } else if (rtype == MDNS_RECORDTYPE_A && instance.local_ipv4) {
        mdns
        mdns_query_answer(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
    query_id, service_record->service, service_length, service_record->hostname,
    strlen(service_record->hostname), service_record->address_ipv4,
    service_record->address_ipv6, (uint16_t)service_record->port, txt_record,
    sizeof(txt_record));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        mdns_record_parse_a(data, size, record_offset, record_length, &addr);
        auto addrstr = sockaddr_to_string(reinterpret_cast<sockaddr*>(&addr),
    sizeof(addr)); Logger::info("%s : %s %.*s A %s", fromaddrstr.c_str(),
    entrytype, MDNS_STRING_FORMAT(entrystr), addrstr.c_str());

    } else if (rtype == MDNS_RECORDTYPE_AAAA && instance.local_ipv6) {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr);
        auto addrstr = sockaddr_to_string(reinterpret_cast<sockaddr*>(&addr),
    sizeof(addr)); Logger::info("%s : %s %.*s AAAA %s", fromaddrstr.c_str(),
    entrytype, MDNS_STRING_FORMAT(entrystr), addrstr.c_str());
    }
    return 0;*/

    const char dns_sd[] = "_services._dns-sd._udp.local.";

    char namebuffer[256];
    size_t offset = name_offset;
    mdns_string_t name = mdns_string_extract(
        data, size, &offset, namebuffer, sizeof(namebuffer)
    );

    const char *record_name = 0;
    if (rtype == MDNS_RECORDTYPE_PTR)
        record_name = "PTR";
    else if (rtype == MDNS_RECORDTYPE_SRV)
        record_name = "SRV";
    else if (rtype == MDNS_RECORDTYPE_A)
        record_name = "A";
    else if (rtype == MDNS_RECORDTYPE_AAAA)
        record_name = "AAAA";
    else if (rtype == MDNS_RECORDTYPE_TXT)
        record_name = "TXT";
    else if (rtype == MDNS_RECORDTYPE_ANY)
        record_name = "ANY";
    else
        return 0;
    Logger::info("mDNS query %s %.*s", record_name, MDNS_STRING_FORMAT(name));

    /*if ((name.length == (sizeof(dns_sd) - 1)) &&
        (strncmp(name.str, dns_sd, sizeof(dns_sd) - 1) == 0)) {
            if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype ==
    MDNS_RECORDTYPE_ANY)) {
                    // The PTR query was for the DNS-SD domain, send answer with
    a PTR record for the
                    // service name we advertise, typically on the
    "<_service-name>._tcp.local." format

                    // Answer PTR record reverse mapping
    "<_service-name>._tcp.local." to
                    // "<hostname>.<_service-name>._tcp.local."
                    mdns_record_t answer = {};
                answer.name = name;
                answer.type = MDNS_RECORDTYPE_PTR;
                answer.data.ptr.name.str = "kokolores"; // TODO
                answer.data.ptr.name.length = strlen(answer.data.ptr.name.str);

                    // Send the answer, unicast or multicast depending on flag
    in query uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE); printf("  -->
    answer %.*s (%s)\n", MDNS_STRING_FORMAT(answer.data.ptr.name), (unicast ?
    "unicast" : "multicast"));

                    if (unicast) {
                            mdns_query_answer_unicast(sock, from, addrlen,
    sendbuffer, sizeof(sendbuffer), query_id, rtype, name.str, name.length,
    answer, 0, 0, 0, 0); } else { mdns_query_answer_multicast(sock, sendbuffer,
    sizeof(sendbuffer), answer, 0, 0, 0, 0);
                    }
            }
    } else if ((name.length == service->service.length) &&
               (strncmp(name.str, service->service.str, name.length) == 0)) {
            if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype ==
    MDNS_RECORDTYPE_ANY)) {
                    // The PTR query was for our service (usually
    "<_service-name._tcp.local"), answer a PTR
                    // record reverse mapping the queried service name to our
    service instance name
                    // (typically on the
    "<hostname>.<_service-name>._tcp.local." format), and add
                    // additional records containing the SRV record mapping the
    service instance name to our
                    // qualified hostname (typically "<hostname>.local.") and
    port, as well as any IPv4/IPv6
                    // address for the hostname as A/AAAA records, and two test
    TXT records

                    // Answer PTR record reverse mapping
    "<_service-name>._tcp.local." to
                    // "<hostname>.<_service-name>._tcp.local."
                    mdns_record_t answer = service->record_ptr;

                    mdns_record_t additional[5] = {0};
                    size_t additional_count = 0;

                    // SRV record mapping
    "<hostname>.<_service-name>._tcp.local." to
                    // "<hostname>.local." with port. Set weight & priority to
    0. additional[additional_count++] = service->record_srv;

                    // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6
    addresses if (service->address_ipv4.sin_family == AF_INET)
                            additional[additional_count++] = service->record_a;
                    if (service->address_ipv6.sin6_family == AF_INET6)
                            additional[additional_count++] =
    service->record_aaaa;

                    // Add two test TXT records for our service instance name,
    will be coalesced into
                    // one record with both key-value pair strings by the
    library additional[additional_count++] = service->txt_record[0];
                    additional[additional_count++] = service->txt_record[1];

                    // Send the answer, unicast or multicast depending on flag
    in query uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE); printf("  -->
    answer %.*s (%s)\n", MDNS_STRING_FORMAT(service->record_ptr.data.ptr.name),
                           (unicast ? "unicast" : "multicast"));

                    if (unicast) {
                            mdns_query_answer_unicast(sock, from, addrlen,
    sendbuffer, sizeof(sendbuffer), query_id, rtype, name.str, name.length,
    answer, 0, 0, additional, additional_count); } else {
                            mdns_query_answer_multicast(sock, sendbuffer,
    sizeof(sendbuffer), answer, 0, 0, additional, additional_count);
                    }
            }
    } else if ((name.length == service->service_instance.length) &&
               (strncmp(name.str, service->service_instance.str, name.length) ==
    0)) { if ((rtype == MDNS_RECORDTYPE_SRV) || (rtype == MDNS_RECORDTYPE_ANY))
    {
                    // The SRV query was for our service instance (usually
                    // "<hostname>.<_service-name._tcp.local"), answer a SRV
    record mapping the service
                    // instance name to our qualified hostname (typically
    "<hostname>.local.") and port, as
                    // well as any IPv4/IPv6 address for the hostname as A/AAAA
    records, and two test TXT
                    // records

                    // Answer PTR record reverse mapping
    "<_service-name>._tcp.local." to
                    // "<hostname>.<_service-name>._tcp.local."
                    mdns_record_t answer = service->record_srv;

                    mdns_record_t additional[5] = {0};
                    size_t additional_count = 0;

                    // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6
    addresses if (service->address_ipv4.sin_family == AF_INET)
                            additional[additional_count++] = service->record_a;
                    if (service->address_ipv6.sin6_family == AF_INET6)
                            additional[additional_count++] =
    service->record_aaaa;

                    // Add two test TXT records for our service instance name,
    will be coalesced into
                    // one record with both key-value pair strings by the
    library additional[additional_count++] = service->txt_record[0];
                    additional[additional_count++] = service->txt_record[1];

                    // Send the answer, unicast or multicast depending on flag
    in query uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE); printf("  -->
    answer %.*s port %d (%s)\n",
                           MDNS_STRING_FORMAT(service->record_srv.data.srv.name),
    service->port, (unicast ? "unicast" : "multicast"));

                    if (unicast) {
                            mdns_query_answer_unicast(sock, from, addrlen,
    sendbuffer, sizeof(sendbuffer), query_id, rtype, name.str, name.length,
    answer, 0, 0, additional, additional_count); } else {
                            mdns_query_answer_multicast(sock, sendbuffer,
    sizeof(sendbuffer), answer, 0, 0, additional, additional_count);
                    }
            }
    } else if ((name.length == service->hostname_qualified.length) &&
               (strncmp(name.str, service->hostname_qualified.str, name.length)
    == 0)) { if (((rtype == MDNS_RECORDTYPE_A) || (rtype ==
    MDNS_RECORDTYPE_ANY)) && (service->address_ipv4.sin_family == AF_INET)) {
                    // The A query was for our qualified hostname (typically
    "<hostname>.local.") and we
                    // have an IPv4 address, answer with an A record mappiing
    the hostname to an IPv4
                    // address, as well as any IPv6 address for the hostname,
    and two test TXT records

                    // Answer A records mapping "<hostname>.local." to IPv4
    address mdns_record_t answer = service->record_a;

                    mdns_record_t additional[5] = {0};
                    size_t additional_count = 0;

                    // AAAA record mapping "<hostname>.local." to IPv6 addresses
                    if (service->address_ipv6.sin6_family == AF_INET6)
                            additional[additional_count++] =
    service->record_aaaa;

                    // Add two test TXT records for our service instance name,
    will be coalesced into
                    // one record with both key-value pair strings by the
    library additional[additional_count++] = service->txt_record[0];
                    additional[additional_count++] = service->txt_record[1];

                    // Send the answer, unicast or multicast depending on flag
    in query uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE); mdns_string_t
    addrstr = ip_address_to_string( addrbuffer, sizeof(addrbuffer), (struct
    sockaddr*)&service->record_a.data.a.addr,
                        sizeof(service->record_a.data.a.addr));
                    printf("  --> answer %.*s IPv4 %.*s (%s)\n",
    MDNS_STRING_FORMAT(service->record_a.name), MDNS_STRING_FORMAT(addrstr),
    (unicast ? "unicast" : "multicast"));

                    if (unicast) {
                            mdns_query_answer_unicast(sock, from, addrlen,
    sendbuffer, sizeof(sendbuffer), query_id, rtype, name.str, name.length,
    answer, 0, 0, additional, additional_count); } else {
                            mdns_query_answer_multicast(sock, sendbuffer,
    sizeof(sendbuffer), answer, 0, 0, additional, additional_count);
                    }
            } else if (((rtype == MDNS_RECORDTYPE_AAAA) || (rtype ==
    MDNS_RECORDTYPE_ANY)) && (service->address_ipv6.sin6_family == AF_INET6)) {
                    // The AAAA query was for our qualified hostname (typically
    "<hostname>.local.") and we
                    // have an IPv6 address, answer with an AAAA record mappiing
    the hostname to an IPv6
                    // address, as well as any IPv4 address for the hostname,
    and two test TXT records

                    // Answer AAAA records mapping "<hostname>.local." to IPv6
    address mdns_record_t answer = service->record_aaaa;

                    mdns_record_t additional[5] = {0};
                    size_t additional_count = 0;

                    // A record mapping "<hostname>.local." to IPv4 addresses
                    if (service->address_ipv4.sin_family == AF_INET)
                            additional[additional_count++] = service->record_a;

                    // Add two test TXT records for our service instance name,
    will be coalesced into
                    // one record with both key-value pair strings by the
    library additional[additional_count++] = service->txt_record[0];
                    additional[additional_count++] = service->txt_record[1];

                    // Send the answer, unicast or multicast depending on flag
    in query uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE); mdns_string_t
    addrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), (struct
    sockaddr*)&service->record_aaaa.data.aaaa.addr,
                                             sizeof(service->record_aaaa.data.aaaa.addr));
                    printf("  --> answer %.*s IPv6 %.*s (%s)\n",
                           MDNS_STRING_FORMAT(service->record_aaaa.name),
    MDNS_STRING_FORMAT(addrstr), (unicast ? "unicast" : "multicast"));

                    if (unicast) {
                            mdns_query_answer_unicast(sock, from, addrlen,
    sendbuffer, sizeof(sendbuffer), query_id, rtype, name.str, name.length,
    answer, 0, 0, additional, additional_count); } else {
                            mdns_query_answer_multicast(sock, sendbuffer,
    sizeof(sendbuffer), answer, 0, 0, additional, additional_count);
                    }
            }
    }*/
    return 0;
}

void MdnsServer::init(const std::map<uint16_t, std::string> &new_services) {
    services = new_services;

    // Find our own IP address(es).
    find_local_ips();
    if (local_ipv4)
        Logger::info("Local IPv4 address is %s", local_ipv4->second.c_str());
    if (local_ipv6)
        Logger::info("Local IPv6 address is %s", local_ipv6->second.c_str());
    if (!local_ipv4 && !local_ipv6) {
        Logger::warn("mDNS disabled: could not determine local IP address.");
        return;
    }

    // Open sockets.
    open_sockets();
    if (sockets.empty()) {
        Logger::warn("mDNS disabled: could not open any socket.");
        return;
    }
}

void MdnsServer::run() {
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
