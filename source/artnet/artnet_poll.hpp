/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "artnet_protocol.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace flux::artnet {

/// ArtPoll is at least 14 bytes; consumers must accept a longer packet and
/// treat missing fields as zero.
inline constexpr std::size_t kArtPollMinPacketSize = 14;

/// ArtPollReply is at least 207 bytes on the wire. Older nodes may omit the
/// Art-Net 3/4 fields that follow the MAC address; those are treated as zero.
inline constexpr std::size_t kArtPollReplyMinPacketSize = 207;

/// Bit 1 of ArtPoll::Flags asks a node to send an ArtPollReply whenever its
/// conditions change. This package polls instead, so the flag stays clear.
inline constexpr std::uint8_t kArtPollFlagSendOnChange = 0x02;

/// Bit 5 of ArtPoll::Flags enables targeted mode, which limits replies to
/// nodes that own a Port-Address inside the requested range.
inline constexpr std::uint8_t kArtPollFlagTargetedMode = 0x20;

/// A decoded ArtPoll.
struct ArtPollPacket {
    std::uint8_t flags{0};
    std::uint8_t diag_priority{0};
    int target_top{-1};
    int target_bottom{-1};

    bool targeted() const
    {
        return (flags & kArtPollFlagTargetedMode) != 0;
    }

    /// Returns true when a node owning this Port-Address should reply.
    bool matches(int port_address) const
    {
        if (!targeted() || target_top < 0 || target_bottom < 0 || target_top < target_bottom)
            return true;
        return port_address >= target_bottom && port_address <= target_top;
    }
};

/// A decoded ArtPollReply.
///
/// Port-Addresses are reconstructed from NetSwitch, SubSwitch and the low
/// nibble of SwIn/SwOut, exactly like @universes.
struct ArtPollReplyPacket {
    std::string ip;
    std::uint16_t port{0};
    std::uint16_t firmware_version{0};
    std::uint8_t net{0};
    std::uint8_t subnet{0};
    std::uint16_t oem{0};
    std::uint8_t ubea_version{0};
    std::uint8_t status1{0};
    std::uint16_t esta_manufacturer{0};
    std::string short_name;
    std::string long_name;
    std::string node_report;
    std::uint16_t num_ports{0};
    std::array<std::uint8_t, 4> port_types{};
    std::array<std::uint8_t, 4> good_input{};
    std::array<std::uint8_t, 4> good_output_a{};
    std::array<std::uint8_t, 4> swin{};
    std::array<std::uint8_t, 4> swout{};
    std::uint8_t acn_priority{0};
    std::uint8_t style{0};
    std::array<std::uint8_t, 6> mac{};
    std::string bind_ip;
    std::uint8_t bind_index{0};
    std::uint8_t status2{0};
    std::array<std::uint8_t, 4> good_output_b{};
    std::uint8_t status3{0};
    std::uint16_t refresh_rate{0};

    bool output_port(std::size_t slot) const
    {
        return slot < 4 && (port_types[slot] & 0x80) != 0;
    }

    bool input_port(std::size_t slot) const
    {
        return slot < 4 && (port_types[slot] & 0x40) != 0;
    }

    int output_universe(std::size_t slot) const
    {
        if (slot >= 4)
            return -1;
        return ((static_cast<int>(net) & 0x7F) << 8) | ((static_cast<int>(subnet) & 0x0F) << 4)
             | (static_cast<int>(swout[slot]) & 0x0F);
    }

    int input_universe(std::size_t slot) const
    {
        if (slot >= 4)
            return -1;
        return ((static_cast<int>(net) & 0x7F) << 8) | ((static_cast<int>(subnet) & 0x0F) << 4)
             | (static_cast<int>(swin[slot]) & 0x0F);
    }

    /// Format the MAC address as lower-case colon-separated hex.
    /// Returns an empty string when the node did not report a MAC.
    std::string mac_string() const
    {
        const bool any = std::any_of(mac.begin(), mac.end(), [](std::uint8_t value) { return value != 0; });
        if (!any)
            return {};

        static constexpr char digits[] = "0123456789abcdef";
        std::string text;
        text.reserve(17);
        for (std::size_t i = 0; i < mac.size(); ++i) {
            if (i > 0)
                text.push_back(':');
            text.push_back(digits[(mac[i] >> 4) & 0x0F]);
            text.push_back(digits[mac[i] & 0x0F]);
        }
        return text;
    }
};

