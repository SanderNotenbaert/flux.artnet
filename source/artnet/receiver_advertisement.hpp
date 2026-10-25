/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "artnet_poll.hpp"
#include "export.hpp"
#include "network_service.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace flux::artnet {

/// Makes the receive side of the package discoverable with ArtPollReply.
///
/// Every active flux.artnet.receive object contributes its @universes. The
/// service merges them into one logical Art-Net node and answers ArtPoll on
/// the shared listener, using one BindIndex page per group of up to four
/// universes that share the same Net/Sub-Net.
class FLUX_ARTNET_EXPORT ReceiverAdvertisementService
{
  public:
    ReceiverAdvertisementService(ReceiverNetworkService& receiver, std::uint16_t port = kDefaultPort);
    ~ReceiverAdvertisementService();

    ReceiverAdvertisementService(const ReceiverAdvertisementService&) = delete;
    ReceiverAdvertisementService& operator=(const ReceiverAdvertisementService&) = delete;

    /// Register the universes of one receive object. Returns an ID for removal.
    std::uint64_t add_receiver(const std::vector<int>& universes);
    void remove_receiver(std::uint64_t id);

    /// Sorted, de-duplicated union of every active receiver's universes.
    std::vector<int> advertised_universes() const;

    /// Build the ArtPollReply pages that would be sent for a given local IP.
    /// Exposed for tests and diagnostics; does not perform network I/O.
    std::vector<ArtPollReplyPacket> replies_for(const std::string& local_ip) const;

    std::size_t receiver_count() const;

  private:
    struct Registration {
        std::vector<int> universes;
    };

    void handle_artpoll(const std::uint8_t* data, std::size_t size, const sockaddr_in& source);
    std::vector<ArtPollReplyPacket> build_replies(const std::vector<int>& universes, const std::string& local_ip) const;

    ReceiverNetworkService& m_receiver;
    std::uint16_t m_port;
    mutable std::mutex m_mutex;
    std::map<std::uint64_t, Registration> m_receivers;
    std::uint64_t m_next_id{1};
};

} // namespace flux::artnet
