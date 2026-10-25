/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min_unittest.h"          // required unit test header
#include "flux.artnet.send.cpp"        // need the source of our object so that we can access it

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace c74::min;
using namespace flux::artnet;

struct artnet_send_test_access {
    static SenderEngine& engine(artnet_send& object)
    {
        return object.m_engine;
    }

    static void output_devices(artnet_send& object)
    {
        object.output_devices();
    }

    static void output_info(artnet_send& object)
    {
        object.output_info();
    }

    static void emit_connected(artnet_send& object, const DiscoveredDevice& device)
    {
        object.emit_connected(device);
    }

    static bool probe_active(const artnet_send& object)
    {
        return object.m_probe_active;
    }

    static std::uint64_t probe_serial(const artnet_send& object)
    {
        return object.m_probe_serial;
    }

    static void on_probe_timer(artnet_send& object)
    {
        object.on_probe_timer();
    }

    static const DiscoveredDevice* probe_match(const std::vector<DiscoveredDevice>& devices,
                                               const std::vector<std::string>& targets,
                                               std::chrono::steady_clock::time_point start)
    {
        return artnet_send::find_probe_match(devices, targets, start);
    }
};

namespace {

struct PacketCapture {
    std::vector<std::vector<std::uint8_t>> packets;

    artnet_send::packet_sender_type function()
    {
        return [this](const std::vector<std::uint8_t>& packet, std::string&) {
            packets.push_back(packet);
            return true;
        };
    }
};

ArtDmxPacket decode_packet(const std::vector<std::uint8_t>& bytes)
{
    ArtDmxPacket packet;
    decode_artdmx(bytes.data(), bytes.size(), packet);
    return packet;
}

std::vector<std::uint8_t> make_poll_reply(const std::string& ip,
                                          const std::string& long_name,
                                          const std::string& short_name,
                                          const std::array<std::uint8_t, 6>& mac,
                                          std::uint8_t bind_index,
                                          std::uint8_t net,
                                          std::uint8_t subnet,
                                          std::uint16_t num_ports,
                                          const std::array<std::uint8_t, 4>& port_types,
                                          const std::array<std::uint8_t, 4>& swout,
                                          const std::array<std::uint8_t, 4>& swin = {})
{
    std::vector<std::uint8_t> data(239, 0);
    std::memcpy(data.data(), "Art-Net\0", 8);
    data[8] = 0x00;
    data[9] = 0x21;

    unsigned a = 0, b = 0, c = 0, d = 0;
    std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d);
    data[10] = static_cast<std::uint8_t>(a);
    data[11] = static_cast<std::uint8_t>(b);
    data[12] = static_cast<std::uint8_t>(c);
    data[13] = static_cast<std::uint8_t>(d);

    data[14] = 0x36;
    data[15] = 0x19; // Port 0x1936, little-endian
    data[16] = 0x01;
    data[17] = 0x02;
    data[18] = net;
    data[19] = subnet;
    data[20] = 0x00;
    data[21] = 0x10;
    data[23] = 0xC0;

    std::memcpy(data.data() + 26, short_name.data(), std::min<std::size_t>(short_name.size(), 17));
    std::memcpy(data.data() + 44, long_name.data(), std::min<std::size_t>(long_name.size(), 63));

    data[172] = static_cast<std::uint8_t>((num_ports >> 8) & 0xFF);
    data[173] = static_cast<std::uint8_t>(num_ports & 0xFF);
    for (std::size_t i = 0; i < 4; ++i) {
        data[174 + i] = port_types[i];
        data[186 + i] = swin[i];
        data[190 + i] = swout[i];
    }

    for (std::size_t i = 0; i < mac.size(); ++i)
        data[201 + i] = mac[i];

    data[207] = static_cast<std::uint8_t>(a);
    data[208] = static_cast<std::uint8_t>(b);
    data[209] = static_cast<std::uint8_t>(c);
    data[210] = static_cast<std::uint8_t>(d);
    data[211] = bind_index;
    return data;
}

ArtPollReplyPacket decode_reply(const std::vector<std::uint8_t>& data)
{
    ArtPollReplyPacket packet;
    REQUIRE(decode_artpollreply(data.data(), data.size(), packet));
    return packet;
}

std::vector<std::string> output_values(void* object, int outlet, std::size_t message_index)
{
    std::vector<std::string> result;
    auto* output = c74::max::object_getoutput(object, outlet);
    if (output == nullptr || output->size() <= message_index)
        return result;

    for (const auto& value : (*output)[message_index]) {
        switch (value.type()) {
            case message_type::symbol_argument:
                result.push_back(static_cast<std::string>(symbol(static_cast<c74::max::t_symbol*>(value))));
                break;
            case message_type::int_argument:
                result.push_back(std::to_string(static_cast<int>(value)));
                break;
            case message_type::float_argument:
                result.push_back(std::to_string(static_cast<double>(value)));
                break;
            default:
                result.push_back("<other>");
                break;
        }
    }
    return result;
}

std::size_t output_message_count(void* object, int outlet)
{
    auto* output = c74::max::object_getoutput(object, outlet);
    return output == nullptr ? 0 : output->size();
}

std::uint16_t find_free_udp_port()
{
    const int descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (descriptor < 0)
        return 0;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (::bind(descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(descriptor);
        return 0;
    }

    socklen_t length = sizeof(address);
    if (::getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        ::close(descriptor);
        return 0;
    }

    const std::uint16_t port = ntohs(address.sin_port);
    ::close(descriptor);
    return port;
}

template <class Predicate>
bool wait_for(Predicate predicate, int timeout_ms)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

} // namespace


