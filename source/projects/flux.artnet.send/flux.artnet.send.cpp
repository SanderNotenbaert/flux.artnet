/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

#include "artnet_protocol.hpp"
#include "min_helpers.hpp"
#include "network_service.hpp"
#include "sender_engine.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#ifndef FLUX_ARTNET_VERSION
#define FLUX_ARTNET_VERSION "unknown"
#endif

using namespace c74::min;
using namespace flux::artnet;


class artnet_send : public object<artnet_send> {
  public:
    MIN_DESCRIPTION	{"Send Art-Net DMX data over UDP. Integer atoms are raw DMX values 0-255; float atoms are normalized 0.0-1.0, clipped and rounded. Lists longer than the remaining configured universes are truncated with a warning."};
    MIN_TAGS		{"artnet, dmx, network"};
    MIN_AUTHOR		{"Sander Notenbaert"};
    MIN_RELATED		{"udpsend, flux.artnet.receive"};

    inlet<>  input	{ this, "(list) DMX channel values to send" };

    // Declared left-to-right: Min creates outlets from the last declaration to
    // the first, so the bang outlet ends up on the left and status on the right.
    outlet<> bang_outlet { this, "bang for every universe frame transmitted", "bang" };
    outlet<> status_outlet { this, "error or warning <message> on invalid input, truncation or network failure" };

  private:
    // Guards attribute setters that run while the C++ object is still being
    // constructed (Min applies each attribute's default value immediately).
    bool m_ready{false};

  public:
    // The real list lives in the engine/state and is exposed through this
    // attribute's getter and setter. A symbol attribute is used instead of
    // attribute<ints>, whose attr_offset_array registration is not reliable
    // in current min-api/Max combinations.
    attribute<symbol> universes { this, "universes", symbol("0"),
        description {
            "Ordered list of zero-based Art-Net Port-Addresses (0-32767). "
            "Channel writes spill into subsequent entries in this list."
        },
        setter {
            MIN_FUNCTION {
                return set_universes(args);
            }
        },
        getter {
            MIN_GETTER_FUNCTION {
                return ints_to_atoms(m_engine.state().universes());
            }
        }
    };

    attribute<symbol> host { this, "host", "",
        description {
            "Unicast destination IP address or hostname. "
            "Leave empty to send to the broadcast address of every active IPv4 interface."
        },
        setter {
            MIN_FUNCTION {
                return set_host(args);
            }
        }
    };

    attribute<number> framerate { this, "framerate", 0.0,
        description {
            "Transmission rate in frames per second. "
            "0 means change-triggered transmission. Valid range: 0-1000."
        },
        setter {
            MIN_FUNCTION {
                return set_framerate(args);
            }
        }
    };

    attribute<int> quiet { this, "quiet", 0,
        description {"When 1, suppress error messages in the Max console. Errors are still sent to the right outlet."},
        style {style::onoff},
        setter {
            MIN_FUNCTION {
                return set_quiet(args);
            }
        }
    };


    message<> bang { this, "bang", "Send the current frame for every configured universe immediately.",
        MIN_FUNCTION {
            std::string error;
            if (!m_engine.send_all(error))
                log_error(error);
            return {};
        }
    };


    message<> list_message { this, "list", "Write channel values from offset 0 of universe index 0. Integers are 0-255; floats are normalized 0.0-1.0.",
        MIN_FUNCTION {
            handle_values(args, false);
            return {};
        }
    };

    message<> int_message { this, "int", "Write one raw DMX channel value (0-255) at offset 0 of universe index 0.",
        MIN_FUNCTION {
            handle_values(args, false);
            return {};
        }
    };

    message<> float_message { this, "float", "Write one normalized channel value (0.0-1.0) at offset 0 of universe index 0.",
        MIN_FUNCTION {
            handle_values(args, false);
            return {};
        }
    };

    message<> channel_message { this, "channel", "Write channel values from a channel offset. Integers are 0-255; floats are normalized 0.0-1.0.",
        MIN_FUNCTION {
            handle_channel(args, false);
            return {};
        }
    };

