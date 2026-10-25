/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

#include "artnet_protocol.hpp"
#include "discovery.hpp"
#include "min_helpers.hpp"
#include "network_service.hpp"
#include "sender_engine.hpp"
#include "shared_core.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
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
    MIN_DESCRIPTION	{"Send DMX channel values from Max to Art-Net lighting devices and software over the network."};
    MIN_TAGS		{"artnet, dmx, network"};
    MIN_AUTHOR		{"Sander Notenbaert"};
    MIN_RELATED		{"udpsend, flux.artnet.receive"};

    inlet<>  input	{ this, "(list) DMX channel values to send" };

    // Declared left-to-right: Min creates outlets from the last declaration to
    // the first, so the bang outlet ends up on the left and status on the right.
    outlet<> bang_outlet { this, "bang for every universe frame transmitted", "bang" };
    outlet<> status_outlet { this, "error or warning messages, devices start/end lists with device name and IP entries, connected name/IP/universe messages, and info field/value messages" };

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
            "Leave empty to send to the broadcast address of every active IPv4 interface. "
            "Changing the host starts a silent ArtPoll probe and emits a connected message when a device replies."
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
        description {"When 1, suppress console messages from this object. Right-outlet messages are unaffected."},
        style {style::onoff},
        setter {
            MIN_FUNCTION {
                return set_quiet(args);
            }
        }
    };

    attribute<int> discover { this, "discover", 0,
        description {
            "When 1, poll for Art-Net devices every 3 seconds and output the full device list on the right outlet whenever it changes. "
            "Discovery temporarily shares UDP port 6454 with flux.artnet.receive."
        },
        style {style::onoff},
        setter {
            MIN_FUNCTION {
                return set_discover(args);
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


    message<> devices_message { this, "devices", "Poll for Art-Net devices and output the full list on the right outlet after a 250 ms collection window.",
        MIN_FUNCTION {
            request_devices();
            return {};
        }
    };


    message<> device_message { this, "device", "Connect to a discovered device by name, or by name and IP address. A single IP address is also accepted.",
        MIN_FUNCTION {
            select_device(args);
            return {};
        }
    };


    message<> info_message { this, "info", "Output the current host and, when it matches a discovered device, its MAC, names and output universes on the right outlet.",
        MIN_FUNCTION {
            output_info();
            return {};
        }
    };


    message<> maxclass_setup { this, "maxclass_setup", "Post the loaded external version.",
        MIN_FUNCTION {
            cout << "flux.artnet.send " << FLUX_ARTNET_VERSION << endl;
            return {};
        }
    };


    message<> blackout_message { this, "blackout", "blackout 1 sends zeros on every configured universe; blackout 0 restores the retained frames.",
        MIN_FUNCTION {
            int value = -1;
            if (args.size() != 1 || !atom_to_int(args[0], value) || (value != 0 && value != 1)) {
                log_error("blackout expects 0 or 1");
                return {};
            }

            m_engine.set_blackout(value == 1);

            std::string error;
            if (!m_engine.send_all(error))
                log_error(error);
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
            return SharedCore::instance().sender().send(packet, destination_host, kDefaultPort, error);
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

    ~artnet_send()
    {
        m_discovery_timer.stop();
        m_devices_timer.stop();
        m_probe_timer.stop();
        m_probe_active = false;
        release_discovery_lease();
    }


    /// Replace the packet transport. Primarily useful for embedding and unit tests.
    using packet_sender_type = std::function<bool(const std::vector<std::uint8_t>&, std::string&)>;

    void set_packet_sender(packet_sender_type sender)
    {
        m_packet_sender = std::move(sender);
    }


  private:
    static constexpr double kMaxFramerate = 1000.0;

    static constexpr bool kEnableNetwork =
#ifdef MIN_TEST
        false;
#else
        true;
#endif

    static constexpr double kDiscoveryTickMs = 250.0;
    static constexpr double kDevicesCollectionMs = 250.0;
    static constexpr auto kDiscoveryPollInterval = std::chrono::seconds(3);

    static constexpr double kProbeCheckMs = 250.0;
    static constexpr auto kProbePollInterval = std::chrono::seconds(3);

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
        if (args.empty()) {
            start_probe("");
            return atoms{symbol("")};
        }
        if (args.size() != 1) {
            log_error("@host accepts a single symbol or an empty value");
            return atoms{symbol(static_cast<const symbol&>(host))};
        }

        std::string value;
        if (!atom_to_string(args[0], value)) {
            log_error("@host must be a symbol");
            return atoms{symbol(static_cast<const symbol&>(host))};
        }

        start_probe(value);
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

    atoms set_discover(const atoms& args)
    {
        if (!m_ready)
            return args;

        int value = 0;
        if (args.size() != 1 || !atom_to_int(args[0], value) || (value != 0 && value != 1)) {
            log_error("@discover must be 0 or 1");
            return atoms{static_cast<const int&>(discover)};
        }

        const bool enabled = value != 0;
        if (enabled == m_discovery_enabled)
            return atoms{value};

        m_discovery_enabled = enabled;
        if (enabled) {
            if (!ensure_discovery_lease()) {
                m_discovery_enabled = false;
                return atoms{0};
            }

            // Emit the current list on the first timer tick, even when the
            // registry has not changed since discovery was last enabled.
            m_last_discovery_generation = std::numeric_limits<std::uint64_t>::max();
            send_discovery_poll();
            m_last_discovery_poll = std::chrono::steady_clock::now();
            m_discovery_timer.delay(kDiscoveryTickMs);
        }
        else {
            m_discovery_timer.stop();
            release_discovery_lease_if_unused();
        }

        return atoms{value};
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


    // ------------------------------------------------------------------
    // Device discovery
    // ------------------------------------------------------------------

    static bool is_ipv4(const std::string& value)
    {
        in_addr address{};
        return ::inet_pton(AF_INET, value.c_str(), &address) == 1;
    }

    static std::string join_tokens(const std::vector<std::string>& tokens)
    {
        std::string result;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0)
                result.push_back(' ');
            result += tokens[i];
        }
        return result;
    }

    bool ensure_discovery_lease()
    {
        if (m_lease_held)
            return true;

        if (!kEnableNetwork) {
            m_lease_held = true;
            return true;
        }

        std::string error;
        if (!SharedCore::instance().discovery().acquire(error)) {
            log_error("could not start device discovery: " + error);
            return false;
        }

        m_lease_held = true;
        m_discovery_error_reported = false;
        return true;
    }

    void release_discovery_lease()
    {
        if (!m_lease_held)
            return;

        m_lease_held = false;
        if (kEnableNetwork)
            SharedCore::instance().discovery().release();
    }

    void release_discovery_lease_if_unused()
    {
        if (m_discovery_enabled || m_devices_pending || m_probe_active)
            return;
        release_discovery_lease();
    }

    void send_discovery_poll()
    {
        if (!kEnableNetwork)
            return;

        std::string error;
        if (!SharedCore::instance().discovery().poll(error)) {
            if (!m_discovery_error_reported) {
                log_error("device discovery failed: " + error);
                m_discovery_error_reported = true;
            }
        }
        else {
            m_discovery_error_reported = false;
        }
    }

    static const DiscoveredDevice* find_probe_match(const std::vector<DiscoveredDevice>& devices,
                                                    const std::vector<std::string>& targets,
                                                    std::chrono::steady_clock::time_point probe_start)
    {
        for (const auto& device : devices) {
            if (device.last_seen < probe_start)
                continue;
            if (std::find(targets.begin(), targets.end(), device.ip) != targets.end())
                return &device;
            if (!device.source_ip.empty() && std::find(targets.begin(), targets.end(), device.source_ip) != targets.end())
                return &device;
        }
        return nullptr;
    }

    bool ensure_probe_lease()
    {
        if (m_lease_held)
            return true;

        if (!kEnableNetwork) {
            m_lease_held = true;
            return true;
        }

        // Probing is best-effort and silent. If another application owns the
        // port, sending still works; only the confirmation is unavailable.
        std::string error;
        if (!SharedCore::instance().discovery().acquire(error))
            return false;

        m_lease_held = true;
        m_discovery_error_reported = false;
        return true;
    }

    void send_probe_poll()
    {
        if (!kEnableNetwork || !m_probe_active || m_probe_targets.empty())
            return;

        std::string error;
        SharedCore::instance().discovery().poll_hosts(m_probe_targets, error);
        // Deliberately silent: no reply simply means the host is not
        // discoverable, while ArtDmx sending can still work.
    }

    void start_probe(const std::string& host)
    {
        if (!m_ready)
            return;

        // Stop the previous probe but keep an already-held listener lease so
        // changing between hosts does not close and reopen port 6454.
        stop_probe(false);

        if (host.empty()) {
            release_discovery_lease_if_unused();
            return;
        }

        const auto targets = resolve_ipv4_addresses(host);
        if (targets.empty()) {
            release_discovery_lease_if_unused();
            return;
        }

        if (!ensure_probe_lease()) {
            release_discovery_lease_if_unused();
            return;
        }

        m_probe_targets = targets;
        m_probe_start = std::chrono::steady_clock::now();
        m_probe_last_poll = m_probe_start;
        m_probe_active = true;
        ++m_probe_serial;

        send_probe_poll();

        if (kEnableNetwork)
            m_probe_timer.delay(kProbeCheckMs);
    }

    void stop_probe(bool release_lease)
    {
        m_probe_timer.stop();
        m_probe_active = false;
        m_probe_targets.clear();

        if (release_lease)
            release_discovery_lease_if_unused();
    }

    void emit_connected(const DiscoveredDevice& device)
    {
        atoms message;
        message.emplace_back(symbol("connected"));
        message.emplace_back(symbol(device.display_name()));
        message.emplace_back(symbol(device.ip));
        for (const int universe : device.output_universes)
            message.emplace_back(universe);
        status_outlet.send(message);

        if (static_cast<const int&>(quiet) == 0) {
            cout << "[flux.artnet.send] connected to " << device.display_name() << " (" << device.ip << ")";
            if (!device.output_universes.empty()) {
                cout << ", universes";
                for (const int universe : device.output_universes)
                    cout << " " << universe;
            }
            cout << endl;
        }
    }

    void on_probe_timer()
    {
        if (!m_probe_active)
            return;

        const auto now = std::chrono::steady_clock::now();
        const auto devices = SharedCore::instance().discovery().devices();
        const DiscoveredDevice* match = find_probe_match(devices, m_probe_targets, m_probe_start);
        if (match != nullptr) {
            emit_connected(*match);
            stop_probe(true);
            return;
        }

        if (now - m_probe_last_poll >= kProbePollInterval) {
            send_probe_poll();
            m_probe_last_poll = now;
        }

        if (m_probe_active)
            m_probe_timer.delay(kProbeCheckMs);
    }

    void request_devices()
    {
        if (!ensure_discovery_lease())
            return;

        m_devices_pending = true;
        send_discovery_poll();
        m_devices_timer.stop();
        m_devices_timer.delay(kDevicesCollectionMs);
    }

    void set_host_value(const std::string& value, bool force_probe = false)
    {
        const std::uint64_t probe_before = m_probe_serial;
        host.set(atoms{symbol(value)});

        // Min may or may not invoke the attribute setter for an unchanged
        // value. Device selection explicitly asks for one fresh probe either
        // way, but avoid starting a second one if the setter already did.
        if (force_probe && m_probe_serial == probe_before)
            start_probe(value);
    }

    void select_device(const atoms& args)
    {
        if (args.empty()) {
            log_error("device message requires a device name or IP address");
            return;
        }

        std::vector<std::string> tokens;
        for (const auto& value : args) {
            std::string token;
            if (!atom_to_string(value, token)) {
                log_error("device arguments must be symbols");
                return;
            }
            if (!token.empty())
                tokens.push_back(token);
        }

        if (tokens.empty()) {
            log_error("device message requires a device name or IP address");
            return;
        }

        // A trailing IPv4 address is the optional disambiguator. Device names
        // may contain spaces, so every preceding token belongs to the name.
        std::string specific_ip;
        if (tokens.size() >= 2 && is_ipv4(tokens.back())) {
            specific_ip = tokens.back();
            tokens.pop_back();
        }

        const std::string name = join_tokens(tokens);
        auto devices = SharedCore::instance().discovery().devices();

        // `device <ip>` is a direct destination. A device that happens to be
        // named like an IP address still wins, as long as it is discovered.
        if (specific_ip.empty() && tokens.size() == 1 && is_ipv4(tokens[0])) {
            const bool named = std::any_of(devices.begin(), devices.end(), [&](const DiscoveredDevice& device) {
                return device.display_name() == tokens[0];
            });
            if (!named) {
                set_host_value(tokens[0], true);
                return;
            }
        }

        if (name.empty()) {
            log_error("device message requires a device name");
            return;
        }

        const DiscoveredDevice* match = nullptr;
        for (const auto& device : devices) {
            if (device.display_name() != name)
                continue;
            if (!specific_ip.empty() && device.ip != specific_ip)
                continue;
            match = &device;
            break;
        }

        if (match == nullptr) {
            std::string message = "no discovered device named '" + name + "'";
            if (!specific_ip.empty())
                message += " at " + specific_ip;
            if (devices.empty())
                message += " (no devices have been discovered; send the devices message first)";
            log_error(message);
            return;
        }

        set_host_value(match->ip, true);
    }

    void output_devices()
    {
        auto& service = SharedCore::instance().discovery();
        const auto generation = service.generation();
        const auto devices = service.devices();

        status_outlet.send(symbol("devices"), symbol("start"));
        for (const auto& device : devices)
            status_outlet.send(symbol("device"), symbol(device.display_name()), symbol(device.ip));
        status_outlet.send(symbol("devices"), symbol("end"));

        m_last_discovery_generation = generation;
    }

    void output_info()
    {
        const std::string configured = static_cast<std::string>(static_cast<const symbol&>(host));
        if (configured.empty()) {
            status_outlet.send(symbol("info"), symbol("host"));
            return;
        }

        status_outlet.send(symbol("info"), symbol("host"), symbol(configured));

        const auto addresses = resolve_ipv4_addresses(configured);
        if (addresses.empty())
            return;

        const auto devices = SharedCore::instance().discovery().devices();
        const DiscoveredDevice* match = nullptr;
        for (const auto& device : devices) {
            if (std::find(addresses.begin(), addresses.end(), device.ip) != addresses.end()) {
                match = &device;
                break;
            }
        }
        if (match == nullptr)
            return;

        if (!match->mac.empty())
            status_outlet.send(symbol("info"), symbol("mac"), symbol(match->mac));
        if (!match->short_name.empty())
            status_outlet.send(symbol("info"), symbol("shortname"), symbol(match->short_name));
        if (!match->long_name.empty())
            status_outlet.send(symbol("info"), symbol("longname"), symbol(match->long_name));

        if (!match->output_universes.empty()) {
            atoms message;
            message.emplace_back(symbol("info"));
            message.emplace_back(symbol("universes"));
            for (const int universe : match->output_universes)
                message.emplace_back(universe);
            status_outlet.send(message);
        }
    }

    void on_discovery_timer()
    {
        if (!m_discovery_enabled)
            return;

        const auto now = std::chrono::steady_clock::now();
        if (now - m_last_discovery_poll >= kDiscoveryPollInterval) {
            send_discovery_poll();
            m_last_discovery_poll = now;
        }

        const auto& service = SharedCore::instance().discovery();
        const auto generation = service.generation();
        if (generation != m_last_discovery_generation)
            output_devices();

        if (m_discovery_enabled)
            m_discovery_timer.delay(kDiscoveryTickMs);
    }

    void on_devices_timer()
    {
        output_devices();
        m_devices_pending = false;
        release_discovery_lease_if_unused();
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

    bool m_discovery_enabled{false};
    bool m_lease_held{false};
    bool m_devices_pending{false};
    bool m_discovery_error_reported{false};
    std::uint64_t m_last_discovery_generation{std::numeric_limits<std::uint64_t>::max()};
    std::chrono::steady_clock::time_point m_last_discovery_poll{};

    bool m_probe_active{false};
    std::vector<std::string> m_probe_targets;
    std::chrono::steady_clock::time_point m_probe_start{};
    std::chrono::steady_clock::time_point m_probe_last_poll{};
    std::uint64_t m_probe_serial{0};

    timer<timer_options::defer_delivery> m_send_timer { this, MIN_FUNCTION {
        on_send_timer();
        return {};
    } };

    timer<timer_options::defer_delivery> m_discovery_timer { this, MIN_FUNCTION {
        on_discovery_timer();
        return {};
    } };

    timer<timer_options::defer_delivery> m_devices_timer { this, MIN_FUNCTION {
        on_devices_timer();
        return {};
    } };

    timer<timer_options::defer_delivery> m_probe_timer { this, MIN_FUNCTION {
        on_probe_timer();
        return {};
    } };


    friend struct artnet_send_test_access;
};


MIN_EXTERNAL_CUSTOM(artnet_send, flux.artnet.send);