SCENARIO("Port-Address helpers split and rebuild every boundary") {
    THEN("the integer list parser accepts atoms and a single text symbol") {
        std::vector<int> parsed;
        std::string error;
        REQUIRE(parse_int_list(atoms{1, 2, 4}, parsed, error));
        REQUIRE(parsed == std::vector<int>{1, 2, 4});

        REQUIRE(parse_int_list(atoms{symbol("1, 2 4")}, parsed, error));
        REQUIRE(parsed == std::vector<int>{1, 2, 4});

        REQUIRE_FALSE(parse_int_list(atoms{symbol("1 nope 4")}, parsed, error));
    }

    THEN("zero maps to Net 0, Sub-Net 0, Universe 0") {
        const auto fields = split_port_address(0);
        REQUIRE(fields.net == 0);
        REQUIRE(fields.subnet == 0);
        REQUIRE(fields.universe == 0);
        REQUIRE(make_port_address(0, 0, 0) == 0);
    }

    THEN("the Universe and Sub-Net boundaries are decoded correctly") {
        const auto fifteen = split_port_address(15);
        REQUIRE(fifteen.subnet == 0);
        REQUIRE(fifteen.universe == 15);

        const auto sixteen = split_port_address(16);
        REQUIRE(sixteen.subnet == 1);
        REQUIRE(sixteen.universe == 0);

        const auto two_fifty_five = split_port_address(255);
        REQUIRE(two_fifty_five.net == 0);
        REQUIRE(two_fifty_five.subnet == 15);
        REQUIRE(two_fifty_five.universe == 15);

        const auto two_fifty_six = split_port_address(256);
        REQUIRE(two_fifty_six.net == 1);
        REQUIRE(two_fifty_six.subnet == 0);
        REQUIRE(two_fifty_six.universe == 0);
    }

    THEN("the maximum Port-Address is 32767") {
        REQUIRE(valid_port_address(32767));
        REQUIRE_FALSE(valid_port_address(32768));
        REQUIRE_FALSE(valid_port_address(-1));

        const auto fields = split_port_address(32767);
        REQUIRE(fields.net == 127);
        REQUIRE(fields.subnet == 15);
        REQUIRE(fields.universe == 15);
        REQUIRE(make_port_address(127, 15, 15) == 32767);
        REQUIRE(make_port_address(128, 0, 0) == -1);
    }

    THEN("the packet encoding uses Art-Net byte order") {
        REQUIRE(decode_port_address(0x2A, 0x01) == 0x12A);
        REQUIRE(decode_port_address(0xFF, 0x7F) == 32767);
        REQUIRE(decode_port_address(0x00, 0x80) == -1);
    }
}


SCENARIO("ArtDmx packets round-trip and reject malformed input") {
    GIVEN("a full 512-channel packet") {
        DmxFrame channels{};
        channels[0] = 1;
        channels[1] = 2;
        channels[510] = 254;
        channels[511] = 255;

        const auto encoded = encode_artdmx(0x1234, channels.data(), channels.size(), 42);
        REQUIRE(encoded.size() == kArtDmxMaxPacketSize);

        ArtDmxPacket decoded;
        REQUIRE(decode_artdmx(encoded.data(), encoded.size(), decoded) == DecodeError::none);
        THEN("all header fields and channel data are preserved") {
            REQUIRE(decoded.sequence == 42);
            REQUIRE(decoded.port_address == 0x1234);
            REQUIRE(decoded.length == 512);
            REQUIRE(decoded.data == channels);
        }
    }

    GIVEN("a short, even-length packet") {
        const std::uint8_t values[4] = {10, 20, 30, 40};
        const auto encoded = encode_artdmx(7, values, 4, 0);
        REQUIRE(encoded.size() == kArtDmxHeaderSize + 4);

        ArtDmxPacket decoded;
        REQUIRE(decode_artdmx(encoded.data(), encoded.size(), decoded) == DecodeError::none);
        THEN("the payload length is preserved and unused channels are zero") {
            REQUIRE(decoded.length == 4);
            REQUIRE(decoded.data[0] == 10);
            REQUIRE(decoded.data[3] == 40);
            REQUIRE(decoded.data[4] == 0);
            REQUIRE(decoded.data[511] == 0);
        }
    }

    GIVEN("a manually assembled packet that does not use the encoder") {
        // Port-Address 0x12 (18): Net 0, Sub-Net 1, Universe 2, four channels.
        const std::uint8_t raw[] = {'A', 'r', 't', '-', 'N', 'e', 't', 0,
                                    0x00, 0x50,        // OpCode 0x5000, little-endian
                                    0x00, 0x0E,        // ProtVer 14, big-endian
                                    0x07,              // sequence
                                    0x00,              // physical
                                    0x12,              // SubUni: Sub-Net 1, Universe 2
                                    0x00,              // Net 0
                                    0x00, 0x04,        // length 4
                                    1, 2, 3, 4};

        ArtDmxPacket decoded;
        REQUIRE(decode_artdmx(raw, sizeof(raw), decoded) == DecodeError::none);
        THEN("the decoder matches the Art-Net wire format") {
            REQUIRE(decoded.sequence == 7);
            REQUIRE(decoded.port_address == 0x12);
            REQUIRE(decoded.length == 4);
            REQUIRE(decoded.data[0] == 1);
            REQUIRE(decoded.data[3] == 4);
        }
    }

    GIVEN("a packet encoded from known values") {
        const std::uint8_t values[4] = {11, 22, 33, 44};
        const auto encoded = encode_artdmx(0x12, values, 4, 7);

        THEN("the fixed header fields and byte order are correct") {
            REQUIRE(encoded.size() == 22);
            REQUIRE(std::memcmp(encoded.data(), "Art-Net\0", 8) == 0);
            REQUIRE(encoded[8] == 0x00);
            REQUIRE(encoded[9] == 0x50);
            REQUIRE(encoded[10] == 0x00);
            REQUIRE(encoded[11] == 0x0E);
            REQUIRE(encoded[12] == 7);
            REQUIRE(encoded[13] == 0);
            REQUIRE(encoded[14] == 0x12);
            REQUIRE(encoded[15] == 0x00);
            REQUIRE(encoded[16] == 0x00);
            REQUIRE(encoded[17] == 0x04);
            REQUIRE(encoded[18] == 11);
            REQUIRE(encoded[21] == 44);
        }
    }

    GIVEN("invalid packets") {
        const std::uint8_t values[4] = {1, 2, 3, 4};
        auto encoded = encode_artdmx(1, values, 4, 1);

        ArtDmxPacket decoded;

        THEN("a packet shorter than the header is rejected") {
            REQUIRE(decode_artdmx(encoded.data(), 17, decoded) == DecodeError::too_short);
        }

        THEN("a wrong identifier is rejected") {
            auto broken = encoded;
            broken[0] = 'X';
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::bad_identifier);
        }

        THEN("a wrong opcode is rejected") {
            auto broken = encoded;
            broken[8] = 0x01;
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::bad_opcode);
        }

        THEN("an unsupported protocol version is rejected") {
            auto broken = encoded;
            broken[11] = 13;
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::unsupported_version);
        }

        THEN("a reserved Net high bit is rejected") {
            auto broken = encoded;
            broken[15] = 0x80;
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::bad_net);
        }

        THEN("a zero payload length is rejected") {
            auto broken = encoded;
            broken[16] = 0;
            broken[17] = 0;
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::zero_length);
        }

        THEN("an odd payload length is rejected") {
            auto broken = encoded;
            broken[17] = 3;
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::odd_length);
        }

        THEN("a payload longer than 512 channels is rejected") {
            auto broken = encoded;
            broken[16] = 0x02;
            broken[17] = 0x02;
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::too_long);
        }

        THEN("a payload shorter than its declared length is rejected") {
            auto broken = encoded;
            broken[17] = 6;
            REQUIRE(decode_artdmx(broken.data(), broken.size(), decoded) == DecodeError::length_mismatch);
        }
    }

    THEN("the encoder rejects odd, empty and too-long lengths") {
        const std::uint8_t values[512] = {};
        REQUIRE(encode_artdmx(0, values, 0, 1).empty());
        REQUIRE(encode_artdmx(0, values, 1, 1).empty());
        REQUIRE(encode_artdmx(0, values, 514, 1).empty());
        REQUIRE(encode_artdmx(32768, values, 2, 1).empty());
    }
}


