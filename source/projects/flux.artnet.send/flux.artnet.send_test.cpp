/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min_unittest.h"          // required unit test header
#include "flux.artnet.send.cpp"        // need the source of our object so that we can access it

#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

using namespace c74::min;
using namespace flux::artnet;

struct artnet_send_test_access {
    static SenderEngine& engine(artnet_send& object)
    {
        return object.m_engine;
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
            REQUIRE(object.outlets()[1]->description() == "error or warning <message> on invalid input, truncation or network failure");
        }

        THEN("invalid attributes leave the previous values intact") {
            object.framerate = -1.0;
            REQUIRE((object.framerate == 0.0));
            object.framerate = 5000.0;
            REQUIRE((object.framerate == 0.0));

            object.quiet = 7;
            REQUIRE((object.quiet == 1));
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

            object.host = symbol("");
            THEN("an empty host restores broadcast behaviour") {
                REQUIRE((object.host == symbol("")));
            }
        }
    }
}
