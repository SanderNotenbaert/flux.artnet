/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "receiver_advertisement.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <set>
#include <utility>

namespace flux::artnet {

namespace {

struct LocalAddress {
    std::string ip;
    std::array<std::uint8_t, 6> mac{};
};

std::string ipv4_from_sockaddr(const sockaddr_in& address)
{
    char text[INET_ADDRSTRLEN] = {};
    if (::inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text)) == nullptr)
        return {};
    return text;
}

/// Discover the local IPv4 address the kernel would use to reach a peer.
std::string local_ipv4_for_peer(const std::string& peer_ip)
{
    if (peer_ip.empty())
        return {};

    const int descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (descriptor < 0)
        return {};

    sockaddr_in peer{};
    peer.sin_family = AF_INET;
    peer.sin_port = htons(kDefaultPort);
    if (::inet_pton(AF_INET, peer_ip.c_str(), &peer.sin_addr) != 1) {
        ::close(descriptor);
        return {};
    }

    if (::connect(descriptor, reinterpret_cast<const sockaddr*>(&peer), sizeof(peer)) != 0) {
        ::close(descriptor);
        return {};
    }

    sockaddr_in local{};
    socklen_t length = sizeof(local);
    std::string result;
    if (::getsockname(descriptor, reinterpret_cast<sockaddr*>(&local), &length) == 0)
        result = ipv4_from_sockaddr(local);

    ::close(descriptor);
    return result;
}

/// Fetch the hardware address belonging to a local IPv4 address.
void mac_for_ip(const std::string& ip, std::array<std::uint8_t, 6>& mac)
{
    if (ip.empty())
        return;

    ifaddrs* interfaces = nullptr;
    if (::getifaddrs(&interfaces) != 0)
        return;

    std::string interface_name;
    for (ifaddrs* entry = interfaces; entry != nullptr; entry = entry->ifa_next) {
        if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET)
            continue;

        const auto* address = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);
        char text[INET_ADDRSTRLEN] = {};
        if (::inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)) == nullptr)
            continue;
        if (ip == text) {
            interface_name = entry->ifa_name != nullptr ? entry->ifa_name : "";
            break;
        }
    }

    if (!interface_name.empty()) {
        for (ifaddrs* entry = interfaces; entry != nullptr; entry = entry->ifa_next) {
            if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_LINK || entry->ifa_name == nullptr)
                continue;
            if (interface_name != entry->ifa_name)
                continue;

            const auto* link = reinterpret_cast<const sockaddr_dl*>(entry->ifa_addr);
            if (link->sdl_alen == 6) {
                std::memcpy(mac.data(), LLADDR(link), 6);
                break;
            }
        }
    }

    ::freeifaddrs(interfaces);
}

std::vector<int> unique_universes(const std::vector<int>& universes)
{
    std::set<int> unique;
    for (const int universe : universes) {
        if (valid_port_address(universe))
            unique.insert(universe);
    }
    return {unique.begin(), unique.end()};
}

struct UniverseGroup {
    std::uint8_t net{0};
    std::uint8_t subnet{0};
    std::vector<int> universes;
};

std::vector<UniverseGroup> group_universes(const std::vector<int>& universes)
{
    std::map<std::pair<std::uint8_t, std::uint8_t>, std::vector<int>> groups;
    for (const int universe : universes) {
        const auto fields = split_port_address(universe);
        groups[{fields.net, fields.subnet}].push_back(universe);
    }

    std::vector<UniverseGroup> result;
    for (auto& entry : groups) {
        auto& values = entry.second;
        std::sort(values.begin(), values.end(), [](int lhs, int rhs) {
            return (lhs & 0x0F) < (rhs & 0x0F);
        });

        for (std::size_t offset = 0; offset < values.size(); offset += 4) {
            UniverseGroup group;
            group.net = entry.first.first;
            group.subnet = entry.first.second;
            const std::size_t end = std::min<std::size_t>(offset + 4, values.size());
            group.universes.assign(values.begin() + static_cast<std::ptrdiff_t>(offset), values.begin() + static_cast<std::ptrdiff_t>(end));
            result.push_back(std::move(group));
        }
    }
    return result;
}

