/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define FLUX_ARTNET_EXPORT __attribute__((visibility("default")))
#else
#define FLUX_ARTNET_EXPORT
#endif
