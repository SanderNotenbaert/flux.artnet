/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

#include "artnet_protocol.hpp"
#include "min_helpers.hpp"
#include "network_service.hpp"
#include "receiver_state.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef FLUX_ARTNET_VERSION
#define FLUX_ARTNET_VERSION "unknown"
#endif

using namespace c74::min;
using namespace flux::artnet;
namespace max = c74::max;


class artnet_receive : public object<artnet_receive> {
  public:
    MIN_DESCRIPTION	{"Receive Art-Net DMX data over UDP. Outlet i corresponds to @universes[i] and emits 512 integers 0-255, or normalized floats 0.0-1.0 with @floatoutput 1."};
    MIN_TAGS		{"artnet, dmx, network"};
    MIN_AUTHOR		{"Sander Notenbaert"};
    MIN_RELATED		{"udpreceive, flux.artnet.send"};

    inlet<> input { this, "(bang) output the current frame for every configured universe" };

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
            "Ordered list of Art-Net Port-Addresses (0-32767) to receive. "
            "One outlet is created per entry. This attribute is creation-time only."
        },
        setter {
            MIN_FUNCTION {
                return set_universes(args);
            }
        },
        getter {
            MIN_GETTER_FUNCTION {
                return ints_to_atoms(m_state->universes());
            }
        }
    };

    attribute<number> framerate { this, "framerate", 0.0,
        description {
            "Output frame rate in frames per second. "
            "0 emits only when received data changes; a positive value emits every configured universe at that fixed rate. "
            "Valid range: 0-1000."
        },
        setter {
            MIN_FUNCTION {
                return set_framerate(args);
            }
        }
    };

    attribute<int> floatoutput { this, "floatoutput", 0,
        description {
            "When 1, outlets emit normalized floats 0.0-1.0 instead of integers 0-255."
        },
        style {style::onoff},
        setter {
            MIN_FUNCTION {
                return set_floatoutput(args);
            }
        }
    };

    attribute<int> quiet { this, "quiet", 0,
        description {"When 1, suppress error messages in the Max console."},
        style {style::onoff},
        setter {
            MIN_FUNCTION {
                return set_quiet(args);
            }
        }
    };


    message<> bang { this, "bang", "Output the current frame for every configured universe immediately.",
        MIN_FUNCTION {
            if (m_blackout)
                output_blackout();
            else
                output_all();
            return {};
        }
    };


    message<> blackout_message { this, "blackout", "blackout 1 outputs zeros on every outlet while received data continues to be tracked; blackout 0 restores the latest frames.",
        MIN_FUNCTION {
            int value = -1;
            if (args.size() != 1 || !atom_to_int(args[0], value) || (value != 0 && value != 1)) {
                log_error("blackout expects 0 or 1");
                return {};
            }

            m_blackout = (value == 1);
            if (m_blackout)
                output_blackout();
            else
                output_all();
            return {};
        }
    };


    // Min calls this after the C++ constructor and before creation-time
    // attributes are applied. Creating the first outlet here means that
    // @universes can create the rest of the outlets while the Max object is
    // still being instantiated, before patch cords are restored.
    message<> setup_message { this, "setup", "Internal setup hook.",
        MIN_FUNCTION {
            create_default_outlet();
            return {};
        }
    };


    message<> maxclass_setup { this, "maxclass_setup", "Post the loaded external version.",
        MIN_FUNCTION {
            cout << "flux.artnet.receive " << FLUX_ARTNET_VERSION << endl;
            return {};
        }
    };


    artnet_receive()
        : m_state{std::make_shared<ReceiverState>()}
    {
        std::string error;
        m_state->set_universes({0}, error);
        m_last_output_versions.assign(1, 0);
        m_framerate = 0.0;

#ifndef MIN_TEST
        m_startup_timer.delay(0.0);
#endif

        m_ready = true;
    }

    ~artnet_receive()
    {
        m_output_timer.stop();
        m_startup_timer.stop();

        if (m_subscription) {
            ReceiverNetworkService::instance().unsubscribe(m_subscription->port, m_subscription);
            m_subscription.reset();
        }

        outlets().clear();
        m_outlet_descriptions.clear();
    }


  private:
    static constexpr bool kEnableNetwork =
