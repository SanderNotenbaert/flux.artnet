/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min_unittest.h"             // required unit test header
#include "flux.artnet.receive.cpp"        // need the source of our object so that we can access it

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace c74::min;
using namespace flux::artnet;

struct artnet_receive_test_access {
    static std::shared_ptr<ReceiverState> state(artnet_receive& object)
    {
        return object.m_state;
    }

    static void poll_output(artnet_receive& object)
    {
        object.poll_output();
    }

    static std::size_t outlet_count(artnet_receive& object)
    {
        return object.m_outlets.size();
    }

    static void mark_started(artnet_receive& object)
    {
        object.m_started = true;
        object.m_universes_configured = true;
    }
};

namespace {

struct SubscriptionGuard {
    std::uint16_t port{0};
    std::shared_ptr<ReceiverSubscription> subscription;

    ~SubscriptionGuard()
    {
        if (subscription)
            ReceiverNetworkService::instance().unsubscribe(port, subscription);
    }
};

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

bool apply_values(ReceiverState& state, int port_address, const std::vector<std::uint8_t>& values, std::uint8_t sequence = 1)
{
    const auto encoded = encode_artdmx(port_address, values.data(), values.size(), sequence);
    if (encoded.empty())
        return false;

    ArtDmxPacket packet;
    if (decode_artdmx(encoded.data(), encoded.size(), packet) != DecodeError::none)
        return false;
    return state.apply(packet);
}

std::vector<int> output_values(void* object, int outlet, std::size_t message_index = 0)
{
    auto* output = c74::max::object_getoutput(object, outlet);
    if (output == nullptr || output->size() <= message_index)
        return {};

    std::vector<int> result;
    result.reserve((*output)[message_index].size());
    for (const auto& value : (*output)[message_index])
        result.push_back(static_cast<int>(value));
    return result;
}

std::size_t output_count(void* object, int outlet)
{
    auto* output = c74::max::object_getoutput(object, outlet);
    return output == nullptr ? 0 : output->size();
}

} // namespace


SCENARIO("ReceiverState routes, normalizes and compares incoming frames") {
    GIVEN("a receiver subscribed to non-contiguous universes") {
        ReceiverState state;
        std::string error;
        REQUIRE(state.set_universes({1, 2, 4}, error));

        THEN("a matching Port-Address updates the correct buffer") {
            REQUIRE(apply_values(state, 2, {10, 20}));
            const auto snapshot = state.snapshot(1);
            REQUIRE(snapshot.data[0] == 10);
            REQUIRE(snapshot.data[1] == 20);
            REQUIRE(snapshot.version == 1);
            REQUIRE(state.snapshot(0).version == 0);
        }

        THEN("an unselected Port-Address is ignored") {
            REQUIRE_FALSE(apply_values(state, 99, {10, 20}));
            REQUIRE(state.snapshot(0).version == 0);
            REQUIRE(state.stats().unrouted == 1);
        }

        THEN("identical consecutive frames do not change the version") {
            REQUIRE(apply_values(state, 1, {1, 2, 3, 4}));
            REQUIRE_FALSE(apply_values(state, 1, {1, 2, 3, 4}));
            REQUIRE(state.snapshot(0).version == 1);
        }

        THEN("a short payload updates only the channels it contains") {
            REQUIRE(apply_values(state, 1, {1, 2, 3, 4}));
            REQUIRE(apply_values(state, 1, {9, 9}));
            const auto snapshot = state.snapshot(0);
            REQUIRE(snapshot.data[0] == 9);
            REQUIRE(snapshot.data[1] == 9);
            REQUIRE(snapshot.data[2] == 3);
            REQUIRE(snapshot.data[3] == 4);
            REQUIRE(snapshot.version == 2);
        }

        THEN("sequence statistics classify duplicates and gaps without dropping packets") {
            REQUIRE(apply_values(state, 1, {1, 2}, 1));
            REQUIRE(apply_values(state, 1, {2, 3}, 2));
            REQUIRE(apply_values(state, 1, {3, 4}, 2));
            REQUIRE(apply_values(state, 1, {4, 5}, 5));

            const auto stats = state.stats();
            REQUIRE(stats.routed == 4);
            REQUIRE(stats.duplicates == 1);
            REQUIRE(stats.out_of_order == 1);
            REQUIRE(state.snapshot(0).data[0] == 4);
        }

        THEN("a sequence of zero disables numbering") {
            REQUIRE(apply_values(state, 1, {1, 2}, 0));
            REQUIRE(apply_values(state, 1, {3, 4}, 0));
            REQUIRE(state.stats().duplicates == 0);
            REQUIRE(state.stats().out_of_order == 0);
        }
    }

    THEN("invalid universe lists are rejected") {
        ReceiverState state;
        std::string error;
        REQUIRE_FALSE(state.set_universes({}, error));
        REQUIRE_FALSE(state.set_universes({1, 1}, error));
        REQUIRE_FALSE(state.set_universes({32768}, error));
        REQUIRE(state.universes() == std::vector<int>{0});
    }
}