SCENARIO("Sequence tracking follows the Art-Net numbering rules") {
    SequenceTracker tracker;

    REQUIRE(tracker.update(1) == SequenceStatus::first);
    REQUIRE(tracker.update(2) == SequenceStatus::in_order);
    REQUIRE(tracker.update(2) == SequenceStatus::duplicate);
    REQUIRE(tracker.update(4) == SequenceStatus::out_of_order);
    REQUIRE(tracker.update(255) == SequenceStatus::out_of_order);
    REQUIRE(tracker.update(1) == SequenceStatus::in_order);
    REQUIRE(tracker.update(0) == SequenceStatus::disabled);
    REQUIRE(tracker.update(9) == SequenceStatus::first);
}


SCENARIO("SenderEngine applies updates atomically and transmits full frames") {
    GIVEN("an engine configured with one universe") {
        PacketCapture capture;
        SenderEngine engine{capture.function()};
        std::string error;

        REQUIRE(engine.set_universes({1}, error));

        WHEN("a sequential list is applied in change-triggered mode") {
            UpdateRequest request;
            request.values = {255, 128, 0};
            REQUIRE(engine.apply(request, false, true, error));

            THEN("one full packet is transmitted") {
                REQUIRE(capture.packets.size() == 1);
                const auto packet = decode_packet(capture.packets[0]);
                REQUIRE(packet.port_address == 1);
                REQUIRE(packet.length == 512);
                REQUIRE(packet.data[0] == 255);
                REQUIRE(packet.data[1] == 128);
                REQUIRE(packet.data[2] == 0);
                REQUIRE(packet.sequence == 1);
            }
        }

        WHEN("an identical update is applied a second time") {
            UpdateRequest request;
            request.values = {42};
            REQUIRE(engine.apply(request, false, true, error));
            REQUIRE(engine.apply(request, false, true, error));

            THEN("the redundant transmission is suppressed") {
                REQUIRE(capture.packets.size() == 1);
            }
        }

        WHEN("a set update is applied") {
            UpdateRequest request;
            request.values = {99};
            REQUIRE(engine.apply(request, true, true, error));

            THEN("nothing is transmitted immediately") {
                REQUIRE(capture.packets.empty());
            }

            THEN("bang sends the updated full frame") {
                REQUIRE(engine.send_all(error));
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 99);
            }
        }

        WHEN("an update contains an invalid channel value") {
            UpdateRequest request;
            request.values = {10, 300};
            REQUIRE_FALSE(engine.apply(request, false, true, error));

            THEN("nothing is transmitted or stored") {
                REQUIRE(capture.packets.empty());
                REQUIRE(engine.state().frame(0)[0] == 0);
                REQUIRE(engine.state().frame(0)[1] == 0);
            }
        }

        WHEN("an update at offset 511 overflows one universe") {
            UpdateRequest request;
            request.channel_offset = 511;
            request.values = {1, 2};
            std::string warning;
            REQUIRE(engine.apply(request, false, true, error, &warning));

            THEN("the update is truncated, applied and transmitted") {
                REQUIRE_FALSE(warning.empty());
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[511] == 1);
                REQUIRE(engine.state().frame(0)[511] == 1);
            }
        }
    }

    GIVEN("non-contiguous universes and an update at offset 510") {
        PacketCapture capture;
        SenderEngine engine{capture.function()};
        std::string error;
        REQUIRE(engine.set_universes({1, 2, 4}, error));

        UpdateRequest request;
        request.channel_offset = 510;
        request.values = {255, 128, 64, 32};
        REQUIRE(engine.apply(request, false, true, error));

        THEN("the write spills into the next configured universe in list order") {
            REQUIRE(capture.packets.size() == 2);
            const auto first = decode_packet(capture.packets[0]);
            const auto second = decode_packet(capture.packets[1]);
            REQUIRE(first.port_address == 1);
            REQUIRE(first.data[510] == 255);
            REQUIRE(first.data[511] == 128);
            REQUIRE(second.port_address == 2);
            REQUIRE(second.data[0] == 64);
            REQUIRE(second.data[1] == 32);
        }

        THEN("the last configured universe is not touched by this update") {
            REQUIRE(engine.state().frame(2)[0] == 0);
        }
    }

    GIVEN("an engine whose universes are reconfigured") {
        PacketCapture capture;
        SenderEngine engine{capture.function()};
        std::string error;
        REQUIRE(engine.set_universes({1, 2}, error));

        UpdateRequest request;
        request.values = {77};
        REQUIRE(engine.apply(request, true, false, error));

        REQUIRE(engine.set_universes({2, 1}, error));
        REQUIRE(engine.send_all(error));

        THEN("buffers remain attached to their Port-Address and order follows the new list") {
            REQUIRE(capture.packets.size() == 2);
            REQUIRE(decode_packet(capture.packets[0]).port_address == 2);
            REQUIRE(decode_packet(capture.packets[0]).data[0] == 0);
            REQUIRE(decode_packet(capture.packets[1]).port_address == 1);
            REQUIRE(decode_packet(capture.packets[1]).data[0] == 77);
        }
    }

    GIVEN("a silent update in one universe followed by an ordinary update in another") {
        PacketCapture capture;
        SenderEngine engine{capture.function()};
        std::string error;
        REQUIRE(engine.set_universes({1, 2}, error));

        UpdateRequest silent_request;
        silent_request.universe_index = 1;
        silent_request.values = {55};
        REQUIRE(engine.apply(silent_request, true, true, error));
        REQUIRE(capture.packets.empty());

        UpdateRequest ordinary_request;
        ordinary_request.values = {66};
        REQUIRE(engine.apply(ordinary_request, false, true, error));

        THEN("the pending silent frame is flushed with the ordinary update") {
            REQUIRE(capture.packets.size() == 2);
            REQUIRE(decode_packet(capture.packets[0]).port_address == 1);
            REQUIRE(decode_packet(capture.packets[0]).data[0] == 66);
            REQUIRE(decode_packet(capture.packets[1]).port_address == 2);
            REQUIRE(decode_packet(capture.packets[1]).data[0] == 55);
        }
    }

    GIVEN("an engine sending repeatedly") {
        PacketCapture capture;
        SenderEngine engine{capture.function()};
        std::string error;
        REQUIRE(engine.set_universes({1}, error));

        REQUIRE(engine.send_all(error));
        REQUIRE(engine.send_all(error));

        THEN("per-universe sequence numbers increment and stay in the 1-255 range") {
            REQUIRE(capture.packets.size() == 2);
            REQUIRE(decode_packet(capture.packets[0]).sequence == 1);
            REQUIRE(decode_packet(capture.packets[1]).sequence == 2);
        }
    }

    GIVEN("an invalid universe list") {
        PacketCapture capture;
        SenderEngine engine{capture.function()};
        std::string error;

        THEN("empty, duplicate and out-of-range lists are rejected") {
            REQUIRE_FALSE(engine.set_universes({}, error));
            REQUIRE_FALSE(engine.set_universes({1, 1}, error));
            REQUIRE_FALSE(engine.set_universes({32768}, error));
            REQUIRE(engine.state().universes() == std::vector<int>{0});
        }
    }
}