/// Build the minimal ArtPoll packet accepted by every Art-Net node.
inline std::vector<std::uint8_t> encode_artpoll(std::uint8_t flags = 0)
{
    std::vector<std::uint8_t> packet(kArtPollMinPacketSize, 0);
    std::memcpy(packet.data(), kArtNetId.data(), kArtNetId.size());
    packet[8] = static_cast<std::uint8_t>(kOpPoll & 0xFF);
    packet[9] = static_cast<std::uint8_t>((kOpPoll >> 8) & 0xFF);
    packet[10] = static_cast<std::uint8_t>((kProtocolVersion >> 8) & 0xFF);
    packet[11] = static_cast<std::uint8_t>(kProtocolVersion & 0xFF);
    packet[12] = flags;
    packet[13] = 0; // DiagPriority: diagnostics are not requested.
    return packet;
}

/// Decode the parts of ArtPoll that affect whether a receiver should reply.
inline bool decode_artpoll(const std::uint8_t* data, std::size_t size, ArtPollPacket& packet)
{
    if (data == nullptr || size < kArtPollMinPacketSize)
        return false;

    if (std::memcmp(data, kArtNetId.data(), kArtNetId.size()) != 0)
        return false;

    const std::uint16_t opcode = static_cast<std::uint16_t>(data[8]) | (static_cast<std::uint16_t>(data[9]) << 8);
    if (opcode != kOpPoll)
        return false;

    packet = ArtPollPacket{};
    packet.flags = data[12];
    packet.diag_priority = data[13];

    if (size >= 18) {
        packet.target_top = (static_cast<int>(data[14]) << 8) | static_cast<int>(data[15]);
        packet.target_bottom = (static_cast<int>(data[16]) << 8) | static_cast<int>(data[17]);
    }
    return true;
}

inline std::string format_ipv4(const std::uint8_t* bytes)
{
    if (bytes == nullptr)
        return {};
    return std::to_string(bytes[0]) + "." + std::to_string(bytes[1]) + "." + std::to_string(bytes[2]) + "." + std::to_string(bytes[3]);
}

/// Read a fixed-length, null-terminated Art-Net string.
inline std::string decode_fixed_string(const std::uint8_t* data, std::size_t length)
{
    if (data == nullptr)
        return {};

    std::size_t count = 0;
    while (count < length && data[count] != 0)
        ++count;

    std::string text;
    text.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t value = data[i];
        if (value >= 0x20 && value != 0x7F)
            text.push_back(static_cast<char>(value));
    }

    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.erase(text.begin());
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
        text.pop_back();
    return text;
}

/// Decode and validate an ArtPollReply.
///
/// The spec requires consumers to accept packets of at least 207 bytes. Bytes
/// beyond the received packet are zero-filled, so BindIp, BindIndex and the
/// newer Art-Net 4 fields are optional.
inline bool decode_artpollreply(const std::uint8_t* data, std::size_t size, ArtPollReplyPacket& packet)
{
    if (data == nullptr || size < kArtPollReplyMinPacketSize)
        return false;

    if (std::memcmp(data, kArtNetId.data(), kArtNetId.size()) != 0)
        return false;

    const std::uint16_t opcode = static_cast<std::uint16_t>(data[8]) | (static_cast<std::uint16_t>(data[9]) << 8);
    if (opcode != kOpPollReply)
        return false;

    packet = ArtPollReplyPacket{};

    packet.ip = format_ipv4(data + 10);
    packet.port = static_cast<std::uint16_t>(data[14]) | (static_cast<std::uint16_t>(data[15]) << 8);
    packet.firmware_version = static_cast<std::uint16_t>(data[16] << 8) | static_cast<std::uint16_t>(data[17]);
    packet.net = data[18];
    packet.subnet = data[19];
    packet.oem = static_cast<std::uint16_t>(data[20] << 8) | static_cast<std::uint16_t>(data[21]);
    packet.ubea_version = data[22];
    packet.status1 = data[23];
    packet.esta_manufacturer = static_cast<std::uint16_t>(data[24]) | (static_cast<std::uint16_t>(data[25]) << 8);
    packet.short_name = decode_fixed_string(data + 26, 18);
    packet.long_name = decode_fixed_string(data + 44, 64);
    packet.node_report = decode_fixed_string(data + 108, 64);
    packet.num_ports = static_cast<std::uint16_t>(data[172] << 8) | static_cast<std::uint16_t>(data[173]);

    for (std::size_t i = 0; i < 4; ++i) {
        packet.port_types[i] = data[174 + i];
        packet.good_input[i] = data[178 + i];
        packet.good_output_a[i] = data[182 + i];
        packet.swin[i] = data[186 + i];
        packet.swout[i] = data[190 + i];
    }

    packet.acn_priority = data[194];
    packet.style = data[200];

    for (std::size_t i = 0; i < packet.mac.size(); ++i)
        packet.mac[i] = data[201 + i];

    if (size >= 212) {
        packet.bind_ip = format_ipv4(data + 207);
        packet.bind_index = data[211];
    }

    if (size >= 213)
        packet.status2 = data[212];
    if (size >= 217) {
        for (std::size_t i = 0; i < 4; ++i)
            packet.good_output_b[i] = data[213 + i];
    }
    if (size >= 218)
        packet.status3 = data[217];
    if (size >= 228)
        packet.refresh_rate = static_cast<std::uint16_t>(data[226] << 8) | static_cast<std::uint16_t>(data[227]);

    return true;
}

