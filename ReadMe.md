# flux.artnet

A native Max package for sending and receiving [Art-Net](https://art-net.org.uk/) (DMX over Ethernet) using the [Min-API](https://cycling74.github.io/min-devkit/).

The package provides two externals:

* **`flux.artnet.send`** — send DMX channel values as Art-Net `ArtDmx` packets over UDP.
* **`flux.artnet.receive`** — receive Art-Net `ArtDmx` packets and output their DMX channel values.

Both objects use the fixed Art-Net UDP port `6454`. `flux.artnet.send` can discover Art-Net nodes with `ArtPoll` / `ArtPollReply`, connect to a discovered device by name, and confirm a connection with a silent probe. `flux.artnet.receive` advertises itself, so it is discoverable by other controllers as well as by the sender. Networking is implemented with native BSD UDP sockets and a shared core library, so multiple sender and receiver objects coexist safely and share one listener on `6454`.


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

The externals and the shared `flux.artnet.core.dylib` are written to the package's `externals` folder. The package can be installed by placing (or aliasing) this folder in `~/Documents/Max 9/Packages`.

If Max is already running while you rebuild, restart Max (or at least reopen the patch) so that it loads the new `.mxo` binaries instead of its cached copies. When the externals load, the Max console prints the loaded version, for example `flux.artnet.send 0.3.0`.

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
| `@host` | unset | Unicast destination IP address or hostname. Empty means broadcast. Changing it starts a silent ArtPoll connection probe. |
| `@framerate` | `0` | Frames per second. `0` means change-triggered transmission. |
| `@quiet` | `0` | When `1`, suppress console messages from this object. Right-outlet messages are unaffected. |
| `@discover` | `0` | When `1`, poll for Art-Net devices every 3 seconds and report changes on the right outlet. |

`@framerate` is accepted in the range 0–1000 frames per second. Art-Net itself is intended for DMX rates of up to about 44 Hz; higher values are supported for testing but should not be used on a production lighting network.

### Destination behaviour

* With no `@host`, the sender broadcasts to the directed broadcast address of every active IPv4 interface. If no usable broadcast address can be determined, it falls back to the limited broadcast address `255.255.255.255`.
* With `@host` set, the sender unicasts to that host. IP addresses and hostnames are supported.
* Clear the host with `@host ""` to return to broadcast mode.
* ArtPoll discovery is optional. You can always set `@host` directly.

For local loopback testing, use `@host 127.0.0.1`.

### Device discovery

`flux.artnet.send` can discover Art-Net nodes with the standard `ArtPoll` /
`ArtPollReply` exchange. Discovery is optional and does not affect sending.

| Message / attribute | Behaviour |
|---|---|
| `devices` | Broadcast one `ArtPoll`, wait 250 ms, then output the full device list on the right outlet. |
| `@discover 1` | Poll every 3 seconds and output the full list whenever a device appears, disappears or changes. |
| `@discover 0` | Stop polling. Known devices remain in the registry until they time out. |
| `device <name>` | Set `@host` to the first discovered device with that name, in IP order. |
| `device <name> <ip>` | Set `@host` to a specific discovered device. Use this when names are duplicated. |
| `device <ip>` | Set `@host` directly to an IP address, whether or not it was discovered. |
| `info` | Report the current destination and, when it matches a discovered device, its details. |

The `devices` message and `@discover 1` use a start/end framed list so that a
patcher can tell when the list is complete:

```
devices start
device "Stage Left Dimmer" 192.168.1.40
device "Stage Left Dimmer" 192.168.1.41
devices end
```

Each device name is emitted as one symbol atom, so names containing spaces stay
intact. The IP address is always the last atom. If a name is duplicated, the
`device <name> <ip>` form selects the intended node.

`info` emits only known values:

```
info host 192.168.1.40
info mac 00:1a:2b:3c:4d:5e
info shortname StageLeft
info longname Stage Left Dimmer
info universes 0 1 2 3
```

`info host` with no value means broadcast mode. The `mac`, `shortname`,
`longname` and `universes` lines are emitted only when `@host` resolves to a
currently discovered device. Device universes are reported as raw 15-bit
Port-Addresses, exactly like `@universes`, so they can be copied straight into
the attribute.

Discovery notes:

* Art-Net replies are normally unicast to UDP port `6454`, so discovery shares
  the package's listener on that port with `flux.artnet.receive`.
* Multi-port nodes are aggregated into one logical device by IP and Art-Net 4
  `BindIndex`. One `device ...` line represents one physical node.
* A device is kept for 10 seconds after its last reply. With `@discover 1`, a
  node that goes offline therefore disappears from the list within about
  10 seconds.
* The one-shot `devices` window is 250 ms. The Art-Net specification permits a
  node to delay its reply by up to one second, so a slow node may be missing
  from a single `devices` snapshot even though it is online. Use
  `@discover 1` when every node must appear reliably.
* Discovery uses broadcast. A device on another subnet or VLAN may not answer;
  connect to it with `@host` directly in that case.
* If another application already owns UDP port `6454`, the listener cannot
  start and an error is reported on the right outlet.
* `@quiet 1` suppresses console messages but does not hide the `devices`,
  `device`, `connected` or `info` output on the right outlet.

### Connection confirmation

When `@host` changes — through the attribute, the object box, or a `device`
message — `flux.artnet.send` starts a silent unicast ArtPoll probe:

* It uses the shared listener on UDP `6454`, so an ArtPollReply can come back
  on the standard Art-Net port.
* It sends one ArtPoll immediately and then every 3 seconds while the host
  remains set and no reply has arrived.
* It does not output device lists and does not report failed attempts.
* As soon as a fresh ArtPollReply from the target arrives, it emits:

```
connected "flux.artnet.receive" 127.0.0.1 0 1
```

The console copy is suppressed by `@quiet 1`. The right outlet always receives
the message.

If the device is not discoverable, no confirmation appears. Sending ArtDmx can
still work normally. If UDP `6454` is already owned by another application, the
probe silently skips, because only the confirmation is unavailable.

`device ...` always forces a fresh probe, even when `@host` already points to
the same device. Re-setting `@host` to the same value through the inspector may
be skipped by Max; use `device` or clear and re-set the host to force a check.

While the probe is waiting for a reply, it keeps `6454` open. Clear `@host` or
delete the object to release it.

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
* **Right outlet** — reports errors and warnings, and carries the discovery API:
  * `error <message>` / `warning <message>` for invalid input, invalid attribute values, network failures and truncated updates.
  * `devices start` / `device <name> <ip>` / `devices end` for device lists.
  * `connected <name> <ip> [universes...]` when the connection probe receives an ArtPollReply.
  * `info <field> <value>` for connection and device details.

Errors and warnings are always sent to the right outlet. `@quiet 0` (the default) also prints console messages; `@quiet 1` suppresses the console output only. Filter the right outlet with `route error warning devices device connected info` when a patch needs to handle the message types separately.

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

### Discoverability

While a receiver is listening, it answers Art-Net `ArtPoll` packets with
`ArtPollReply`, so other controllers — including `flux.artnet.send` — can
discover it.

All active receive objects in one Max process are advertised as one logical
Art-Net device with the union of their `@universes`. It reports:

* ShortName `flux.artnet`, LongName `flux.artnet.receive`
* Style `StVisual`
* its receive universes as output Port-Addresses
* multiple `BindIp` / `BindIndex` pages when more than four universes are used
* targeted ArtPoll ranges are honoured

The receiver only advertises while it is actually bound to UDP `6454`. If
another application owns the port, the receiver cannot listen and cannot be
discovered.

The sender’s discovery poll includes `127.0.0.1`, so a receive object in the
same Max process appears in `devices` even without a LAN. Other machines
discover it through the normal broadcast ArtPoll.

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
* `source/artnet/artnet_poll.hpp` — `ArtPoll` encoding and `ArtPollReply` decoding.
* `source/artnet/dmx_frame.hpp` — the 512-channel frame type.
* `source/artnet/sender_engine.hpp` — sender frame ownership, validation, coalescing and transmission policy.
* `source/artnet/receiver_state.hpp` — thread-safe receive buffers and routing state.
* `source/artnet/network_service.hpp` — shared UDP sender socket and per-port receive listeners.
* `source/artnet/discovery.hpp` / `discovery.cpp` — thread-safe device registry and the ArtPoll discovery service.
* `source/artnet/receiver_advertisement.hpp` / `receiver_advertisement.cpp` — makes the receive side discoverable with ArtPollReply.
* `source/artnet/shared_core.hpp` / `shared_core.cpp` — the process-wide networking core built as `flux.artnet.core.dylib`.

This keeps protocol behaviour testable without Max or real sockets.

Max loads each external as a separate module, so a singleton compiled into both
externals would give each one its own private copy. The package therefore
builds a small shared library, `externals/flux.artnet.core.dylib`, which both
`.mxo` bundles link. It owns the single UDP listener on port `6454`, the
outbound sender socket, and the discovery registry. As a result:

* All `flux.artnet.receive` objects in a Max process share one UDP listener per port.
* All `flux.artnet.send` objects share one outbound UDP socket and one device registry.
* A sender and a receiver in the same process share the listener, so discovery,
  connection probing and receiving can coexist on port `6454`.
* A sender's `@discover 1` works even when no receiver object exists.
* A receive object makes the process discoverable and advertises the union of
  all active receive universes.

The dylib must stay next to the `.mxo` bundles in the package's `externals`
folder. Moving a single `.mxo` out of the package without the dylib will cause
the external to fail to load.


## Unit tests

```sh
cd build
ctest -C Release . --output-on-failure
```

The suite covers Port-Address boundaries, `ArtDmx` and `ArtPoll`/`ArtPollReply` encoding/decoding, malformed packets, sequence handling, sender validation and spill-over, silent updates, change suppression, receiver routing, short payloads, outlet ordering, rate mode, real UDP loopback between the sender and receiver services, device registry merging and expiry, duplicate device names, real UDP `ArtPollReply` delivery through the discovery listener, receiver advertisement pages, and connection-probe matching and confirmation.


## Releasing

The GitHub Actions release workflow builds and tests the universal package, then publishes a ready-to-use zip when a version tag is pushed:

```sh
git tag v0.3.0
git push origin v0.3.0
```

The tag must match the `version` value in `package-info.json` (without the leading `v`). The release artifact is named like `flux.artnet-v0.3.0-macos-universal.zip` and contains a single `flux.artnet` folder with:

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
* To verify discovery, send `devices` or enable `@discover 1`. The node must answer ArtPoll on UDP port 6454; some nodes are configured not to respond to discovery.
* A receive object advertises itself as `flux.artnet.receive` with its receive universes as output ports. It should appear in `devices` and in other controllers’ discovery views.
* A sender with `@host` set starts a silent connection probe. A `connected` message confirms that the target answered ArtPoll; no message means the host is not discoverable, but sending can still work.


## Package layout

```
source/projects/flux.artnet.send      sender object and tests
source/projects/flux.artnet.receive   receiver object and tests
source/artnet                         shared protocol, networking, discovery and core library
help                                  help patchers (hand-maintained)
examples                              focused example patchers
docs                                  Max reference pages
tools                                 independent Python Art-Net probe
```


## License

MIT. See [License.md](License.md).