SCENARIO("ArtPoll and ArtPollReply packets use the Art-Net wire format") {
    THEN("the ArtPoll encoder produces the 14-byte minimum packet") {
        const auto packet = encode_artpoll();
        REQUIRE(packet.size() == 14);
        REQUIRE(std::memcmp(packet.data(), "Art-Net\0", 8) == 0);
        REQUIRE(packet[8] == 0x00);
        REQUIRE(packet[9] == 0x20);
        REQUIRE(packet[10] == 0x00);
        REQUIRE(packet[11] == 0x0E);
        REQUIRE(packet[12] == 0);
    }

    THEN("ArtPoll decodes flags and targeted Port-Address ranges") {
        auto packet = encode_artpoll();
        ArtPollPacket poll;
        REQUIRE(decode_artpoll(packet.data(), packet.size(), poll));
        REQUIRE_FALSE(poll.targeted());
        REQUIRE(poll.matches(0));
        REQUIRE(poll.matches(32767));

        packet.resize(18, 0);
        packet[12] = kArtPollFlagTargetedMode;
        packet[14] = 0x00;
        packet[15] = 0x10; // top = 16
        packet[16] = 0x00;
        packet[17] = 0x02; // bottom = 2
        REQUIRE(decode_artpoll(packet.data(), packet.size(), poll));
        REQUIRE(poll.targeted());
        REQUIRE(poll.matches(2));
        REQUIRE(poll.matches(16));
        REQUIRE_FALSE(poll.matches(1));
        REQUIRE_FALSE(poll.matches(17));
    }

    THEN("ArtPollReply encoding round-trips through the decoder") {
        ArtPollReplyPacket reply;
        reply.ip = "192.168.1.40";
        reply.port = kDefaultPort;
        reply.firmware_version = 0x0300;
        reply.net = 0;
        reply.subnet = 1;
        reply.oem = 0xFFFF;
        reply.status1 = 0xC0;
        reply.esta_manufacturer = 0x7FF0;
        reply.short_name = "flux.artnet";
        reply.long_name = "flux.artnet.receive";
        reply.node_report = "ready";
        reply.num_ports = 1;
        reply.port_types[0] = 0x80;
        reply.swout[0] = 2;
        reply.style = 0x06;
        reply.mac = {0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e};
        reply.bind_ip = "192.168.1.40";
        reply.bind_index = 1;
        reply.status2 = 0x08;

        const auto encoded = encode_artpollreply(reply);
        REQUIRE(encoded.size() == 239);

        ArtPollReplyPacket decoded;
        REQUIRE(decode_artpollreply(encoded.data(), encoded.size(), decoded));
        REQUIRE(decoded.ip == "192.168.1.40");
        REQUIRE(decoded.short_name == "flux.artnet");
        REQUIRE(decoded.long_name == "flux.artnet.receive");
        REQUIRE(decoded.num_ports == 1);
        REQUIRE(decoded.port_types[0] == 0x80);
        REQUIRE(decoded.output_universe(0) == 18);
        REQUIRE(decoded.style == 0x06);
        REQUIRE(decoded.bind_ip == "192.168.1.40");
        REQUIRE(decoded.bind_index == 1);
        REQUIRE(decoded.mac_string() == "00:1a:2b:3c:4d:5e");
    }

    GIVEN("a synthetic ArtPollReply") {
        const auto data = make_poll_reply("192.168.1.40",
                                          "Stage Left Dimmer",
                                          "StageLeft",
                                          {0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e},
                                          1,
                                          0,
                                          1,
                                          1,
                                          {0x80, 0, 0, 0},
                                          {2, 0, 0, 0});

        THEN("the decoder reads names, identity and universes") {
            const auto reply = decode_reply(data);
            REQUIRE(reply.ip == "192.168.1.40");
            REQUIRE(reply.long_name == "Stage Left Dimmer");
            REQUIRE(reply.short_name == "StageLeft");
            REQUIRE(reply.mac_string() == "00:1a:2b:3c:4d:5e");
            REQUIRE(reply.num_ports == 1);
            REQUIRE(reply.port_types[0] == 0x80);
            REQUIRE(reply.output_universe(0) == 18);
            REQUIRE(reply.bind_index == 1);
        }

        THEN("a 207-byte reply without the Art-Net 4 tail is accepted") {
            const std::vector<std::uint8_t> short_reply(data.begin(), data.begin() + 207);
            const auto reply = decode_reply(short_reply);
            REQUIRE(reply.ip == "192.168.1.40");
            REQUIRE(reply.bind_index == 0);
            REQUIRE(reply.bind_ip.empty());
            REQUIRE(reply.output_universe(0) == 18);
        }

        THEN("short, misidentified and wrong-opcode packets are rejected") {
            ArtPollReplyPacket reply;
            REQUIRE_FALSE(decode_artpollreply(data.data(), 206, reply));

            auto broken = data;
            broken[0] = 'X';
            REQUIRE_FALSE(decode_artpollreply(broken.data(), broken.size(), reply));

            broken = data;
            broken[8] = 0x00;
            broken[9] = 0x50;
            REQUIRE_FALSE(decode_artpollreply(broken.data(), broken.size(), reply));
        }
    }
}