ArtPollReplyPacket make_reply(const UniverseGroup& group, const LocalAddress& local, std::uint8_t bind_index)
{
    ArtPollReplyPacket reply;
    reply.ip = local.ip;
    reply.port = kDefaultPort;
    reply.firmware_version = 0x0300;
    reply.net = group.net;
    reply.subnet = group.subnet;
    reply.oem = 0xFFFF;
    reply.status1 = 0xC0; // Indicators in normal mode.
    reply.esta_manufacturer = 0x7FF0; // Prototype manufacturer code.
    reply.short_name = "flux.artnet";
    reply.long_name = "flux.artnet.receive";
    reply.node_report = "#0000 [0000] flux.artnet.receive ready";
    reply.num_ports = static_cast<std::uint16_t>(group.universes.size());
    reply.style = 0x06; // StVisual.
    reply.mac = local.mac;
    reply.bind_ip = local.ip;
    reply.bind_index = bind_index;
    reply.status2 = 0x08; // Supports 15-bit Port-Addresses.

    for (std::size_t i = 0; i < group.universes.size(); ++i) {
        reply.port_types[i] = 0x80; // Output port.
        reply.swout[i] = static_cast<std::uint8_t>(group.universes[i] & 0x0F);
    }

    return reply;
}

} // namespace

ReceiverAdvertisementService::ReceiverAdvertisementService(ReceiverNetworkService& receiver, std::uint16_t port)
    : m_receiver(receiver)
    , m_port(port)
{
}

ReceiverAdvertisementService::~ReceiverAdvertisementService()
{
    m_receiver.set_poll_handler({});
}

std::uint64_t ReceiverAdvertisementService::add_receiver(const std::vector<int>& universes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t id = m_next_id++;
    m_receivers[id] = Registration{unique_universes(universes)};

    if (m_receivers.size() == 1) {
        m_receiver.set_poll_handler([this](const std::uint8_t* data, std::size_t size, const sockaddr_in& source) {
            handle_artpoll(data, size, source);
        });
    }
    return id;
}

void ReceiverAdvertisementService::remove_receiver(std::uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_receivers.erase(id);
    if (m_receivers.empty())
        m_receiver.set_poll_handler({});
}

std::vector<int> ReceiverAdvertisementService::advertised_universes() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::set<int> unique;
    for (const auto& entry : m_receivers) {
        for (const int universe : entry.second.universes) {
            if (valid_port_address(universe))
                unique.insert(universe);
        }
    }
    return {unique.begin(), unique.end()};
}

std::vector<ArtPollReplyPacket> ReceiverAdvertisementService::replies_for(const std::string& local_ip) const
{
    return build_replies(advertised_universes(), local_ip);
}

std::vector<ArtPollReplyPacket> ReceiverAdvertisementService::build_replies(const std::vector<int>& universes, const std::string& local_ip) const
{
    if (universes.empty())
        return {};

    LocalAddress local;
    local.ip = local_ip.empty() ? "0.0.0.0" : local_ip;
    mac_for_ip(local.ip, local.mac);

    const auto groups = group_universes(universes);
    std::vector<ArtPollReplyPacket> replies;
    replies.reserve(groups.size());
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const auto bind_index = static_cast<std::uint8_t>(std::min<std::size_t>(i + 1, 255));
        replies.push_back(make_reply(groups[i], local, bind_index));
    }
    return replies;
}

std::size_t ReceiverAdvertisementService::receiver_count() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_receivers.size();
}

void ReceiverAdvertisementService::handle_artpoll(const std::uint8_t* data, std::size_t size, const sockaddr_in& source)
{
    ArtPollPacket poll;
    if (!decode_artpoll(data, size, poll))
        return;

    const auto universes = advertised_universes();
    if (universes.empty())
        return;

    if (poll.targeted()) {
        const bool matches = std::any_of(universes.begin(), universes.end(), [&](int universe) {
            return poll.matches(universe);
        });
        if (!matches)
            return;
    }

    const std::string source_ip = ipv4_from_sockaddr(source);
    if (source_ip.empty())
        return;

    const std::string local_ip = local_ipv4_for_peer(source_ip);
    const auto replies = build_replies(universes, local_ip);
    for (const auto& reply : replies) {
        const auto packet = encode_artpollreply(reply);
        std::string error;
        m_receiver.send_on_port(m_port, packet, {source_ip}, error);
    }
}

} // namespace flux::artnet
