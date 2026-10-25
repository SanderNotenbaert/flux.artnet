/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "discovery.hpp"
#include "network_service.hpp"
#include "receiver_advertisement.hpp"

namespace flux::artnet {

/// The process-wide networking core shared by the sender and receiver externals.
///
/// Max loads each external as a separate module. Instances of a singleton
/// inside an external are therefore private to that module. This class lives
/// in the separate flux.artnet.core dylib, which both externals link, giving
/// them one shared UDP listener on port 6454 and one discovery registry.
class FLUX_ARTNET_EXPORT SharedCore
{
  public:
    static SharedCore& instance();

    SharedCore(const SharedCore&) = delete;
    SharedCore& operator=(const SharedCore&) = delete;

    SenderNetworkService& sender()
    {
        return m_sender;
    }

    ReceiverNetworkService& receiver()
    {
        return m_receiver;
    }

    DiscoveryService& discovery()
    {
        return m_discovery;
    }

    ReceiverAdvertisementService& advertisement()
    {
        return m_advertisement;
    }

  private:
    SharedCore();
    ~SharedCore();

    SenderNetworkService m_sender;
    ReceiverNetworkService m_receiver;
    DiscoveryService m_discovery;
    ReceiverAdvertisementService m_advertisement;
};

} // namespace flux::artnet
