/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "discovery.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <set>
#include <utility>

namespace flux::artnet {

DiscoveryService::DiscoveryService(SenderNetworkService& sender, ReceiverNetworkService& receiver, std::uint16_t port)
    : m_sender(sender)
    , m_receiver(receiver)
    , m_port(port)
{
}

DiscoveryService::~DiscoveryService()
{
    release();
}

bool DiscoveryService::acquire(std::string& error)
{
    std::lock_guard<std::mutex> lock(m_client_mutex);
    error.clear();

    if (m_clients == 0) {
        m_receiver.set_poll_reply_handler([this](const std::uint8_t* data, std::size_t size, const sockaddr_in& source) {
            handle_datagram(data, size, source);
        });

        m_subscription = m_receiver.subscribe_listener(m_port, error);
        if (!m_subscription) {
            m_receiver.set_poll_reply_handler({});
            return false;
        }
    }

    ++m_clients;
    return true;
}

void DiscoveryService::release()
{
    std::lock_guard<std::mutex> lock(m_client_mutex);
    if (m_clients == 0)
        return;

    --m_clients;
    if (m_clients > 0)
        return;

    auto subscription = std::move(m_subscription);
    m_subscription.reset();
    m_receiver.set_poll_reply_handler({});
    if (subscription)
        m_receiver.unsubscribe(subscription->port, subscription);
}

bool DiscoveryService::poll(std::string& error)
{
    error.clear();

    // Art-Net controllers poll the two Art-Net broadcast addresses as well as
    // the directed broadcast addresses of the local interfaces. Some nodes
    // are only reachable through one of these.
    std::set<std::string> unique;
    for (const auto& address : m_sender.broadcast_addresses()) {
        if (!address.empty())
            unique.insert(address);
    }
    unique.insert("2.255.255.255");
    unique.insert("10.255.255.255");
    // Poll loopback as well so a receive object in the same Max process is
    // discoverable even when no LAN interface is active.
    unique.insert("127.0.0.1");

    std::vector<std::string> destinations;
    destinations.reserve(unique.size());
    destinations.assign(unique.begin(), unique.end());

    return m_receiver.send_on_port(m_port, encode_artpoll(), destinations, error);
}

bool DiscoveryService::poll_hosts(const std::vector<std::string>& hosts, std::string& error)
{
    error.clear();

    std::set<std::string> unique;
    for (const auto& host : hosts) {
        if (!host.empty())
            unique.insert(host);
    }
    if (unique.empty()) {
        error = "no probe destination was supplied";
        return false;
    }

    std::vector<std::string> destinations(unique.begin(), unique.end());
    return m_receiver.send_on_port(m_port, encode_artpoll(), destinations, error);
}

std::vector<DiscoveredDevice> DiscoveryService::devices() const
{
    return m_registry.snapshot();
}

std::uint64_t DiscoveryService::generation() const
{
    return m_registry.generation();
}

void DiscoveryService::handle_datagram(const std::uint8_t* data, std::size_t size, const sockaddr_in& source)
{
    ArtPollReplyPacket reply;
    if (!decode_artpollreply(data, size, reply))
        return;

    char text[INET_ADDRSTRLEN] = {};
    std::string source_ip;
    if (::inet_ntop(AF_INET, &source.sin_addr, text, sizeof(text)) != nullptr)
        source_ip = text;

    m_registry.update(reply, source_ip, DeviceRegistry::Clock::now());
}

std::vector<std::string> resolve_ipv4_addresses(const std::string& host)
{
    std::vector<std::string> result;
    if (host.empty())
        return result;

    in_addr literal{};
    if (::inet_pton(AF_INET, host.c_str(), &literal) == 1) {
        result.push_back(host);
        return result;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* addresses = nullptr;
    if (::getaddrinfo(host.c_str(), nullptr, &hints, &addresses) != 0)
        return result;

    std::set<std::string> unique;
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        if (address->ai_family != AF_INET || address->ai_addr == nullptr)
            continue;
        const auto* socket_address = reinterpret_cast<const sockaddr_in*>(address->ai_addr);
        char text[INET_ADDRSTRLEN] = {};
        if (::inet_ntop(AF_INET, &socket_address->sin_addr, text, sizeof(text)) != nullptr)
            unique.insert(text);
    }
    ::freeaddrinfo(addresses);

    result.assign(unique.begin(), unique.end());
    return result;
}

} // namespace flux::artnet
