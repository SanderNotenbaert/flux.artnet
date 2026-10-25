/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "artnet_poll.hpp"
#include "export.hpp"
#include "network_service.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace flux::artnet {

/// One discovered Art-Net node.
///
/// The registry merges all ArtPollReply packets that report the same routable
/// IP address. Multi-port nodes that use Art-Net 4 BindIndex pages therefore
/// appear once, with the output universes of every page combined.
struct DiscoveredDevice {
    std::string ip;
    std::string source_ip;
    std::string bind_ip;
    std::string mac;
    std::string short_name;
    std::string long_name;
    std::string node_report;
    int num_ports{0};
    int firmware_version{0};
    int oem{0};
    int bind_index{0};
    std::vector<int> output_universes;
    std::vector<int> input_universes;
    std::chrono::steady_clock::time_point last_seen{};

    /// The name used by the device list and by the `device <name>` message.
    /// LongName wins, then ShortName, then the IP address.
    std::string display_name() const
    {
        if (!long_name.empty())
            return long_name;
        if (!short_name.empty())
            return short_name;
        return ip;
    }

    /// A stable description of everything this package exposes. Used to
    /// detect real device changes without reacting to polling timestamps.
    std::string signature() const
    {
        std::string text;
        text += ip;
        text += '\x1f';
        text += display_name();
        text += '\x1f';
        text += mac;
        text += '\x1f';
        text += short_name;
        text += '\x1f';
        text += long_name;
        text += '\x1f';
        text += std::to_string(num_ports);
        text += '\x1f';
        for (const int universe : output_universes) {
            text += std::to_string(universe);
            text += ',';
        }
        text += '\x1f';
        for (const int universe : input_universes) {
            text += std::to_string(universe);
            text += ',';
        }
        return text;
    }
};

/// Thread-safe registry of discovered Art-Net nodes.
///
/// The UDP listener thread calls update(); Max's scheduler thread calls
/// snapshot() and generation(). No Max API is used here.
class FLUX_ARTNET_EXPORT DeviceRegistry
{
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    /// Stop advertising a node that has not replied for this long. With a poll
    /// every 3 seconds this tolerates several lost packets without making a
    /// device flap.
    static constexpr std::chrono::seconds kDeviceTimeout{10};

    void update(const ArtPollReplyPacket& reply, const std::string& source_ip, TimePoint now = Clock::now());
    std::vector<DiscoveredDevice> snapshot(TimePoint now = Clock::now()) const;
    std::uint64_t generation() const;
    void clear();

  private:
    struct BindBlock {
        std::uint16_t num_ports{0};
        std::uint8_t net{0};
        std::uint8_t subnet{0};
        std::array<std::uint8_t, 4> port_types{};
        std::array<std::uint8_t, 4> swin{};
        std::array<std::uint8_t, 4> swout{};
        TimePoint last_seen{};
    };

    struct DeviceRecord {
        DiscoveredDevice device;
        std::map<int, BindBlock> blocks;
    };

    static std::size_t block_port_count(const BindBlock& block);
    static void refresh(DeviceRecord& record, TimePoint now);
    void prune_locked(TimePoint now) const;

    mutable std::mutex m_mutex;
    mutable std::map<std::string, DeviceRecord> m_devices;
    mutable std::uint64_t m_generation{0};
};

/// Discovery service shared by every flux.artnet.send object.
///
/// The service keeps one listener on the Art-Net port while at least one
/// sender wants discovery. It sends ArtPoll from that listener so that
/// ArtPollReply packets arrive on port 6454, and stores replies in the
/// registry. Max output is emitted by the external, never by the listener
/// thread.
class FLUX_ARTNET_EXPORT DiscoveryService
{
  public:
    DiscoveryService(SenderNetworkService& sender, ReceiverNetworkService& receiver, std::uint16_t port = kDefaultPort);
    ~DiscoveryService();

    DiscoveryService(const DiscoveryService&) = delete;
    DiscoveryService& operator=(const DiscoveryService&) = delete;

