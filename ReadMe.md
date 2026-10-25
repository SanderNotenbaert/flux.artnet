# flux.artnet

A native Max package for sending and receiving [Art-Net](https://art-net.org.uk/) (DMX over Ethernet) using the [Min-API](https://cycling74.github.io/min-devkit/).

The package provides two externals:

* **`flux.artnet.send`** — send DMX channel values as Art-Net `ArtDmx` packets over UDP.
* **`flux.artnet.receive`** — receive Art-Net `ArtDmx` packets and output their DMX channel values.

Both objects use the fixed Art-Net UDP port `6454`. Networking is implemented with native BSD UDP sockets and a shared network service, so multiple sender and receiver objects can coexist safely.


## Requirements

* macOS 10.13 or newer
* Max 8.2 or newer
* Xcode 10 or newer
* [CMake](https://cmake.org/download/) 3.19 or newer

The macOS build produces a universal binary containing both `arm64` (Apple Silicon) and `x86_64` (Intel) code, so the same `.mxo` runs natively on either architecture.

This release targets macOS only. Windows support is not implemented.


## Building

Clone the repository recursively so that the `min-api` and `min-lib` submodules are initialized:

```sh
git clone --recursive https://github.com/SanderNotenbaert/flux.artnet.git
```

Then configure and build:

```sh
cd flux.artnet
mkdir build
cd build
cmake -G Xcode ..
cmake --build . --config Release
```

The externals are written to the package's `externals` folder. The package can be installed by placing (or aliasing) this folder in `~/Documents/Max 9/Packages`.

If Max is already running while you rebuild, restart Max (or at least reopen the patch) so that it loads the new `.mxo` binaries instead of its cached copies. When the externals load, the Max console prints the loaded version, for example `flux.artnet.send 0.1.0`.

To build for a single architecture instead of a universal binary:

```sh
cmake -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 ..
```


## Terminology

The three addressing concepts are deliberately kept distinct:

* **Port-Address** — the 15-bit Art-Net address carried in an `ArtDmx` packet, from `0` to `32767`. It is composed of a 7-bit Net, 4-bit Sub-Net and 4-bit Universe.
* **Universe index** — a zero-based position in the ordered `@universes` list. The Max message API always uses this index, never the raw Port-Address.
* **Channel offset** — a zero-based channel position from `0` to `511` inside a 512-channel DMX frame.

For example, `@universes 1 2 4` configures three Port-Addresses:

| Universe index | Port-Address |
|---|---|
| 0 | 1 |
| 1 | 2 |
| 2 | 4 |

`@universes` accepts either separate integers (`@universes 1 2 4`) or a single space- or comma-separated value (`@universes "1 2 4"`) when editing through a text field.

`universe 1 255` therefore writes channel 0 of the second entry (Port-Address 2), not Port-Address 1.

Every configured universe owns an independent 512-byte frame buffer, initially all zeroes.


## `flux.artnet.send`

### Attributes

| Attribute | Default | Description |
|---|---|---|
| `@universes` | `0` | Ordered list of zero-based Port-Addresses (0–32767). May be changed at runtime. |
| `@host` | unset | Unicast destination IP address or hostname. Empty means broadcast. |
| `@framerate` | `0` | Frames per second. `0` means change-triggered transmission. |
| `@quiet` | `0` | When `1`, suppress error messages in the Max console. |

`@framerate` is accepted in the range 0–1000 frames per second. Art-Net itself is intended for DMX rates of up to about 44 Hz; higher values are supported for testing but should not be used on a production lighting network.

### Destination behaviour

* With no `@host`, the sender broadcasts to the directed broadcast address of every active IPv4 interface. If no usable broadcast address can be determined, it falls back to the limited broadcast address `255.255.255.255`.
* With `@host` set, the sender unicasts to that host. IP addresses and hostnames are supported.
* Clear the host with `@host ""` to return to broadcast mode.
* No ArtPoll discovery is required to send to a known host.

For local loopback testing, use `@host 127.0.0.1`.

### Message forms

All channel values are zero-based and must be integers in `0`–`255`.

| Message | Meaning |
|---|---|
| `1 2 3` | Write from channel offset 0 of universe index 0. |
| `channel 510 1 2` | Write from channel offset 510 of universe index 0. |
| `universe 1 1 2` | Write from offset 0 of universe index 1. |
| `universe 1 channel 510 1 2` | Write from offset 510 of universe index 1. |

Writes spill into subsequent entries of the `@universes` list in order. For example, with `@universes 1 2`, `channel 510 1 2 3 4` writes channels 510 and 511 of Port-Address 1 and channels 0 and 1 of Port-Address 2.

Each message is a **partial** update. Channels not mentioned retain their previous values.

Channel values may be either integers in `0`–`255` or normalized floats in `0.0`–`1.0`. Floats are clipped to `0.0`–`1.0`, multiplied by 255 and rounded to the nearest integer DMX step. Integer and float values may be mixed in one list. For example, `0.0 0.5 1.0` and `0 128 255` both produce the same first three DMX values.

Because float atoms are always treated as normalized, `128.0` clips to `1.0` and becomes DMX 255. Use integers when you want raw DMX values.

### Silent updates with `set`

Each form also accepts a `set` prefix:

```
set 1 2 3
set channel 510 1 2
set universe 1 1 2
set universe 1 channel 510 1 2
```

A `set` message updates the stored frame without transmitting immediately.

* At `@framerate 0`, the next ordinary message that changes at least one channel also flushes any pending `set` changes. `bang` always flushes everything.
* At `@framerate N` (N > 0), the next scheduled tick sends the latest state.
* `set` never pauses or disables scheduled transmission.

### `bang`

`bang` immediately sends the current full frame for every configured universe, regardless of `@framerate`. It does not change the configured rate or clear stored channel values.

### `blackout`

`blackout 1` immediately sends a full zero frame for every configured universe and keeps transmitting zeros while active. Stored channel values are retained, and incoming updates continue to update them. `blackout 0` immediately sends the retained frames and resumes normal transmission. In scheduled mode, scheduled ticks continue with zeros while blacked out. The left outlet bangs for every zero frame.

### Transmission modes

**Change-triggered (`@framerate 0`)**

* Transmits when an ordinary message changes at least one channel value.
* Suppresses transmission when an update leaves every value unchanged.
* `set` updates do not transmit by themselves.
* Every transmission contains the complete 512-channel frame for each affected universe.
* `bang` sends all configured universes.

**Scheduled (`@framerate N`, N > 0)**

* Sends every configured universe at the requested rate, including when the data has not changed.
* Coalesces intermediate updates: each tick sends the latest buffer state.
* `bang` sends all universes immediately in addition to scheduled transmission.
* The timer is one-shot and re-armed from its callback, so callbacks cannot overlap.

### Validation

An update is validated before any buffer is modified:

* Channel offset must be `0`–`511`.
* Universe index must refer to an entry in `@universes`.
* Channel values must be integers in `0`–`255`, or normalized floats clipped to `0.0`–`1.0` and rounded to the nearest DMX step. Non-finite floats are rejected.
* At least one channel value is required.
* If the update is longer than the remaining configured universes, the values that fit are applied and the rest are dropped. A warning is sent to the right outlet and printed to the console unless `@quiet 1`.
* Validation errors leave all buffers unchanged and are reported to the right outlet; `@quiet 1` suppresses only the console copy.

`@universes` must contain at least one entry, only valid Port-Addresses, and no duplicates.

### Sender outlets

`flux.artnet.send` has two outlets:

* **Left outlet (`bang`)** — emits a bang after every universe frame is successfully transmitted. Sending a frame that spills over three universes produces three bangs.
* **Right outlet (`error <message>` / `warning <message>`)** — reports invalid input, invalid attribute values, network failures and truncated updates.

Errors and warnings are always sent to the right outlet. `@quiet 0` (the default) also prints them to the Max console; `@quiet 1` suppresses the console output only.

### Protocol details

The sender emits standard Art-Net 4 `ArtDmx` packets:

* identifier `Art-Net\0`
* opcode `0x5000` (little-endian)
* `ProtVer` 14 (big-endian)
* per-universe sequence numbers 1–255 (wrapping; 0 is never emitted)
* full 512-channel payload

The sequence number is maintained independently for each configured universe.


## `flux.artnet.receive`

### Attributes

| Attribute | Default | Description |
|---|---|---|
| `@universes` | `0` | Ordered list of Port-Addresses to receive (0–32767). Creation-time only. |
| `@framerate` | `0` | Output frame rate. `0` emits on change; a positive value emits at a fixed rate (0–1000). |
| `@floatoutput` | `0` | When `1`, outlets emit normalized floats `0.0`–`1.0` instead of integers `0`–`255`. |
| `@quiet` | `0` | When `1`, suppress error messages in the Max console. |

`@quiet 0` (the default) reports invalid attribute values and UDP listener bind failures to the Max console. `@quiet 1` suppresses those console messages; normal output is unchanged.

### Outlets

One outlet is created for each entry in `@universes`, in list order. The first outlet (leftmost) corresponds to universe index 0.

Each outlet emits a plain list of exactly 512 integer values by default. With `@floatoutput 1`, it emits 512 normalized floats instead (`0.0`–`1.0`). The universe number is **not** prepended either way.

Hovering over an outlet shows a hint such as `flux.artnet.receive: universe index 0 (Port-Address 1): 512 channel values`.

### Receiving behaviour

* Only packets whose Port-Address is in `@universes` are processed.
* Packets for other Port-Addresses are ignored.
* Each configured universe keeps an independent 512-channel frame.
* Incoming channel data is compared with the stored frame. Identical frames do not change the stored state and do not trigger output in change mode.
* Malformed or unrelated UDP datagrams are ignored safely.
* Receivers for the same local port share a single UDP socket inside the package, so multiple receiver objects can listen on port 6454 at the same time.

### `blackout`

`blackout 1` immediately emits zeros on every outlet. The receiver keeps listening and updating its internal buffers, but live output is suppressed while blacked out. In fixed-rate mode it emits zeros at `@framerate`; in change mode the zeros are emitted once, and `bang` emits zeros. `blackout 0` immediately emits the latest stored frames and resumes normal output. This is a local output blackout; use `flux.artnet.send blackout 1` to black out physical fixtures.

### Short payloads

Art-Net `ArtDmx` payloads may contain fewer than 512 channels. This implementation validates that the payload length is even and between 2 and 512 bytes, then updates only the channels present in the packet. All remaining channels retain their previous stored values. This is documented behaviour, not an accident of the implementation.

### Output rate

`@framerate 0` is **change mode**:

* Emits a 512-value list when the stored channel data changes.
* Never emits merely because a packet arrived with identical data.
* `bang` emits every current frame immediately, including universes that have not received a packet yet (zero frames).
* Received packets are polled on Max's main thread at 100 Hz, so output coalesces bursts and never blocks the UDP receive loop.

`@framerate N` where N > 0 is **rate mode**:

* The UDP receive loop runs independently of Max output.
* Emits the latest stored frame for every configured universe at the configured rate.
* Intermediate packets coalesce into the latest state; no backlog is built.
* `bang` emits all current frames immediately without changing the configured rate.

### Dynamic configuration

`@universes` is a **creation-time** attribute for `flux.artnet.receive`, because changing the number of outlets of a live Max object is not reliable. Attempts to change it at runtime are rejected and reported to the console unless `@quiet 1` is set.

Create a new object to use a different set of universes. `@framerate`, `@floatoutput` and `@quiet` can be changed at runtime.

A maximum of 64 universes per receiver object is supported to keep the number of outlets and output work bounded.

### Sequence handling

Sequence number 0 means sequence numbering is disabled and is accepted. Non-zero sequences are tracked per universe for diagnostics, including duplicates and gaps, but packets are **not** rejected based on sequence number. This keeps interoperability with senders that restart, wrap around, or do not use sequence numbering.


## Protocol library

The Art-Net encoder, decoder, Port-Address helpers, frame state, and UDP services are deliberately separated from the Max object classes:

* `source/artnet/artnet_protocol.hpp` — Port-Address helpers, `ArtDmx` encode/decode, sequence tracking.
* `source/artnet/dmx_frame.hpp` — the 512-channel frame type.
* `source/artnet/sender_engine.hpp` — sender frame ownership, validation, coalescing and transmission policy.
* `source/artnet/receiver_state.hpp` — thread-safe receive buffers and routing state.
* `source/artnet/network_service.hpp` — shared UDP sender socket and per-port receive listeners.

This keeps protocol behaviour testable without Max or real sockets.

Because Max loads each external as a separate module, a "shared service" means shared among all instances of that external. All `flux.artnet.receive` objects in a patch share one UDP listener per port; all `flux.artnet.send` objects share one outbound UDP socket. The sender and receiver modules do not need to share runtime state with each other.


## Unit tests

```sh
cd build
ctest -C Release . --output-on-failure
```

The suite covers Port-Address boundaries, packet encoding/decoding, malformed packets, sequence handling, sender validation and spill-over, silent updates, change suppression, receiver routing, short payloads, outlet ordering, rate mode, and real UDP loopback between the sender and receiver services.


## Releasing

The GitHub Actions release workflow builds and tests the universal package, then publishes a ready-to-use zip when a version tag is pushed:

```sh
git tag v0.1.0
git push origin v0.1.0
```

The tag must match the `version` value in `package-info.json` (without the leading `v`). The release artifact is named like `flux.artnet-v0.1.0-macos-universal.zip` and contains a single `flux.artnet` folder with:

```
externals/
help/
docs/
package-info.json
License.md
ReadMe.md
```

Unzip that folder into `~/Documents/Max 9/Packages`. Running the Release workflow manually builds and uploads the same zip as a workflow artifact without creating a GitHub Release.


## Manual interoperability check

The automated tests verify the package against itself over a real UDP loopback. For a final hardware check, patch `flux.artnet.send @host <node-ip>` to an Art-Net node and verify that it responds to a known universe and channel. Any Art-Net 3 or Art-Net 4 implementation that supports standard `ArtDmx` packets should interoperate.

The repository also includes an independent Python probe for manual checks:

```sh
# Receive packets sent by Max
python3 tools/artnet_probe.py receive --udp-port 6454 --port-address 0 --show 16

# Send a packet to a Max receiver
python3 tools/artnet_probe.py send --host 127.0.0.1 --port-address 0 --values 1,2,3,4
```

It uses only the Python standard library and is intentionally independent of the C++ implementation.

Common check points:

* The node's Port-Address must match the configured `@universes` value.
* DMX channels in Max are zero-based; most lighting consoles are one-based. Channel offset 0 corresponds to DMX channel 1.
* Broadcast may not reach a node on a different subnet or VLAN; use unicast `@host` in that case.


## Package layout

```
source/projects/flux.artnet.send      sender object and tests
source/projects/flux.artnet.receive   receiver object and tests
source/artnet                         shared protocol and networking library
help                                  help patchers
examples                              focused example patchers
docs                                  Max reference pages
tools                                 independent Python Art-Net probe
```


## License

MIT. See [License.md](License.md).