    message<> universe_message { this, "universe", "Write channel values in a selected universe. Integers are 0-255; floats are normalized 0.0-1.0.",
        MIN_FUNCTION {
            handle_universe(args, false);
            return {};
        }
    };

    message<> set_message { this, "set", "Update the stored frame without transmitting. Integers are 0-255; floats are normalized 0.0-1.0.",
        MIN_FUNCTION {
            handle_set(args);
            return {};
        }
    };


    message<> maxclass_setup { this, "maxclass_setup", "Post the loaded external version.",
        MIN_FUNCTION {
            cout << "flux.artnet.send " << FLUX_ARTNET_VERSION << endl;
            return {};
        }
    };


    artnet_send()
    {
        m_packet_sender = [this](const std::vector<std::uint8_t>& packet, std::string& error) {
#ifdef MIN_TEST
            (void)packet;
            (void)error;
            return true;
#else
            const std::string destination_host = static_cast<std::string>(static_cast<const symbol&>(host));
            return SenderNetworkService::instance().send(packet, destination_host, kDefaultPort, error);
#endif
        };

        m_engine.set_transport([this](const std::vector<std::uint8_t>& packet, std::string& error) {
            const bool sent = m_packet_sender(packet, error);
            if (sent)
                bang_outlet.send(symbol("bang"));
            return sent;
        });

        m_ready = true;
    }


    /// Replace the packet transport. Primarily useful for embedding and unit tests.
    using packet_sender_type = std::function<bool(const std::vector<std::uint8_t>&, std::string&)>;

    void set_packet_sender(packet_sender_type sender)
    {
        m_packet_sender = std::move(sender);
    }


  private:
    static constexpr double kMaxFramerate = 1000.0;

    atoms set_universes(const atoms& args)
    {
        if (!m_ready)
            return args;

        const std::vector<int> old_universes = m_engine.state().universes();
        std::vector<int> values;
        std::string error;

        if (!parse_int_list(args, values, error)) {
            log_error("@universes: " + error);
            return atoms{ints_to_symbol(old_universes)};
        }
        if (!m_engine.set_universes(values, error)) {
            log_error("@universes: " + error);
            return atoms{ints_to_symbol(old_universes)};
        }
        return atoms{ints_to_symbol(values)};
    }

    atoms set_host(const atoms& args)
    {
        if (!m_ready)
            return args;
        if (args.empty())
            return atoms{symbol("")};
        if (args.size() != 1) {
            log_error("@host accepts a single symbol or an empty value");
            return atoms{symbol(static_cast<const symbol&>(host))};
        }

        std::string value;
        if (!atom_to_string(args[0], value)) {
            log_error("@host must be a symbol");
            return atoms{symbol(static_cast<const symbol&>(host))};
        }
        return atoms{symbol(value)};
    }

    atoms set_framerate(const atoms& args)
    {
        if (!m_ready)
            return args;

        double value = 0.0;
        if (args.size() != 1 || !atom_to_number(args[0], value)) {
            log_error("@framerate must be a finite number");
            return atoms{m_framerate};
        }
        if (value < 0.0 || value > kMaxFramerate) {
            log_error("@framerate must be in the range 0-1000 frames per second");
            return atoms{m_framerate};
        }

        m_framerate = value;
        restart_send_timer();
        return atoms{m_framerate};
    }

    atoms set_quiet(const atoms& args)
    {
        if (!m_ready)
            return args;

        int value = 0;
        if (args.size() != 1 || !atom_to_int(args[0], value)) {
            log_error("@quiet must be 0 or 1");
            return atoms{static_cast<const int&>(quiet)};
        }
        return atoms{value != 0 ? 1 : 0};
    }


    bool parse_values(const atoms& args, UpdateRequest& request, std::string& error)
    {
        request.universe_index = 0;
        request.channel_offset = 0;
        if (!atoms_to_dmx_values(args, request.values, error))
            return false;
        if (request.values.empty()) {
            error = "update contains no channel values";
            return false;
        }
        return true;
    }