SCENARIO("DeviceRegistry merges and tracks Art-Net nodes") {
    DeviceRegistry registry;
    const auto start = DeviceRegistry::Clock::now();
    const std::array<std::uint8_t, 6> mac_a{0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e};
    const std::array<std::uint8_t, 6> mac_b{0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5f};

    GIVEN("a single output device") {
        const auto data = make_poll_reply("192.168.1.40", "Stage Left Dimmer", "StageLeft", mac_a, 1, 0, 1, 1, {0x80, 0, 0, 0}, {2, 0, 0, 0});
        registry.update(decode_reply(data), "192.168.1.40", start);

        THEN("it is exposed with its display name, MAC and output universes") {
            const auto devices = registry.snapshot(start);
            REQUIRE(devices.size() == 1);
            REQUIRE(devices[0].ip == "192.168.1.40");
            REQUIRE(devices[0].display_name() == "Stage Left Dimmer");
            REQUIRE(devices[0].short_name == "StageLeft");
            REQUIRE(devices[0].mac == "00:1a:2b:3c:4d:5e");
            REQUIRE(devices[0].output_universes == std::vector<int>{18});
        }

        THEN("polling the same reply does not count as a change") {
            const auto generation = registry.generation();
            registry.update(decode_reply(data), "192.168.1.40", start + std::chrono::seconds(1));
            REQUIRE(registry.generation() == generation);
        }
    }

    GIVEN("duplicate names on two IP addresses") {
        const auto first = make_poll_reply("192.168.1.41", "Dimmer", "Dimmer", mac_a, 1, 0, 0, 1, {0x80, 0, 0, 0}, {1, 0, 0, 0});
        const auto second = make_poll_reply("192.168.1.40", "Dimmer", "Dimmer", mac_b, 1, 0, 0, 1, {0x80, 0, 0, 0}, {2, 0, 0, 0});
        registry.update(decode_reply(first), "192.168.1.41", start);
        registry.update(decode_reply(second), "192.168.1.40", start);

        THEN("the list is sorted by IP so selection is deterministic") {
            const auto devices = registry.snapshot(start);
            REQUIRE(devices.size() == 2);
            REQUIRE(devices[0].ip == "192.168.1.40");
            REQUIRE(devices[1].ip == "192.168.1.41");
            REQUIRE(devices[0].display_name() == devices[1].display_name());
        }
    }

    GIVEN("a multi-bind node sending one port per ArtPollReply") {
        const auto page_one = make_poll_reply("192.168.1.40", "Eight Port Node", "Node", mac_a, 1, 0, 0, 1, {0x80, 0, 0, 0}, {1, 0, 0, 0});
        const auto page_two = make_poll_reply("192.168.1.40", "Eight Port Node", "Node", mac_a, 2, 0, 0, 1, {0x80, 0, 0, 0}, {2, 0, 0, 0});
        registry.update(decode_reply(page_one), "192.168.1.40", start);
        registry.update(decode_reply(page_two), "192.168.1.40", start);

        THEN("both pages become one device with both universes") {
            const auto devices = registry.snapshot(start);
            REQUIRE(devices.size() == 1);
            REQUIRE(devices[0].output_universes == (std::vector<int>{1, 2}));
        }
    }

    GIVEN("a multi-port reply with four output ports") {
        const auto data = make_poll_reply("192.168.1.40", "Four Port Node", "Node", mac_a, 1, 0, 0, 4, {0x80, 0x80, 0x80, 0x80}, {0, 1, 2, 3});
        registry.update(decode_reply(data), "192.168.1.40", start);

        THEN("all four Port-Addresses are reported") {
            const auto devices = registry.snapshot(start);
            REQUIRE(devices[0].output_universes == (std::vector<int>{0, 1, 2, 3}));
        }
    }

    GIVEN("an input-only node") {
        const auto data = make_poll_reply("192.168.1.40", "Input Node", "In", mac_a, 1, 0, 0, 1, {0x40, 0, 0, 0}, {0, 0, 0, 0}, {5, 0, 0, 0});
        registry.update(decode_reply(data), "192.168.1.40", start);

        THEN("it has an input universe but no output universe") {
            const auto devices = registry.snapshot(start);
            REQUIRE(devices[0].output_universes.empty());
            REQUIRE(devices[0].input_universes == std::vector<int>{5});
        }
    }

    GIVEN("a device that stops replying") {
        const auto data = make_poll_reply("192.168.1.40", "Dimmer", "Dimmer", mac_a, 1, 0, 0, 1, {0x80, 0, 0, 0}, {1, 0, 0, 0});
        registry.update(decode_reply(data), "192.168.1.40", start);

        THEN("it expires after the discovery timeout") {
            REQUIRE(registry.snapshot(start + DeviceRegistry::kDeviceTimeout).size() == 1);
            REQUIRE(registry.snapshot(start + DeviceRegistry::kDeviceTimeout + std::chrono::seconds(1)).empty());
        }
    }
}


SCENARIO("DiscoveryService receives ArtPollReply over a real UDP socket") {
    const std::uint16_t port = find_free_udp_port();
    REQUIRE(port != 0);

    SenderNetworkService sender;
    ReceiverNetworkService receiver;
    DiscoveryService discovery{sender, receiver, port};

    std::string error;
    REQUIRE(discovery.acquire(error));
    REQUIRE(receiver.listener_for_port(port) != nullptr);

    // The listener socket is also the ArtPoll sender. A loopback ArtPoll is
    // ignored by the packet router but proves the send path works.
    REQUIRE(receiver.send_on_port(port, encode_artpoll(), {"127.0.0.1"}, error));

    const std::array<std::uint8_t, 6> mac{0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e};
    const auto reply = make_poll_reply("192.168.1.40", "Stage Left Dimmer", "StageLeft", mac, 1, 0, 1, 1, {0x80, 0, 0, 0}, {2, 0, 0, 0});
    REQUIRE(sender.send(reply, "127.0.0.1", port, error));

    REQUIRE(wait_for([&] { return discovery.devices().size() == 1; }, 1000));

    const auto devices = discovery.devices();
    REQUIRE(devices[0].ip == "192.168.1.40");
    REQUIRE(devices[0].display_name() == "Stage Left Dimmer");
    REQUIRE(devices[0].mac == "00:1a:2b:3c:4d:5e");
    REQUIRE(devices[0].output_universes == std::vector<int>{18});

    discovery.release();
    REQUIRE(receiver.listener_for_port(port) == nullptr);
}


SCENARIO("Discovery and receiving share one UDP listener") {
    const std::uint16_t port = find_free_udp_port();
    REQUIRE(port != 0);

    SenderNetworkService sender;
    ReceiverNetworkService receiver;
    auto state = std::make_shared<ReceiverState>();

    std::string error;
    REQUIRE(state->set_universes({0}, error));
    auto subscription = receiver.subscribe(port, state, error);
    REQUIRE(subscription != nullptr);

    DiscoveryService discovery{sender, receiver, port};
    REQUIRE(discovery.acquire(error));

    DmxFrame frame{};
    frame[0] = 42;
    const auto dmx = encode_artdmx(0, frame.data(), frame.size(), 1);
    REQUIRE(sender.send(dmx, "127.0.0.1", port, error));

    const std::array<std::uint8_t, 6> mac{0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e};
    const auto reply = make_poll_reply("192.168.1.40", "Stage Left Dimmer", "StageLeft", mac, 1, 0, 1, 1, {0x80, 0, 0, 0}, {2, 0, 0, 0});
    REQUIRE(sender.send(reply, "127.0.0.1", port, error));

    REQUIRE(wait_for([&] {
        return state->snapshot(0).data[0] == 42 && discovery.devices().size() == 1;
    }, 1000));

    discovery.release();
    REQUIRE(receiver.listener_for_port(port) != nullptr);

    receiver.unsubscribe(port, subscription);
    REQUIRE(receiver.listener_for_port(port) == nullptr);
}


