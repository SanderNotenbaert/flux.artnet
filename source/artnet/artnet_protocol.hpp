/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "dmx_frame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace flux::artnet {

/// Art-Net Port-Addresses are 15 bits: 7 bits of Net, 4 bits of Sub-Net and 4 bits of Universe.
inline constexpr int kMaxPortAddress = 32767;
inline constexpr int kMaxNet = 127;
inline constexpr int kMaxSubNet = 15;
inline constexpr int kMaxUniverse = 15;

/// The Art-Net specification defines UDP port 6454 for all Art-Net traffic.
inline constexpr std::uint16_t kDefaultPort = 6454;

/// ArtDmx protocol version and opcode.
inline constexpr std::uint16_t kProtocolVersion = 14;
inline constexpr std::uint16_t kOpDmx = 0x5000;

/// ArtDmx header size and maximum packet size.
inline constexpr std::size_t kArtDmxHeaderSize = 18;
inline constexpr std::size_t kArtDmxMaxPacketSize = kArtDmxHeaderSize + kChannelCount;

/// The 8-byte Art-Net identifier, including its terminating zero byte.
inline constexpr std::array<std::uint8_t, 8> kArtNetId{'A', 'r', 't', '-', 'N', 'e', 't', 0};

struct PortAddressFields {
    std::uint8_t net{0};
    std::uint8_t subnet{0};
    std::uint8_t universe{0};
};

/// Validate a Port-Address.
inline bool valid_port_address(int port_address)
{
    return port_address >= 0 && port_address <= kMaxPortAddress;
}

/// Split a Port-Address into its Net, Sub-Net and Universe fields.
inline PortAddressFields split_port_address(int port_address)
{
    if (!valid_port_address(port_address))
        return {};

    PortAddressFields fields;
    fields.net = static_cast<std::uint8_t>((port_address >> 8) & 0x7F);
    fields.subnet = static_cast<std::uint8_t>((port_address >> 4) & 0x0F);
    fields.universe = static_cast<std::uint8_t>(port_address & 0x0F);
    return fields;
}

/// Build a Port-Address from Net, Sub-Net and Universe fields.
/// Returns -1 when any field is out of range.
inline int make_port_address(int net, int subnet, int universe)
{
    if (net < 0 || net > kMaxNet || subnet < 0 || subnet > kMaxSubNet || universe < 0 || universe > kMaxUniverse)
        return -1;
    return (net << 8) | (subnet << 4) | universe;
}

/// Decode the ArtDmx SubUni and Net bytes into a Port-Address.
/// Returns -1 if the reserved high bit of the Net field is set.
inline int decode_port_address(std::uint8_t subuni, std::uint8_t net_field)
{
    if ((net_field & 0x80) != 0)
        return -1;
    return (static_cast<int>(net_field) << 8) | static_cast<int>(subuni);
}

/// A decoded ArtDmx packet.
struct ArtDmxPacket {
    std::uint8_t sequence{0};
    std::uint8_t physical{0};
    int port_address{0};
    std::size_t length{0};
    DmxFrame data{};
};

enum class DecodeError {
    none,
    too_short,
    bad_identifier,
    bad_opcode,
    unsupported_version,
    bad_net,
    zero_length,
    too_long,
    odd_length,
    length_mismatch
};

inline const char* decode_error_string(DecodeError error)
{
    switch (error) {
        case DecodeError::none:
            return "no error";
        case DecodeError::too_short:
            return "packet is shorter than the ArtDmx header";
        case DecodeError::bad_identifier:
            return "packet does not start with the Art-Net identifier";
        case DecodeError::bad_opcode:
            return "packet is not an ArtDmx packet";
        case DecodeError::unsupported_version:
            return "unsupported Art-Net protocol version";
        case DecodeError::bad_net:
            return "ArtDmx Net field has its reserved high bit set";
        case DecodeError::zero_length:
            return "ArtDmx payload is empty";
        case DecodeError::too_long:
            return "ArtDmx payload is longer than 512 channels";
        case DecodeError::odd_length:
            return "ArtDmx payload length is not even";
        case DecodeError::length_mismatch:
            return "ArtDmx payload is shorter than the declared length";
    }
    return "unknown error";
}