SCENARIO("artnet.receive exposes one outlet per universe and 512-value lists") {
    ext_main(nullptr);

    test_wrapper<artnet_receive> instance;
    artnet_receive&              object = instance;

    GIVEN("a new instance") {
        THEN("the documented defaults are present") {
            REQUIRE((object.universes.get_atoms() == atoms{0}));
            REQUIRE((object.framerate == 0.0));
            REQUIRE((object.floatoutput == 0));
            REQUIRE((object.quiet == 0));
        }

        THEN("invalid attributes leave the previous values intact") {
            object.framerate = -1.0;
            REQUIRE((object.framerate == 0.0));
            object.framerate = 5000.0;
            REQUIRE((object.framerate == 0.0));

            object.framerate = 30.0;
            REQUIRE((object.framerate == 30.0));
            object.framerate = 0.0;
            REQUIRE((object.framerate == 0.0));
        }

        THEN("bang emits 512 zero values from the initial frame") {
            object.bang();
            REQUIRE(output_count(object, 0) == 1);
            const auto values = output_values(object, 0);
            REQUIRE(values.size() == 512);
            REQUIRE(values.front() == 0);
            REQUIRE(values.back() == 0);

            auto* output = c74::max::object_getoutput(object, 0);
            REQUIRE((*output)[0][0].type() == message_type::int_argument);
        }

        WHEN("several universes are configured") {
            object.universes.set(atoms{1, 2, 4});

            THEN("one outlet per universe is created") {
                REQUIRE(artnet_receive_test_access::outlet_count(object) == 3);
                REQUIRE((object.universes.get_atoms() == atoms{1, 2, 4}));
            }

            THEN("bang emits every outlet in configured order, including zero frames") {
                object.bang();
                REQUIRE(output_count(object, 0) == 1);
                REQUIRE(output_count(object, 1) == 1);
                REQUIRE(output_count(object, 2) == 1);
                REQUIRE(output_values(object, 0).size() == 512);
                REQUIRE(output_values(object, 1).size() == 512);
                REQUIRE(output_values(object, 2).size() == 512);
            }
        }

        WHEN("a received frame changes the store") {
            object.universes.set(atoms{1, 2});
            const auto state = artnet_receive_test_access::state(object);

            REQUIRE(apply_values(*state, 2, {10, 20, 30, 40}, 1));
            artnet_receive_test_access::poll_output(object);

            THEN("only the matching outlet emits, as a 512-value list") {
                REQUIRE(output_count(object, 0) == 0);
                REQUIRE(output_count(object, 1) == 1);
                const auto values = output_values(object, 1);
                REQUIRE(values.size() == 512);
                REQUIRE(values[0] == 10);
                REQUIRE(values[1] == 20);
                REQUIRE(values[2] == 30);
                REQUIRE(values[3] == 40);
                REQUIRE(values[4] == 0);
            }

            THEN("an identical frame is suppressed in change mode") {
                REQUIRE_FALSE(apply_values(*state, 2, {10, 20, 30, 40}, 2));
                artnet_receive_test_access::poll_output(object);
                REQUIRE(output_count(object, 1) == 1);
            }

            THEN("a changed frame emits again") {
                REQUIRE(apply_values(*state, 2, {11, 20, 30, 40}, 2));
                artnet_receive_test_access::poll_output(object);
                REQUIRE(output_count(object, 1) == 2);
                REQUIRE(output_values(object, 1, 1)[0] == 11);
            }
        }

        WHEN("floatoutput is enabled") {
            object.universes.set(atoms{1});
            const auto state = artnet_receive_test_access::state(object);
            REQUIRE(apply_values(*state, 1, {255, 128, 0, 0}, 1));

            object.floatoutput = 1;
            artnet_receive_test_access::poll_output(object);

            THEN("outlets emit normalized floats") {
                auto* output = c74::max::object_getoutput(object, 0);
                REQUIRE(output->size() == 1);
                const auto& values = (*output)[0];
                REQUIRE(values.size() == 512);
                REQUIRE(values[0].type() == message_type::float_argument);
                REQUIRE(static_cast<double>(values[0]) == 1.0);
                REQUIRE(std::abs(static_cast<double>(values[1]) - (128.0 / 255.0)) < 1e-6);
                REQUIRE(static_cast<double>(values[2]) == 0.0);
            }

            THEN("switching back to integer output works") {
                object.floatoutput = 0;
                object.bang();

                auto* output = c74::max::object_getoutput(object, 0);
                REQUIRE(output->size() == 2);
                const auto& values = (*output)[1];
                REQUIRE(values[0].type() == message_type::int_argument);
                REQUIRE(static_cast<int>(values[0]) == 255);
            }
        }

        WHEN("rate mode is selected") {
            object.universes.set(atoms{1});
            object.framerate = 30.0;

            artnet_receive_test_access::poll_output(object);
            artnet_receive_test_access::poll_output(object);

            THEN("the latest frame is emitted at every poll, even without changes") {
                REQUIRE(output_count(object, 0) == 2);
                REQUIRE(output_values(object, 0).size() == 512);
            }

            THEN("bang emits immediately without changing the rate") {
                object.bang();
                REQUIRE(output_count(object, 0) == 3);
                REQUIRE((object.framerate == 30.0));
            }
        }

        WHEN("change mode has not received a packet") {
            artnet_receive_test_access::poll_output(object);

            THEN("no output is emitted") {
                REQUIRE(output_count(object, 0) == 0);
            }
        }

        WHEN("universes are changed at runtime") {
            artnet_receive_test_access::mark_started(object);
            object.universes.set(atoms{1, 2});

            THEN("the change is rejected and the outlets stay consistent") {
                REQUIRE((object.universes.get_atoms() == atoms{0}));
                REQUIRE(artnet_receive_test_access::outlet_count(object) == 1);
            }
        }

        WHEN("a short packet is received") {
            object.universes.set(atoms{1});
            const auto state = artnet_receive_test_access::state(object);

            REQUIRE(apply_values(*state, 1, {1, 2, 3, 4}, 1));
            REQUIRE(apply_values(*state, 1, {9, 9}, 2));

            THEN("channels beyond the payload retain their previous values") {
                const auto snapshot = state->snapshot(0);
                REQUIRE(snapshot.data[0] == 9);
                REQUIRE(snapshot.data[1] == 9);
                REQUIRE(snapshot.data[2] == 3);
                REQUIRE(snapshot.data[3] == 4);
            }
        }
    }
}


