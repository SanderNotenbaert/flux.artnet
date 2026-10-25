/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "artnet_protocol.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace flux::artnet {

/// A channel update after the message selectors have been parsed.
struct UpdateRequest {
    int universe_index{0};
    int channel_offset{0};
    std::vector<int> values;
};

/// Owns the sender's ordered universe list and its independent frame buffers.
///
/// The frame buffers are keyed by Port-Address, so reconfiguring @universes
/// preserves the stored data of universes that remain configured.
class SenderState
{
  public:
    SenderState()
    {
        std::string ignored;
        set_universes({0}, ignored);
    }

    bool set_universes(const std::vector<int>& universes, std::string& error)
    {
        if (universes.empty()) {
            error = "@universes must contain at least one Port-Address";
            return false;
        }

        std::set<int> seen;
        for (const int port_address : universes) {
            if (!valid_port_address(port_address)) {
                error = "@universes contains an invalid Port-Address (" + std::to_string(port_address) + "); valid values are 0-32767";
                return false;
            }
            if (!seen.insert(port_address).second) {
                error = "@universes contains the Port-Address " + std::to_string(port_address) + " more than once";
                return false;
            }
        }

        std::map<int, DmxFrame> old_frames;
        std::map<int, std::uint8_t> old_sequences;
        for (std::size_t i = 0; i < m_universes.size(); ++i) {
            old_frames[m_universes[i]] = m_frames[i];
            old_sequences[m_universes[i]] = m_sequences[i];
        }

        std::vector<DmxFrame> new_frames(universes.size());
        std::vector<std::uint8_t> new_sequences(universes.size(), 0);
        for (std::size_t i = 0; i < universes.size(); ++i) {
            const auto frame = old_frames.find(universes[i]);
            if (frame != old_frames.end())
                new_frames[i] = frame->second;

            const auto sequence = old_sequences.find(universes[i]);
            if (sequence != old_sequences.end())
                new_sequences[i] = sequence->second;
        }

        m_universes = universes;
        m_frames = std::move(new_frames);
        m_sequences = std::move(new_sequences);
        return true;
    }

    /// Apply an update atomically. Nothing is written unless the complete
    /// request is valid.
    ///
    /// @param request      The parsed update.
    /// @param changed      Indices of configured universes whose data changed.
    /// @param error        Human-readable validation error.
    /// Apply an update atomically for validation errors. If the update is
    /// longer than the remaining configured universes, the values that fit are
    /// applied and the rest are dropped with an optional warning.
    ///
    /// @param request      The parsed update.
    /// @param changed      Indices of configured universes whose data changed.
    /// @param error        Human-readable validation error.
    /// @param warning      Optional truncated-update warning.
    bool apply(const UpdateRequest& request, std::vector<std::size_t>& changed, std::string& error, std::string* warning = nullptr)
    {
        changed.clear();
        if (warning)
            warning->clear();

        if (request.values.empty()) {
            error = "update contains no channel values";
            return false;
        }

        if (request.universe_index < 0 || static_cast<std::size_t>(request.universe_index) >= m_universes.size()) {
            error = "universe index " + std::to_string(request.universe_index) + " is outside the configured universe list";
            return false;
        }

        if (request.channel_offset < 0 || request.channel_offset >= static_cast<int>(kChannelCount)) {
            error = "channel offset " + std::to_string(request.channel_offset) + " is outside the range 0-511";
            return false;
        }

        const std::size_t available = (m_universes.size() - static_cast<std::size_t>(request.universe_index)) * kChannelCount
                                    - static_cast<std::size_t>(request.channel_offset);

        std::size_t count = request.values.size();
        if (count > available) {
            count = available;
            if (warning) {
                *warning = "update contains " + std::to_string(request.values.size()) + " values but only " + std::to_string(available)
                         + " fit in the configured universes; truncated to " + std::to_string(available);
            }
        }

        for (std::size_t i = 0; i < count; ++i) {
            const int value = request.values[i];
            if (value < 0 || value > 255) {
                error = "channel value " + std::to_string(value) + " is outside the range 0-255";
                return false;
            }
        }

        std::size_t universe_index = static_cast<std::size_t>(request.universe_index);
        std::size_t channel_offset = static_cast<std::size_t>(request.channel_offset);
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t byte = static_cast<std::uint8_t>(request.values[i]);
            if (m_frames[universe_index][channel_offset] != byte) {
                m_frames[universe_index][channel_offset] = byte;
                if (changed.empty() || changed.back() != universe_index)
                    changed.push_back(universe_index);
            }
            ++channel_offset;
            if (channel_offset == kChannelCount) {
                channel_offset = 0;
                ++universe_index;
            }
        }
        return true;
    }

    const std::vector<int>& universes() const
    {
        return m_universes;
    }

    std::size_t universe_count() const
    {
        return m_universes.size();
    }

    const DmxFrame& frame(std::size_t index) const
    {
        return m_frames[index];
    }

    /// Sequence numbers run from 1 to 255; 0 is reserved for disabling
    /// numbering and is never emitted by this sender.
    std::uint8_t next_sequence(std::size_t index)
    {
        std::uint8_t& sequence = m_sequences[index];
        sequence = (sequence >= 255) ? 1 : static_cast<std::uint8_t>(sequence + 1);
        return sequence;
    }

  private:
    std::vector<int> m_universes;
    std::vector<DmxFrame> m_frames;
    std::vector<std::uint8_t> m_sequences;
};

