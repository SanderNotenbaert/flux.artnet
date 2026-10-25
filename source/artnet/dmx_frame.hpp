/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace flux::artnet {

/// Number of DMX channels in one universe.
inline constexpr std::size_t kChannelCount = 512;

/// A complete DMX universe frame. Values are zero-based channel offsets 0-511.
using DmxFrame = std::array<std::uint8_t, kChannelCount>;

inline bool frames_equal(const DmxFrame& a, const DmxFrame& b)
{
    return a == b;
}

} // namespace flux::artnet
