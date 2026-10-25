/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "artnet_protocol.hpp"
#include "dmx_frame.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace flux::artnet {

/// Owns the receive-side frame buffers for one receiver object.
///
/// This state is shared between the UDP listener thread and Max's main thread.
/// All access to frame data is protected by a mutex. The universe list is
/// immutable once the receiver has been registered, so it can be read without
/// locking by the listener's routing check.
class ReceiverState
{
  public:
    struct Stats {
        std::uint64_t packets{0};
        std::uint64_t routed{0};
        std::uint64_t unrouted{0};
        std::uint64_t changed{0};
        std::uint64_t duplicates{0};
        std::uint64_t out_of_order{0};
    };

    struct Snapshot {
        DmxFrame data{};
        std::uint64_t version{0};
    };

    ReceiverState()
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

        for (std::size_t i = 0; i < universes.size(); ++i) {
            if (!valid_port_address(universes[i])) {
                error = "@universes contains an invalid Port-Address (" + std::to_string(universes[i]) + "); valid values are 0-32767";
                return false;
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (universes[j] == universes[i]) {
                    error = "@universes contains the Port-Address " + std::to_string(universes[i]) + " more than once";
                    return false;
                }
            }
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_universes = universes;
        m_frames.assign(universes.size(), DmxFrame{});
        m_versions.assign(universes.size(), 0);
        m_sequences.assign(universes.size(), SequenceTracker{});
        m_stats = Stats{};
        return true;
    }

    /// Check whether a Port-Address is subscribed.
    /// The universe list must not change while a receiver is registered.
    bool accepts(int port_address) const
    {
        for (const int configured : m_universes) {
            if (configured == port_address)
                return true;
        }
        return false;
    }

    /// Apply a decoded ArtDmx packet.
    /// Returns true when the stored data changed.
    bool apply(const ArtDmxPacket& packet)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_stats.packets;

        std::size_t index = m_universes.size();
        for (std::size_t i = 0; i < m_universes.size(); ++i) {
            if (m_universes[i] == packet.port_address) {
                index = i;
                break;
            }
        }
        if (index == m_universes.size()) {
            ++m_stats.unrouted;
            return false;
        }
        ++m_stats.routed;

        switch (m_sequences[index].update(packet.sequence)) {
            case SequenceStatus::duplicate:
                ++m_stats.duplicates;
                break;
            case SequenceStatus::out_of_order:
                ++m_stats.out_of_order;
                break;
            case SequenceStatus::disabled:
            case SequenceStatus::first:
            case SequenceStatus::in_order:
                break;
        }

        bool changed = false;
        const std::size_t count = std::min(packet.length, kChannelCount);
        for (std::size_t channel = 0; channel < count; ++channel) {
            if (m_frames[index][channel] != packet.data[channel]) {
                m_frames[index][channel] = packet.data[channel];
                changed = true;
            }
        }

        if (changed) {
            ++m_versions[index];
            ++m_stats.changed;
        }
        return changed;
    }

    Snapshot snapshot(std::size_t index) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Snapshot result;
        if (index < m_frames.size()) {
            result.data = m_frames[index];
            result.version = m_versions[index];
        }
        return result;
    }

    std::size_t universe_count() const
    {
        return m_universes.size();
    }

    /// The universe list is immutable once a receiver is registered.
    const std::vector<int>& universes() const
    {
        return m_universes;
    }

    int universe(std::size_t index) const
    {
        return index < m_universes.size() ? m_universes[index] : -1;
    }

    Stats stats() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }

  private:
    mutable std::mutex m_mutex;
    std::vector<int> m_universes;
    std::vector<DmxFrame> m_frames;
    std::vector<std::uint64_t> m_versions;
    std::vector<SequenceTracker> m_sequences;
    Stats m_stats{};
};

} // namespace flux::artnet