/// Sender behaviour independent of Max. Packets are handed to a transport
/// function, which makes it straightforward to test without real sockets.
class SenderEngine
{
  public:
    using Transport = std::function<bool(const std::vector<std::uint8_t>&, std::string&)>;

    explicit SenderEngine(Transport transport = {})
        : m_transport(std::move(transport))
    {
    }

    void set_transport(Transport transport)
    {
        m_transport = std::move(transport);
    }

    bool set_universes(const std::vector<int>& universes, std::string& error)
    {
        if (!m_state.set_universes(universes, error))
            return false;

        // Drop silent-update markers for universes that are no longer configured.
        for (auto pending = m_pending_silent.begin(); pending != m_pending_silent.end();) {
            if (std::find(m_state.universes().begin(), m_state.universes().end(), *pending) == m_state.universes().end())
                pending = m_pending_silent.erase(pending);
            else
                ++pending;
        }
        return true;
    }

    /// Apply an update.
    ///
    /// @param silent    A `set` update; it never transmits by itself.
    /// @param send_now  Transmit immediately (change-triggered mode).
    bool apply(const UpdateRequest& request, bool silent, bool send_now, std::string& error, std::string* warning = nullptr)
    {
        std::vector<std::size_t> changed;
        if (!m_state.apply(request, changed, error, warning))
            return false;

        if (silent) {
            // Remember silently changed universes so that the next ordinary
            // change in change-triggered mode also flushes them.
            for (const std::size_t index : changed)
                m_pending_silent.insert(m_state.universes()[index]);
            return true;
        }

        if (!send_now || changed.empty())
            return true;

        std::vector<std::size_t> to_send;
        for (std::size_t index = 0; index < m_state.universe_count(); ++index) {
            const bool was_changed = std::find(changed.begin(), changed.end(), index) != changed.end();
            const bool was_pending = m_pending_silent.count(m_state.universes()[index]) != 0;
            if (was_changed || was_pending)
                to_send.push_back(index);
        }

        const bool ok = send_indices(to_send, error);
        if (ok) {
            for (const std::size_t index : to_send)
                m_pending_silent.erase(m_state.universes()[index]);
        }
        else {
            // Keep failed universes pending so that a later update or bang retries them.
            for (const std::size_t index : to_send)
                m_pending_silent.insert(m_state.universes()[index]);
        }
        return ok;
    }

    /// Send the current full frame for every configured universe.
    bool send_all(std::string& error)
    {
        bool ok = true;
        std::string first_error;
        for (std::size_t i = 0; i < m_state.universe_count(); ++i) {
            std::string universe_error;
            if (!send_index(i, universe_error)) {
                if (ok)
                    first_error = universe_error;
                ok = false;
            }
        }
        if (!ok) {
            error = first_error;
            return false;
        }
        m_pending_silent.clear();
        return true;
    }

    const SenderState& state() const
    {
        return m_state;
    }

    SenderState& state()
    {
        return m_state;
    }

  private:
    bool send_indices(const std::vector<std::size_t>& indices, std::string& error)
    {
        bool ok = true;
        std::string first_error;
        for (const std::size_t index : indices) {
            std::string universe_error;
            if (!send_index(index, universe_error)) {
                if (ok)
                    first_error = universe_error;
                ok = false;
            }
        }
        if (!ok)
            error = first_error;
        return ok;
    }

    bool send_index(std::size_t index, std::string& error)
    {
        if (index >= m_state.universe_count()) {
            error = "internal error: universe index out of range";
            return false;
        }
        if (!m_transport) {
            error = "no packet transport is configured";
            return false;
        }

        const auto& frame = m_state.frame(index);
        const auto packet = encode_artdmx(m_state.universes()[index], frame.data(), frame.size(), m_state.next_sequence(index));
        if (packet.empty()) {
            error = "internal error: failed to encode ArtDmx packet";
            return false;
        }
        return m_transport(packet, error);
    }

    SenderState m_state;
    Transport m_transport;
    std::set<int> m_pending_silent;
};

} // namespace flux::artnet