SCENARIO("ReceiverAdvertisementService merges receive universes into ArtPollReply pages") {
    SenderNetworkService sender;
    ReceiverNetworkService receiver;
    ReceiverAdvertisementService advertisement{receiver, 0};

    REQUIRE(advertisement.receiver_count() == 0);
    REQUIRE(advertisement.replies_for("192.168.1.10").empty());

    const auto first = advertisement.add_receiver({0, 1});
    const auto second = advertisement.add_receiver({1, 16, 17, 18, 19});
    REQUIRE(advertisement.receiver_count() == 2);
    REQUIRE(advertisement.advertised_universes() == (std::vector<int>{0, 1, 16, 17, 18, 19}));

    const auto replies = advertisement.replies_for("192.168.1.10");
    REQUIRE(replies.size() == 2);

    REQUIRE(replies[0].ip == "192.168.1.10");
    REQUIRE(replies[0].short_name == "flux.artnet");
    REQUIRE(replies[0].long_name == "flux.artnet.receive");
    REQUIRE(replies[0].style == 0x06);
    REQUIRE(replies[0].bind_ip == "192.168.1.10");
    REQUIRE(replies[0].bind_index == 1);
    REQUIRE(replies[0].num_ports == 2);
    REQUIRE(replies[0].output_universe(0) == 0);
    REQUIRE(replies[0].output_universe(1) == 1);

    REQUIRE(replies[1].bind_index == 2);
    REQUIRE(replies[1].net == 0);
    REQUIRE(replies[1].subnet == 1);
    REQUIRE(replies[1].num_ports == 4);
    REQUIRE(replies[1].output_universe(0) == 16);
    REQUIRE(replies[1].output_universe(3) == 19);

    advertisement.remove_receiver(second);
    REQUIRE(advertisement.advertised_universes() == (std::vector<int>{0, 1}));
    advertisement.remove_receiver(first);
    REQUIRE(advertisement.receiver_count() == 0);
}


SCENARIO("ArtPoll from the discovery service reaches the receiver advertisement") {
    const std::uint16_t port = find_free_udp_port();
    REQUIRE(port != 0);

    SenderNetworkService sender;
    ReceiverNetworkService receiver;
    DiscoveryService discovery{sender, receiver, port};
    ReceiverAdvertisementService advertisement{receiver, port};

    std::string error;
    REQUIRE(discovery.acquire(error));
    const auto registration = advertisement.add_receiver({0, 1});
    REQUIRE(registration != 0);

    REQUIRE(discovery.poll_hosts({"127.0.0.1"}, error));
    REQUIRE(wait_for([&] { return discovery.devices().size() == 1; }, 1000));

    const auto devices = discovery.devices();
    REQUIRE(devices[0].ip == "127.0.0.1");
    REQUIRE(devices[0].source_ip == "127.0.0.1");
    REQUIRE(devices[0].display_name() == "flux.artnet.receive");
    REQUIRE(devices[0].output_universes == (std::vector<int>{0, 1}));

    advertisement.remove_receiver(registration);
    discovery.release();
}


SCENARIO("Connection probe matching requires a fresh reply from the target") {
    DiscoveredDevice device;
    device.ip = "192.168.1.40";
    device.source_ip = "192.168.1.40";
    device.last_seen = std::chrono::steady_clock::now();

    const std::vector<DiscoveredDevice> devices{device};
    const std::vector<std::string> matching{"192.168.1.40"};
    const std::vector<std::string> other{"192.168.1.41"};

    REQUIRE(artnet_send_test_access::probe_match(devices, matching, device.last_seen) == &devices[0]);
    REQUIRE(artnet_send_test_access::probe_match(devices, matching, device.last_seen + std::chrono::seconds(1)) == nullptr);
    REQUIRE(artnet_send_test_access::probe_match(devices, other, device.last_seen) == nullptr);

    DiscoveredDevice source_match = device;
    source_match.ip = "10.0.0.99";
    source_match.source_ip = "192.168.1.40";
    REQUIRE(artnet_send_test_access::probe_match({source_match}, matching, source_match.last_seen) != nullptr);
}