SCENARIO("the UDP receive service routes real datagrams and shares ports") {
    GIVEN("a receiver on a free UDP port") {
        const auto port = find_free_udp_port();
        REQUIRE(port != 0);

        auto state = std::make_shared<ReceiverState>();
        std::string error;
        REQUIRE(state->set_universes({5}, error));

        auto subscription = ReceiverNetworkService::instance().subscribe(port, state, error);
        REQUIRE(subscription != nullptr);
        SubscriptionGuard guard{port, subscription};

        WHEN("a unicast ArtDmx packet is sent to it") {
            const std::vector<std::uint8_t> values{1, 2, 3, 4};
            const auto packet = encode_artdmx(5, values.data(), values.size(), 1);
            REQUIRE_FALSE(packet.empty());
            REQUIRE(SenderNetworkService::instance().send(packet, "127.0.0.1", port, error));

            THEN("the packet is decoded and routed to the subscriber") {
                REQUIRE(wait_for([&] { return state->snapshot(0).version > 0; }, 2000));
                const auto snapshot = state->snapshot(0);
                REQUIRE(snapshot.data[0] == 1);
                REQUIRE(snapshot.data[1] == 2);
                REQUIRE(snapshot.data[2] == 3);
                REQUIRE(snapshot.data[3] == 4);
            }
        }

        WHEN("a malformed datagram arrives before a valid one") {
            const std::uint8_t garbage[5] = {1, 2, 3, 4, 5};
            sockaddr_in destination{};
            destination.sin_family = AF_INET;
            destination.sin_port = htons(port);
            REQUIRE(inet_pton(AF_INET, "127.0.0.1", &destination.sin_addr) == 1);

            const int descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
            REQUIRE(descriptor >= 0);
            REQUIRE(::sendto(descriptor, garbage, sizeof(garbage), 0, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) == static_cast<ssize_t>(sizeof(garbage)));
            ::close(descriptor);

            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            THEN("the malformed packet is ignored and the listener keeps working") {
                REQUIRE(state->snapshot(0).version == 0);

                const std::vector<std::uint8_t> values{7, 8, 9, 10};
                const auto packet = encode_artdmx(5, values.data(), values.size(), 1);
                REQUIRE(SenderNetworkService::instance().send(packet, "127.0.0.1", port, error));
                REQUIRE(wait_for([&] { return state->snapshot(0).version > 0; }, 2000));
                REQUIRE(state->snapshot(0).data[0] == 7);
            }
        }

        WHEN("two receiver objects subscribe to the same port") {
            auto second_state = std::make_shared<ReceiverState>();
            REQUIRE(second_state->set_universes({5}, error));
            auto second_subscription = ReceiverNetworkService::instance().subscribe(port, second_state, error);
            REQUIRE(second_subscription != nullptr);

            {
                SubscriptionGuard second_guard{port, second_subscription};
                REQUIRE(ReceiverNetworkService::instance().listener_for_port(port) != nullptr);

                const std::vector<std::uint8_t> values{42, 43};
                const auto packet = encode_artdmx(5, values.data(), values.size(), 1);
                REQUIRE(SenderNetworkService::instance().send(packet, "127.0.0.1", port, error));

                THEN("both subscribers receive the packet from one socket") {
                    REQUIRE(wait_for([&] { return state->snapshot(0).version > 0 && second_state->snapshot(0).version > 0; }, 2000));
                    REQUIRE(state->snapshot(0).data[0] == 42);
                    REQUIRE(second_state->snapshot(0).data[0] == 42);
                }
            }
        }

        THEN("broadcast destination discovery always yields at least one address") {
            const auto addresses = SenderNetworkService::instance().broadcast_addresses();
            REQUIRE_FALSE(addresses.empty());
            for (const auto& address : addresses) {
                in_addr parsed{};
                REQUIRE(inet_pton(AF_INET, address.c_str(), &parsed) == 1);
            }
        }
    }
}