    /// Keep the discovery listener alive. Balanced by release().
    bool acquire(std::string& error);
    void release();

    /// Broadcast one ArtPoll. The listener must be acquired first.
    bool poll(std::string& error);

    /// Send one unicast ArtPoll to each address. Used by the connection probe.
    bool poll_hosts(const std::vector<std::string>& hosts, std::string& error);

    std::vector<DiscoveredDevice> devices() const;
    std::uint64_t generation() const;

    DeviceRegistry& registry()
    {
        return m_registry;
    }

    const DeviceRegistry& registry() const
    {
        return m_registry;
    }

  private:
    void handle_datagram(const std::uint8_t* data, std::size_t size, const sockaddr_in& source);

    SenderNetworkService& m_sender;
    ReceiverNetworkService& m_receiver;
    std::uint16_t m_port{kDefaultPort};
    DeviceRegistry m_registry;

    mutable std::mutex m_client_mutex;
    int m_clients{0};
    std::shared_ptr<ReceiverSubscription> m_subscription;
};

/// Resolve a hostname to its IPv4 addresses. IP literals are returned as-is.
std::vector<std::string> resolve_ipv4_addresses(const std::string& host);

// ---------------------------------------------------------------------------
// DeviceRegistry implementation
// ---------------------------------------------------------------------------

inline std::size_t DeviceRegistry::block_port_count(const BindBlock& block)
{
    if (block.num_ports == 0)
        return 4;
    return std::min<std::size_t>(block.num_ports, 4);
}

inline void DeviceRegistry::refresh(DeviceRecord& record, TimePoint now)
{
    for (auto block = record.blocks.begin(); block != record.blocks.end();) {
        if (now - block->second.last_seen > kDeviceTimeout)
            block = record.blocks.erase(block);
        else
            ++block;
    }

    bool any_output = false;
    bool any_input = false;
    for (const auto& entry : record.blocks) {
        const auto& block = entry.second;
        const auto count = block_port_count(block);
        for (std::size_t slot = 0; slot < count; ++slot) {
            any_output = any_output || (block.port_types[slot] & 0x80) != 0;
            any_input = any_input || (block.port_types[slot] & 0x40) != 0;
        }
    }

    std::vector<int> outputs;
    std::vector<int> inputs;
    for (const auto& entry : record.blocks) {
        const auto& block = entry.second;
        const auto count = block_port_count(block);
        for (std::size_t slot = 0; slot < count; ++slot) {
            const std::uint8_t type = block.port_types[slot];

            // Some older nodes leave PortTypes at zero. In that case treat the
            // configured universes as both inputs and outputs rather than
            // hiding the device's universes entirely.
            const bool is_output = (type & 0x80) != 0 || (!any_output && type == 0);
            const bool is_input = (type & 0x40) != 0 || (!any_input && type == 0);

            if (is_output) {
                const int universe = ((static_cast<int>(block.net) & 0x7F) << 8) | ((static_cast<int>(block.subnet) & 0x0F) << 4)
                                   | (static_cast<int>(block.swout[slot]) & 0x0F);
                outputs.push_back(universe);
            }
            if (is_input) {
                const int universe = ((static_cast<int>(block.net) & 0x7F) << 8) | ((static_cast<int>(block.subnet) & 0x0F) << 4)
                                   | (static_cast<int>(block.swin[slot]) & 0x0F);
                inputs.push_back(universe);
            }
        }
    }

    auto dedupe = [](std::vector<int>& values) {
        std::vector<int> unique;
        unique.reserve(values.size());
        for (const int value : values) {
            if (std::find(unique.begin(), unique.end(), value) == unique.end())
                unique.push_back(value);
        }
        values = std::move(unique);
    };
    dedupe(outputs);
    dedupe(inputs);

    record.device.output_universes = std::move(outputs);
    record.device.input_universes = std::move(inputs);

    if (record.device.num_ports <= 0) {
        std::size_t count = 0;
        for (const auto& entry : record.blocks)
            count += block_port_count(entry.second);
        record.device.num_ports = static_cast<int>(count);
    }
}