#ifdef MIN_TEST
        false;
#else
        true;
#endif

    static constexpr double kMaxOutputRate = 1000.0;
    static constexpr double kChangePollRate = 100.0;
    static constexpr std::size_t kMaxReceiverUniverses = 64;


    atoms set_universes(const atoms& args)
    {
        if (!m_ready)
            return args;

        std::vector<int> old_universes;
        {
            const auto& current = m_state->universes();
            old_universes.assign(current.begin(), current.end());
        }

        if (m_started || m_universes_configured) {
            log_error("@universes can only be set when the object is created; recreate the object to change it");
            return atoms{ints_to_symbol(old_universes)};
        }

        std::vector<int> values;
        std::string error;
        if (!parse_int_list(args, values, error)) {
            log_error("@universes: " + error);
            return atoms{ints_to_symbol(old_universes)};
        }
        if (values.size() > kMaxReceiverUniverses) {
            log_error("@universes supports at most " + std::to_string(kMaxReceiverUniverses) + " entries");
            return atoms{ints_to_symbol(old_universes)};
        }
        if (!m_state->set_universes(values, error)) {
            log_error("@universes: " + error);
            return atoms{ints_to_symbol(old_universes)};
        }

        rebuild_outlets(values.size());
        rebuild_outlet_descriptions(values);
        m_last_output_versions.assign(values.size(), 0);
        m_universes_configured = true;
        return atoms{ints_to_symbol(values)};
    }

    atoms set_framerate(const atoms& args)
    {
        if (!m_ready)
            return args;

        double value = 0.0;
        if (args.size() != 1 || !atom_to_number(args[0], value) || value < 0.0 || value > kMaxOutputRate) {
            log_error("@framerate must be in the range 0-1000 frames per second");
            return atoms{m_framerate};
        }

        m_framerate = value;
        if (m_started)
            restart_output_timer();
        return atoms{m_framerate};
    }

    atoms set_floatoutput(const atoms& args)
    {
        if (!m_ready)
            return args;

        int value = 0;
        if (args.size() != 1 || !atom_to_int(args[0], value)) {
            log_error("@floatoutput must be 0 or 1");
            return atoms{static_cast<const int&>(floatoutput)};
        }
        return atoms{value != 0 ? 1 : 0};
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


    void create_default_outlet()
    {
        if (!m_outlets.empty())
            return;
        m_outlets.push_back(static_cast<max::t_outlet*>(max::outlet_new(maxobj(), "list")));
        if (m_outlet_descriptions.empty())
            rebuild_outlet_descriptions({0});
    }

    /// Min uses its own outlet list for the Max assist/hint callback. The raw
    /// Max outlets are created separately, so keep one lightweight Min outlet
    /// object per universe purely to provide the hover hint.
    void rebuild_outlet_descriptions(const std::vector<int>& universes)
    {
        outlets().clear();
        m_outlet_descriptions.clear();
        m_outlet_descriptions.reserve(universes.size());

        for (std::size_t index = 0; index < universes.size(); ++index) {
            const std::string description = "universe index " + std::to_string(index) + " (Port-Address "
                                          + std::to_string(universes[index]) + "): 512 channel values";
            m_outlet_descriptions.push_back(std::make_unique<outlet<>>(this, description, "list"));
        }
    }

    /// Create one outlet per configured universe.
    ///
    /// Max places the first outlet created furthest to the right, so the
    /// default outlet created during setup becomes the rightmost outlet and is
    /// associated with the last universe. The remaining outlets are created
    /// from right to left, leaving universe index 0 as the leftmost outlet.
    void rebuild_outlets(std::size_t count)
    {
        if (count == 0)
            return;
        if (m_outlets.size() == count)
            return;

        create_default_outlet();
        max::t_outlet* default_outlet = m_outlets[0];

        std::vector<max::t_outlet*> outlets(count, nullptr);
        outlets[count - 1] = default_outlet;
        for (std::size_t index = count - 1; index-- > 0;) {
            outlets[index] = static_cast<max::t_outlet*>(max::outlet_new(maxobj(), "list"));
        }
        m_outlets = std::move(outlets);
    }


    void emit_frame(std::size_t index, const DmxFrame& frame)
    {
        if (index >= m_outlets.size() || m_outlets[index] == nullptr)
            return;

        const bool floats = static_cast<const int&>(floatoutput) != 0;
        for (std::size_t channel = 0; channel < kChannelCount; ++channel) {
            if (floats)
                max::atom_setfloat(&m_output_atoms[channel], static_cast<double>(frame[channel]) / 255.0);
            else
                max::atom_setlong(&m_output_atoms[channel], static_cast<max::t_atom_long>(frame[channel]));
        }

        max::outlet_list(m_outlets[index], nullptr, static_cast<short>(kChannelCount), m_output_atoms.data());
    }

    void output_all()
    {
        if (m_last_output_versions.size() != m_state->universe_count())
            m_last_output_versions.assign(m_state->universe_count(), 0);

        for (std::size_t index = 0; index < m_state->universe_count(); ++index) {
            const auto snapshot = m_state->snapshot(index);
            emit_frame(index, snapshot.data);
            m_last_output_versions[index] = snapshot.version;
        }
    }

    void output_blackout()
    {
        static const DmxFrame silence{};
        for (std::size_t index = 0; index < m_outlets.size(); ++index)
            emit_frame(index, silence);
    }

    void poll_output()
    {
        if (m_blackout) {
            if (m_framerate > 0.0)
                output_blackout();
            return;
        }

        if (m_last_output_versions.size() != m_state->universe_count())
            m_last_output_versions.assign(m_state->universe_count(), 0);

        const bool rate_mode = (m_framerate > 0.0);
        for (std::size_t index = 0; index < m_state->universe_count(); ++index) {
            const auto snapshot = m_state->snapshot(index);
            if (rate_mode || snapshot.version != m_last_output_versions[index]) {
                emit_frame(index, snapshot.data);
                m_last_output_versions[index] = snapshot.version;
            }
        }
    }


    double interval_ms() const
    {
        if (m_framerate > 0.0)
            return 1000.0 / m_framerate;
        return 1000.0 / kChangePollRate;
    }

    void start_output_timer()
    {
        m_output_timer.stop();
        m_output_timer.delay(interval_ms());
    }

    void restart_output_timer()
    {
        if (!m_started)
            return;
        start_output_timer();
    }

    void on_output_timer()
    {
        poll_output();
        if (m_started)
            m_output_timer.delay(interval_ms());
    }


    void on_startup()
    {
        if (m_started)
            return;

        m_started = true;
        m_universes_configured = true;

        if (kEnableNetwork) {
            std::string error;
            m_subscription = ReceiverNetworkService::instance().subscribe(kDefaultPort, m_state, error);
            if (!m_subscription)
                log_error("could not start receiver: " + error);
            start_output_timer();
        }
    }


    void log_error(const std::string& message)
    {
        if (static_cast<const int&>(quiet) == 0)
            cerr << "[flux.artnet.receive] " << message << endl;
    }


    std::shared_ptr<ReceiverState> m_state;
    std::vector<max::t_outlet*> m_outlets;
    std::vector<std::unique_ptr<outlet<>>> m_outlet_descriptions;
    std::vector<std::uint64_t> m_last_output_versions;
    std::array<max::t_atom, kChannelCount> m_output_atoms{};
    std::shared_ptr<ReceiverSubscription> m_subscription;

    double m_framerate{0.0};
    bool m_started{false};
    bool m_universes_configured{false};
    bool m_blackout{false};

    timer<timer_options::defer_delivery> m_startup_timer { this, MIN_FUNCTION {
        on_startup();
        return {};
    } };

    timer<timer_options::defer_delivery> m_output_timer { this, MIN_FUNCTION {
        on_output_timer();
        return {};
    } };


    friend struct artnet_receive_test_access;
};


MIN_EXTERNAL_CUSTOM(artnet_receive, flux.artnet.receive);