SCENARIO("artnet.send exposes the documented attributes and message forms") {
    ext_main(nullptr);

    PacketCapture capture;
    test_wrapper<artnet_send> instance;
    artnet_send&              object = instance;
    object.set_packet_sender(capture.function());

    GIVEN("a new instance") {
        THEN("the documented defaults are present") {
            REQUIRE((object.universes.get_atoms() == atoms{0}));
            REQUIRE((object.host == symbol("")));
            REQUIRE((object.framerate == 0.0));
            REQUIRE((object.quiet == 0));
            REQUIRE(object.outlets().size() == 2);
            REQUIRE(object.outlets()[0]->description() == "bang for every universe frame transmitted");
            REQUIRE(object.outlets()[1]->description() == "error or warning messages, devices start/end lists with device name and IP entries, connected name/IP/universe messages, and info field/value messages");
        }

        THEN("invalid attributes leave the previous values intact") {
            object.framerate = -1.0;
            REQUIRE((object.framerate == 0.0));
            object.framerate = 5000.0;
            REQUIRE((object.framerate == 0.0));

            object.quiet = 7;
            REQUIRE((object.quiet == 1));

            object.discover = 2;
            REQUIRE((object.discover == 0));
        }

        WHEN("discovery is enabled") {
            object.discover = 1;

            THEN("the attribute is set and can be cleared") {
                REQUIRE((object.discover == 1));
                object.discover = 0;
                REQUIRE((object.discover == 0));
            }
        }

        WHEN("a list is sent") {
            object.list_message(atoms{1, 2, 3});

            THEN("it starts at channel 0 of the first configured universe") {
                REQUIRE(capture.packets.size() == 1);
                const auto packet = decode_packet(capture.packets[0]);
                REQUIRE(packet.port_address == 0);
                REQUIRE(packet.data[0] == 1);
                REQUIRE(packet.data[1] == 2);
                REQUIRE(packet.data[2] == 3);
            }
        }

        WHEN("a frame is transmitted") {
            object.list_message(atoms{1});

            THEN("the left outlet emits a bang") {
                auto* output = c74::max::object_getoutput(object, 0);
                REQUIRE(output->size() == 1);
                REQUIRE((*output)[0].size() == 1);
                REQUIRE(((*output)[0][0] == symbol("bang")));
            }

            THEN("the right outlet stays quiet on success") {
                auto* output = c74::max::object_getoutput(object, 1);
                REQUIRE(output->size() == 0);
            }
        }

        WHEN("a normalized float list is sent") {
            object.list_message(atoms{0.0, 0.5, 1.0, 1.5, -0.5});

            THEN("floats are clipped to 0-1 and rounded to DMX values") {
                REQUIRE(capture.packets.size() == 1);
                const auto packet = decode_packet(capture.packets[0]);
                REQUIRE(packet.data[0] == 0);
                REQUIRE(packet.data[1] == 128);
                REQUIRE(packet.data[2] == 255);
                REQUIRE(packet.data[3] == 255);
                REQUIRE(packet.data[4] == 0);
            }
        }

        WHEN("integer and normalized float values are mixed") {
            object.list_message(atoms{255, 0.0, 128, 1.0});

            THEN("each atom is interpreted according to its type") {
                REQUIRE(capture.packets.size() == 1);
                const auto packet = decode_packet(capture.packets[0]);
                REQUIRE(packet.data[0] == 255);
                REQUIRE(packet.data[1] == 0);
                REQUIRE(packet.data[2] == 128);
                REQUIRE(packet.data[3] == 255);
            }
        }

        WHEN("a single normalized float is sent") {
            object.float_message(atoms{0.5});

            THEN("it is rounded to the nearest DMX value") {
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 128);
            }
        }

        WHEN("a non-finite float is sent") {
            object.list_message(atoms{std::numeric_limits<double>::quiet_NaN()});

            THEN("the update is rejected") {
                REQUIRE(capture.packets.empty());
            }
        }

        WHEN("set is used with normalized floats") {
            object.set_message(atoms{0.5});

            THEN("nothing is transmitted until bang") {
                REQUIRE(capture.packets.empty());
                object.bang();
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 128);
            }
        }

        WHEN("a single int is sent") {
            object.int_message(atoms{123});

            THEN("it writes one channel at offset 0") {
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 123);
            }
        }

        WHEN("a channel message is sent") {
            object.channel_message(atoms{510, 9, 8});

            THEN("it starts at the requested offset") {
                REQUIRE(capture.packets.size() == 1);
                const auto packet = decode_packet(capture.packets[0]);
                REQUIRE(packet.data[510] == 9);
                REQUIRE(packet.data[511] == 8);
            }
        }

        WHEN("universes are configured and a universe message is sent") {
            object.universes.set(atoms{1, 2, 4});
            REQUIRE((object.universes.get_atoms() == atoms{1, 2, 4}));
            object.universe_message(atoms{1, 200});

            THEN("the configured universe index selects the second entry") {
                REQUIRE(capture.packets.size() == 1);
                const auto packet = decode_packet(capture.packets[0]);
                REQUIRE(packet.port_address == 2);
                REQUIRE(packet.data[0] == 200);
            }
        }

        WHEN("all set message forms are used") {
            object.universes.set(atoms{1, 2});
            object.set_message(atoms{5, 6});
            object.set_message(atoms{symbol("channel"), 510, 7, 8});
            object.set_message(atoms{symbol("universe"), 1, 9});
            object.set_message(atoms{symbol("universe"), 1, symbol("channel"), 510, 10, 11});

            THEN("no packet is transmitted by set") {
                REQUIRE(capture.packets.empty());
            }

            THEN("bang transmits the accumulated state of every universe") {
                object.bang();
                REQUIRE(capture.packets.size() == 2);

                const auto first = decode_packet(capture.packets[0]);
                REQUIRE(first.port_address == 1);
                REQUIRE(first.data[0] == 5);
                REQUIRE(first.data[1] == 6);
                REQUIRE(first.data[510] == 7);
                REQUIRE(first.data[511] == 8);

                const auto second = decode_packet(capture.packets[1]);
                REQUIRE(second.port_address == 2);
                REQUIRE(second.data[0] == 9);
                REQUIRE(second.data[510] == 10);
                REQUIRE(second.data[511] == 11);
            }
        }

        WHEN("an invalid update is received") {
            object.list_message(atoms{1, 300});

            THEN("the right outlet reports an error") {
                auto* status = c74::max::object_getoutput(object, 1);
                REQUIRE(status->size() == 1);
                REQUIRE(((*status)[0][0] == symbol("error")));
            }

            THEN("no packet is produced and the buffer is unchanged") {
                REQUIRE(capture.packets.empty());
                object.bang();
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 0);
            }
        }

        WHEN("quiet is enabled and an invalid update is received") {
            object.quiet = 1;
            object.list_message(atoms{300});

            THEN("the right outlet still reports the error") {
                auto* status = c74::max::object_getoutput(object, 1);
                REQUIRE(status->size() == 1);
                REQUIRE(((*status)[0][0] == symbol("error")));
            }
        }

        WHEN("blackout is enabled") {
            object.list_message(atoms{10, 20});
            capture.packets.clear();
            object.blackout_message(atoms{1});

            THEN("zero frames are transmitted for every configured universe") {
                REQUIRE(capture.packets.size() == 1);
                const auto packet = decode_packet(capture.packets[0]);
                REQUIRE(packet.length == 512);
                for (std::size_t channel = 0; channel < 512; ++channel)
                    REQUIRE(packet.data[channel] == 0);
                REQUIRE(artnet_send_test_access::engine(object).state().frame(0)[0] == 10);
                REQUIRE(artnet_send_test_access::engine(object).state().frame(0)[1] == 20);
            }
        }

        WHEN("blackout is enabled and a new value arrives") {
            object.list_message(atoms{10, 20});
            object.blackout_message(atoms{1});
            capture.packets.clear();
            object.list_message(atoms{30});

            THEN("the transmitted frame stays zero but the stored value is updated") {
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 0);
                REQUIRE(artnet_send_test_access::engine(object).state().frame(0)[0] == 30);
            }
        }

        WHEN("blackout is released") {
            object.list_message(atoms{10, 20});
            object.blackout_message(atoms{1});
            capture.packets.clear();
            object.blackout_message(atoms{0});

            THEN("the retained frame is transmitted immediately") {
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 10);
                REQUIRE(decode_packet(capture.packets[0]).data[1] == 20);
            }
        }

        WHEN("blackout is enabled with multiple universes") {
            object.universes.set(atoms{1, 2});
            capture.packets.clear();
            object.blackout_message(atoms{1});

            THEN("every configured universe receives a zero frame") {
                REQUIRE(capture.packets.size() == 2);
                REQUIRE(decode_packet(capture.packets[0]).port_address == 1);
                REQUIRE(decode_packet(capture.packets[1]).port_address == 2);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 0);
                REQUIRE(decode_packet(capture.packets[1]).data[0] == 0);
            }
        }

        WHEN("an update overflows the configured universes") {
            object.channel_message(atoms{511, 1, 2});

            THEN("the update is truncated, transmitted and reported as a warning") {
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[511] == 1);

                auto* status = c74::max::object_getoutput(object, 1);
                REQUIRE(status->size() == 1);
                REQUIRE(((*status)[0][0] == symbol("warning")));
            }
        }

        WHEN("bang is used with non-contiguous universes") {
            object.universes.set(atoms{1, 2, 4});
            capture.packets.clear();
            object.bang();

            THEN("every configured universe is sent in list order") {
                REQUIRE(capture.packets.size() == 3);
                REQUIRE(decode_packet(capture.packets[0]).port_address == 1);
                REQUIRE(decode_packet(capture.packets[1]).port_address == 2);
                REQUIRE(decode_packet(capture.packets[2]).port_address == 4);
            }
        }

        WHEN("scheduled mode is enabled") {
            object.framerate = 0.1; // long interval so the test remains deterministic
            object.list_message(atoms{88});

            THEN("ordinary updates do not transmit immediately") {
                REQUIRE(capture.packets.empty());
            }

            THEN("bang still transmits immediately") {
                object.bang();
                REQUIRE(capture.packets.size() == 1);
                REQUIRE(decode_packet(capture.packets[0]).data[0] == 88);
            }
        }

        WHEN("the host is set and cleared") {
            object.host = symbol("127.0.0.1");
            THEN("the host is stored") {
                REQUIRE((object.host == symbol("127.0.0.1")));
            }

            THEN("a connection probe starts") {
                REQUIRE(artnet_send_test_access::probe_active(object));
            }

            object.host = symbol("");
            THEN("an empty host restores broadcast behaviour") {
                REQUIRE((object.host == symbol("")));
            }

            THEN("the connection probe stops") {
                REQUIRE_FALSE(artnet_send_test_access::probe_active(object));
            }
        }

        WHEN("a device is selected repeatedly") {
            object.host = symbol("127.0.0.1");
            const auto serial = artnet_send_test_access::probe_serial(object);
            object.device_message(atoms{symbol("127.0.0.1")});

            THEN("a fresh probe is started even when the host is unchanged") {
                REQUIRE(artnet_send_test_access::probe_serial(object) == serial + 1);
                REQUIRE(artnet_send_test_access::probe_active(object));
            }
        }
    }

    GIVEN("a populated discovery registry") {
        auto& registry = SharedCore::instance().discovery().registry();
        registry.clear();

        const auto start = DeviceRegistry::Clock::now();
        const std::array<std::uint8_t, 6> mac_a{0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e};
        const std::array<std::uint8_t, 6> mac_b{0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5f};

        const auto first = make_poll_reply("192.168.1.40", "Stage Left Dimmer", "StageLeft", mac_a, 1, 0, 1, 1, {0x80, 0, 0, 0}, {2, 0, 0, 0});
        const auto second = make_poll_reply("192.168.1.41", "Stage Left Dimmer", "StageLeft", mac_b, 1, 0, 2, 1, {0x80, 0, 0, 0}, {3, 0, 0, 0});
        registry.update(decode_reply(first), "192.168.1.40", start);
        registry.update(decode_reply(second), "192.168.1.41", start);

        THEN("discover defaults to 0") {
            REQUIRE(object.discover == 0);
        }

        WHEN("the devices message is output") {
            artnet_send_test_access::output_devices(object);

            THEN("the right outlet emits a start/end framed list") {
                REQUIRE(output_message_count(object, 1) == 4);
                REQUIRE(output_values(object, 1, 0) == (std::vector<std::string>{"devices", "start"}));
                REQUIRE(output_values(object, 1, 1) == (std::vector<std::string>{"device", "Stage Left Dimmer", "192.168.1.40"}));
                REQUIRE(output_values(object, 1, 2) == (std::vector<std::string>{"device", "Stage Left Dimmer", "192.168.1.41"}));
                REQUIRE(output_values(object, 1, 3) == (std::vector<std::string>{"devices", "end"}));
            }
        }

        WHEN("a duplicate name is selected") {
            object.device_message(atoms{symbol("Stage Left Dimmer")});

            THEN("the first device in IP order is used") {
                REQUIRE((object.host == symbol("192.168.1.40")));
            }
        }

        WHEN("a device is selected with name and IP") {
            object.device_message(atoms{symbol("Stage Left Dimmer"), symbol("192.168.1.41")});

            THEN("the specific device is used") {
                REQUIRE((object.host == symbol("192.168.1.41")));
            }
        }

        WHEN("a device is selected by IP only") {
            object.device_message(atoms{symbol("192.168.1.99")});

            THEN("the IP is used directly") {
                REQUIRE((object.host == symbol("192.168.1.99")));
            }
        }

        WHEN("an unknown name is selected") {
            object.device_message(atoms{symbol("Unknown")});

            THEN("an error is reported and the host stays empty") {
                REQUIRE((object.host == symbol("")));
                auto* status = c74::max::object_getoutput(object, 1);
                REQUIRE(status->size() == 1);
                REQUIRE(((*status)[0][0] == symbol("error")));
            }
        }

        WHEN("info is requested while broadcasting") {
            artnet_send_test_access::output_info(object);

            THEN("only the host field is emitted") {
                REQUIRE(output_message_count(object, 1) == 1);
                REQUIRE(output_values(object, 1, 0) == (std::vector<std::string>{"info", "host"}));
            }
        }

        WHEN("info is requested for a discovered device") {
            object.host = symbol("192.168.1.40");
            artnet_send_test_access::output_info(object);

            THEN("host, MAC, names and output universes are emitted") {
                REQUIRE(output_message_count(object, 1) == 5);
                REQUIRE(output_values(object, 1, 0) == (std::vector<std::string>{"info", "host", "192.168.1.40"}));
                REQUIRE(output_values(object, 1, 1) == (std::vector<std::string>{"info", "mac", "00:1a:2b:3c:4d:5e"}));
                REQUIRE(output_values(object, 1, 2) == (std::vector<std::string>{"info", "shortname", "StageLeft"}));
                REQUIRE(output_values(object, 1, 3) == (std::vector<std::string>{"info", "longname", "Stage Left Dimmer"}));
                REQUIRE(output_values(object, 1, 4) == (std::vector<std::string>{"info", "universes", "18"}));
            }
        }

        WHEN("info is requested for a host that is not discovered") {
            object.host = symbol("192.168.1.99");
            artnet_send_test_access::output_info(object);

            THEN("only the host field is emitted") {
                REQUIRE(output_message_count(object, 1) == 1);
                REQUIRE(output_values(object, 1, 0) == (std::vector<std::string>{"info", "host", "192.168.1.99"}));
            }
        }

        WHEN("a connection is confirmed") {
            object.quiet = 1;
            const auto devices = registry.snapshot(start);
            artnet_send_test_access::emit_connected(object, devices[0]);

            THEN("the right outlet emits connected with name, IP and universes") {
                REQUIRE(output_message_count(object, 1) == 1);
                REQUIRE(output_values(object, 1, 0) == (std::vector<std::string>{"connected", "Stage Left Dimmer", "192.168.1.40", "18"}));
            }
        }

        WHEN("the probe receives a fresh reply") {
            object.host = symbol("192.168.1.40");
            registry.update(decode_reply(first), "192.168.1.40", DeviceRegistry::Clock::now());
            artnet_send_test_access::on_probe_timer(object);

            THEN("connected is emitted and probing stops") {
                REQUIRE(output_message_count(object, 1) == 1);
                REQUIRE(output_values(object, 1, 0) == (std::vector<std::string>{"connected", "Stage Left Dimmer", "192.168.1.40", "18"}));
                REQUIRE_FALSE(artnet_send_test_access::probe_active(object));
            }
        }
    }
}