inline void DeviceRegistry::update(const ArtPollReplyPacket& reply, const std::string& source_ip, TimePoint now)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    prune_locked(now);

    std::string key = reply.ip;
    if (key.empty() || key == "0.0.0.0")
        key = source_ip;
    if (key.empty() || key == "0.0.0.0")
        key = reply.bind_ip;
    if (key.empty() || key == "0.0.0.0")
        return;

    const auto inserted = m_devices.find(key) == m_devices.end();
    DeviceRecord& record = m_devices[key];
    const std::string old_signature = inserted ? std::string{} : record.device.signature();

    const bool is_root = reply.bind_index <= 1;
    record.device.ip = key;
    record.device.source_ip = source_ip;
    record.device.last_seen = now;

    if (is_root || record.device.short_name.empty())
        record.device.short_name = reply.short_name;
    if (is_root || record.device.long_name.empty())
        record.device.long_name = reply.long_name;
    if (is_root || record.device.node_report.empty())
        record.device.node_report = reply.node_report;
    if (is_root)
        record.device.num_ports = static_cast<int>(reply.num_ports);
    if (is_root)
        record.device.firmware_version = static_cast<int>(reply.firmware_version);
    if (is_root)
        record.device.oem = static_cast<int>(reply.oem);
    if (is_root)
        record.device.bind_index = static_cast<int>(reply.bind_index);

    if (is_root || record.device.bind_ip.empty())
        record.device.bind_ip = reply.bind_ip;

    const std::string mac = reply.mac_string();
    if (!mac.empty() && (is_root || record.device.mac.empty()))
        record.device.mac = mac;

    BindBlock& block = record.blocks[reply.bind_index <= 1 ? 0 : static_cast<int>(reply.bind_index)];
    block.num_ports = reply.num_ports;
    block.net = reply.net;
    block.subnet = reply.subnet;
    block.port_types = reply.port_types;
    block.swin = reply.swin;
    block.swout = reply.swout;
    block.last_seen = now;

    refresh(record, now);

    if (inserted || old_signature != record.device.signature())
        ++m_generation;
}

inline void DeviceRegistry::prune_locked(TimePoint now) const
{
    bool removed = false;
    for (auto device = m_devices.begin(); device != m_devices.end();) {
        DeviceRecord& record = device->second;
        const std::string before = record.device.signature();
        refresh(record, now);
        if (record.device.signature() != before)
            removed = true;

        if (now - record.device.last_seen > kDeviceTimeout || record.blocks.empty()) {
            device = m_devices.erase(device);
            removed = true;
        }
        else {
            ++device;
        }
    }
    if (removed)
        ++m_generation;
}

inline std::vector<DiscoveredDevice> DeviceRegistry::snapshot(TimePoint now) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    prune_locked(now);

    std::vector<DiscoveredDevice> result;
    result.reserve(m_devices.size());
    for (const auto& entry : m_devices)
        result.push_back(entry.second.device);

    auto ip_key = [](const std::string& ip) {
        unsigned a = 0, b = 0, c = 0, d = 0;
        char extra = 0;
        if (std::sscanf(ip.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255)
            return (a << 24) | (b << 16) | (c << 8) | d;
        return 0xFFFFFFFFu;
    };

    std::sort(result.begin(), result.end(), [&](const DiscoveredDevice& lhs, const DiscoveredDevice& rhs) {
        const auto lhs_key = ip_key(lhs.ip);
        const auto rhs_key = ip_key(rhs.ip);
        if (lhs_key != rhs_key)
            return lhs_key < rhs_key;
        return lhs.display_name() < rhs.display_name();
    });
    return result;
}

inline std::uint64_t DeviceRegistry::generation() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    prune_locked(Clock::now());
    return m_generation;
}

inline void DeviceRegistry::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
    ++m_generation;
}

} // namespace flux::artnet