    bool parse_channel(const atoms& args, UpdateRequest& request, std::string& error)
    {
        request.universe_index = 0;
        if (args.empty()) {
            error = "channel message is missing its channel offset";
            return false;
        }
        if (!atom_to_int(args[0], request.channel_offset)) {
            error = "channel offset must be an integer";
            return false;
        }

        const atoms values(args.begin() + 1, args.end());
        if (!atoms_to_dmx_values(values, request.values, error))
            return false;
        if (request.values.empty()) {
            error = "update contains no channel values";
            return false;
        }
        return true;
    }

    bool parse_universe(const atoms& args, UpdateRequest& request, std::string& error)
    {
        request.channel_offset = 0;
        if (args.empty()) {
            error = "universe message is missing its universe index";
            return false;
        }
        if (!atom_to_int(args[0], request.universe_index)) {
            error = "universe index must be an integer";
            return false;
        }

        std::size_t position = 1;
        if (position < args.size() && atom_is_symbol(args[position], "channel")) {
            ++position;
            if (position >= args.size()) {
                error = "channel selector is missing its channel offset";
                return false;
            }
            if (!atom_to_int(args[position], request.channel_offset)) {
                error = "channel offset must be an integer";
                return false;
            }
            ++position;
        }

        const atoms values(args.begin() + static_cast<atoms::difference_type>(position), args.end());
        if (!atoms_to_dmx_values(values, request.values, error))
            return false;
        if (request.values.empty()) {
            error = "update contains no channel values";
            return false;
        }
        return true;
    }

    void handle_values(const atoms& args, bool silent)
    {
        UpdateRequest request;
        std::string error;
        if (!parse_values(args, request, error)) {
            log_error(error);
            return;
        }
        apply_update(request, silent);
    }

    void handle_channel(const atoms& args, bool silent)
    {
        UpdateRequest request;
        std::string error;
        if (!parse_channel(args, request, error)) {
            log_error(error);
            return;
        }
        apply_update(request, silent);
    }

    void handle_universe(const atoms& args, bool silent)
    {
        UpdateRequest request;
        std::string error;
        if (!parse_universe(args, request, error)) {
            log_error(error);
            return;
        }
        apply_update(request, silent);
    }

    void handle_set(const atoms& args)
    {
        if (args.empty()) {
            log_error("set message contains no channel values");
            return;
        }

        if (atom_is_symbol(args[0], "universe")) {
            handle_universe(atoms(args.begin() + 1, args.end()), true);
            return;
        }
        if (atom_is_symbol(args[0], "channel")) {
            handle_channel(atoms(args.begin() + 1, args.end()), true);
            return;
        }
        handle_values(args, true);
    }

    void apply_update(const UpdateRequest& request, bool silent)
    {
        const bool send_now = !silent && m_framerate <= 0.0;
        std::string error;
        std::string warning;
        if (!m_engine.apply(request, silent, send_now, error, &warning)) {
            log_error(error);
        }
        else if (!warning.empty()) {
            log_warning(warning);
        }
    }


    void restart_send_timer()
    {
        m_send_timer.stop();
        if (m_framerate > 0.0)
            m_send_timer.delay(1000.0 / m_framerate);
    }

    void on_send_timer()
    {
        if (m_framerate <= 0.0)
            return;

        std::string error;
        if (!m_engine.send_all(error))
            log_error(error);

        if (m_framerate > 0.0)
            m_send_timer.delay(1000.0 / m_framerate);
    }

    void log_error(const std::string& message)
    {
        status_outlet.send(symbol("error"), message);
        if (static_cast<const int&>(quiet) == 0)
            cerr << "[flux.artnet.send] " << message << endl;
    }

    void log_warning(const std::string& message)
    {
        status_outlet.send(symbol("warning"), message);
        if (static_cast<const int&>(quiet) == 0)
            cerr << "[flux.artnet.send] warning: " << message << endl;
    }


    SenderEngine m_engine;
    double m_framerate{0.0};
    packet_sender_type m_packet_sender;
    timer<timer_options::defer_delivery> m_send_timer { this, MIN_FUNCTION {
        on_send_timer();
        return {};
    } };


    friend struct artnet_send_test_access;
};


MIN_EXTERNAL_CUSTOM(artnet_send, flux.artnet.send);