/// Parse a dotted IPv4 string into four bytes.
inline bool parse_ipv4(const std::string& text, std::array<std::uint8_t, 4>& bytes)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    char extra = 0;
    if (std::sscanf(text.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4)
        return false;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return false;

    bytes = {static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(c), static_cast<std::uint8_t>(d)};
    return true;
}

/// Write a null-terminated fixed-length Art-Net string.
inline void encode_fixed_string(std::uint8_t* destination, std::size_t length, const std::string& text)
{
    if (destination == nullptr || length == 0)
        return;

    std::memset(destination, 0, length);
    const std::size_t count = std::min(text.size(), length - 1);
    std::memcpy(destination, text.data(), count);
}

/// Encode a complete Art-Net 4 ArtPollReply (239 bytes).
inline std::vector<std::uint8_t> encode_artpollreply(const ArtPollReplyPacket& packet)
{
    std::vector<std::uint8_t> data(239, 0);

    std::memcpy(data.data(), kArtNetId.data(), kArtNetId.size());
    data[8] = static_cast<std::uint8_t>(kOpPollReply & 0xFF);
    data[9] = static_cast<std::uint8_t>((kOpPollReply >> 8) & 0xFF);

    std::array<std::uint8_t, 4> ip{};
    if (parse_ipv4(packet.ip, ip))
        std::memcpy(data.data() + 10, ip.data(), ip.size());

    const std::uint16_t port = packet.port != 0 ? packet.port : kDefaultPort;
    data[14] = static_cast<std::uint8_t>(port & 0xFF);
    data[15] = static_cast<std::uint8_t>((port >> 8) & 0xFF);
    data[16] = static_cast<std::uint8_t>((packet.firmware_version >> 8) & 0xFF);
    data[17] = static_cast<std::uint8_t>(packet.firmware_version & 0xFF);
    data[18] = packet.net;
    data[19] = packet.subnet;
    data[20] = static_cast<std::uint8_t>((packet.oem >> 8) & 0xFF);
    data[21] = static_cast<std::uint8_t>(packet.oem & 0xFF);
    data[22] = packet.ubea_version;
    data[23] = packet.status1;
    data[24] = static_cast<std::uint8_t>(packet.esta_manufacturer & 0xFF);
    data[25] = static_cast<std::uint8_t>((packet.esta_manufacturer >> 8) & 0xFF);

    encode_fixed_string(data.data() + 26, 18, packet.short_name);
    encode_fixed_string(data.data() + 44, 64, packet.long_name);
    encode_fixed_string(data.data() + 108, 64, packet.node_report);

    data[172] = static_cast<std::uint8_t>((packet.num_ports >> 8) & 0xFF);
    data[173] = static_cast<std::uint8_t>(packet.num_ports & 0xFF);

    for (std::size_t i = 0; i < 4; ++i) {
        data[174 + i] = packet.port_types[i];
        data[178 + i] = packet.good_input[i];
        data[182 + i] = packet.good_output_a[i];
        data[186 + i] = packet.swin[i];
        data[190 + i] = packet.swout[i];
    }

    data[194] = packet.acn_priority;
    data[200] = packet.style;

    for (std::size_t i = 0; i < packet.mac.size(); ++i)
        data[201 + i] = packet.mac[i];

    std::array<std::uint8_t, 4> bind{};
    if (parse_ipv4(packet.bind_ip, bind))
        std::memcpy(data.data() + 207, bind.data(), bind.size());

    data[211] = packet.bind_index;
    data[212] = packet.status2;

    for (std::size_t i = 0; i < 4; ++i)
        data[213 + i] = packet.good_output_b[i];

    data[217] = packet.status3;
    data[226] = static_cast<std::uint8_t>((packet.refresh_rate >> 8) & 0xFF);
    data[227] = static_cast<std::uint8_t>(packet.refresh_rate & 0xFF);

    return data;
}

} // namespace flux::artnet
