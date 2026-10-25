/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "shared_core.hpp"

namespace flux::artnet {

SharedCore::SharedCore()
    : m_discovery(m_sender, m_receiver)
    , m_advertisement(m_receiver)
{
}

SharedCore::~SharedCore()
{
    // Join every listener thread before the advertisement and discovery
    // services (which the listeners call into) are destroyed.
    m_discovery.release();
    m_receiver.set_poll_handler({});
    m_receiver.set_poll_reply_handler({});
    m_receiver.stop_all();
}

SharedCore& SharedCore::instance()
{
    static SharedCore core;
    return core;
}

} // namespace flux::artnet