/// Encode a complete ArtDmx packet.
///
/// The sender in this package always transmits full 512-channel frames. The
/// encoder still accepts shorter even lengths so that it can be used and tested
/// independently of that policy.
inline std::vector<std::uint8_t> encode_artdmx(int port_address, const std::uint8_t* channels, std::size_t length, std::uint8_t sequence)
{
    if (!valid_port_address(port_address) || channels == nullptr)
        return {};
    if (length == 0 || length > kChannelCount || (length % 2) != 0)
        return {};

    const auto fields = split_port_address(port_address);
    std::vector<std::uint8_t> packet(kArtDmxHeaderSize + length, 0);

    std::memcpy(packet.data(), kArtNetId.data(), kArtNetId.size());

    // OpCode is transmitted little-endian.
    packet[8] = static_cast<std::uint8_t>(kOpDmx & 0xFF);
    packet[9] = static_cast<std::uint8_t>((kOpDmx >> 8) & 0xFF);

    // ProtVer is transmitted big-endian.
    packet[10] = static_cast<std::uint8_t>((kProtocolVersion >> 8) & 0xFF);
    packet[11] = static_cast<std::uint8_t>(kProtocolVersion & 0xFF);

    packet[12] = sequence;
    packet[13] = 0; // Physical input port is not used.
    packet[14] = static_cast<std::uint8_t>((fields.subnet << 4) | fields.universe);
    packet[15] = fields.net;

    packet[16] = static_cast<std::uint8_t>((length >> 8) & 0xFF);
    packet[17] = static_cast<std::uint8_t>(length & 0xFF);

    std::memcpy(packet.data() + kArtDmxHeaderSize, channels, length);
    return packet;
}

/// Decode and validate an ArtDmx packet.
inline DecodeError decode_artdmx(const std::uint8_t* data, std::size_t size, ArtDmxPacket& packet)
{
    if (data == nullptr || size < kArtDmxHeaderSize)
        return DecodeError::too_short;

    if (std::memcmp(data, kArtNetId.data(), kArtNetId.size()) != 0)
        return DecodeError::bad_identifier;

    const std::uint16_t opcode = static_cast<std::uint16_t>(data[8]) | (static_cast<std::uint16_t>(data[9]) << 8);
    if (opcode != kOpDmx)
        return DecodeError::bad_opcode;

    const std::uint16_t version = (static_cast<std::uint16_t>(data[10]) << 8) | static_cast<std::uint16_t>(data[11]);
    if (version != kProtocolVersion)
        return DecodeError::unsupported_version;

    if ((data[15] & 0x80) != 0)
        return DecodeError::bad_net;

    const int port_address = decode_port_address(data[14], data[15]);
    if (port_address < 0)
        return DecodeError::bad_net;

    const std::size_t length = (static_cast<std::size_t>(data[16]) << 8) | static_cast<std::size_t>(data[17]);
    if (length == 0)
        return DecodeError::zero_length;
    if (length > kChannelCount)
        return DecodeError::too_long;
    if ((length % 2) != 0)
        return DecodeError::odd_length;
    if (kArtDmxHeaderSize + length > size)
        return DecodeError::length_mismatch;

    packet.sequence = data[12];
    packet.physical = data[13];
    packet.port_address = port_address;
    packet.length = length;
    packet.data.fill(0);
    std::memcpy(packet.data.data(), data + kArtDmxHeaderSize, length);
    return DecodeError::none;
}

/// Sequence handling according to the Art-Net specification.
///
/// Sequence 0 means that sequence numbering is disabled. Non-zero sequences
/// run from 1 to 255 and wrap back to 1. Receivers must not reject packets
/// aggressively, so this tracker classifies packets but the receiver always
/// processes them.
enum class SequenceStatus { disabled, first, in_order, out_of_order, duplicate };

class SequenceTracker
{
  public:
    SequenceStatus update(std::uint8_t sequence)
    {
        if (sequence == 0) {
            m_has_sequence = false;
            return SequenceStatus::disabled;
        }

        SequenceStatus status;
        if (!m_has_sequence)
            status = SequenceStatus::first;
        else if (sequence == m_expected)
            status = SequenceStatus::in_order;
        else if (sequence == m_last)
            status = SequenceStatus::duplicate;
        else
            status = SequenceStatus::out_of_order;

        m_has_sequence = true;
        m_last = sequence;
        m_expected = (sequence == 255) ? 1 : static_cast<std::uint8_t>(sequence + 1);
        return status;
    }

    void reset()
    {
        m_has_sequence = false;
        m_last = 0;
        m_expected = 1;
    }

    bool has_sequence() const
    {
        return m_has_sequence;
    }

  private:
    bool m_has_sequence{false};
    std::uint8_t m_last{0};
    std::uint8_t m_expected{1};
};

} // namespace flux::artnet
