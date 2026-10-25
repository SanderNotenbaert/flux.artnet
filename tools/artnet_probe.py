#!/usr/bin/env python3
"""Small, dependency-free Art-Net probe for manual interoperability checks.

This tool is intentionally independent of the Max package implementation. It
can send ArtDmx packets to a receiver and listen for ArtDmx packets from a
sender, which makes it useful for verifying flux.artnet against a separate
implementation.

Examples:
    python3 tools/artnet_probe.py send --host 127.0.0.1 --port-address 0 --values 1,2,3
    python3 tools/artnet_probe.py receive --udp-port 6454 --port-address 0
"""

import argparse
import socket
import struct
import sys
import time


ARTNET_ID = b"Art-Net\x00"
OP_DMX = 0x5000
PROT_VER = 14
MAX_CHANNELS = 512


def split_port_address(port_address):
    if not 0 <= port_address <= 32767:
        raise ValueError("Port-Address must be in the range 0-32767")
    net = (port_address >> 8) & 0x7F
    subnet = (port_address >> 4) & 0x0F
    universe = port_address & 0x0F
    return net, subnet, universe


def make_artdmx(port_address, values, sequence=1):
    net, subnet, universe = split_port_address(port_address)
    if len(values) == 0 or len(values) > MAX_CHANNELS or len(values) % 2 != 0:
        raise ValueError("ArtDmx needs an even number of channels between 2 and 512")
    if any(value < 0 or value > 255 for value in values):
        raise ValueError("channel values must be in the range 0-255")

    header = ARTNET_ID
    header += struct.pack("<H", OP_DMX)
    header += struct.pack(">H", PROT_VER)
    header += struct.pack("B", sequence & 0xFF)
    header += struct.pack("B", 0)
    header += struct.pack("B", (subnet << 4) | universe)
    header += struct.pack("B", net)
    header += struct.pack(">H", len(values))
    return header + bytes(values)


def parse_artdmx(data):
    if len(data) < 18:
        return None
    if data[:8] != ARTNET_ID:
        return None
    opcode = struct.unpack_from("<H", data, 8)[0]
    if opcode != OP_DMX:
        return None
    prot_ver = struct.unpack_from(">H", data, 10)[0]
    if prot_ver != PROT_VER:
        return None
    sequence = data[12]
    physical = data[13]
    subuni = data[14]
    net = data[15]
    if net & 0x80:
        return None
    length = struct.unpack_from(">H", data, 16)[0]
    if length < 2 or length > MAX_CHANNELS or length % 2 != 0:
        return None
    if len(data) < 18 + length:
        return None

    port_address = (net << 8) | subuni
    values = list(data[18:18 + length])
    return {
        "sequence": sequence,
        "physical": physical,
        "port_address": port_address,
        "length": length,
        "values": values,
    }


def command_send(args):
    values = [int(value.strip()) for value in args.values.split(",") if value.strip()]
    packet = make_artdmx(args.port_address, values, args.sequence)

    destination = (args.host, args.udp_port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    try:
        for index in range(args.count):
            sock.sendto(packet, destination)
            if args.interval > 0 and index + 1 < args.count:
                time.sleep(args.interval)
    finally:
        sock.close()

    net, subnet, universe = split_port_address(args.port_address)
    print(
        f"sent {args.count} packet(s) of {len(values)} channel(s) to {args.host}:{args.udp_port}; "
        f"Port-Address {args.port_address} (Net {net}, Sub-Net {subnet}, Universe {universe}), "
        f"sequence {args.sequence}"
    )
    return 0


def command_receive(args):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", args.udp_port))
    sock.settimeout(0.2)

    print(f"listening on UDP port {args.udp_port} for Port-Address {args.port_address} (Ctrl-C to stop)")
    deadline = time.monotonic() + args.timeout if args.timeout > 0 else None
    received = 0

    try:
        while deadline is None or time.monotonic() < deadline:
            try:
                data, source = sock.recvfrom(2048)
            except socket.timeout:
                continue

            packet = parse_artdmx(data)
            if packet is None:
                continue
            if args.port_address >= 0 and packet["port_address"] != args.port_address:
                continue

            received += 1
            values = packet["values"]
            shown = values[: args.show] if args.show > 0 else values
            channels = ", ".join(f"{index}:{value}" for index, value in enumerate(shown))
            print(
                f"{source[0]}:{source[1]} Port-Address {packet['port_address']} "
                f"sequence {packet['sequence']} length {packet['length']} | {channels}"
            )
            if args.count > 0 and received >= args.count:
                break
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()

    print(f"received {received} matching packet(s)")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Independent Art-Net ArtDmx probe")
    subparsers = parser.add_subparsers(dest="command", required=True)

    sender = subparsers.add_parser("send", help="send ArtDmx packets")
    sender.add_argument("--host", default="127.0.0.1", help="destination IP address")
    sender.add_argument("--udp-port", type=int, default=6454, help="destination UDP port")
    sender.add_argument("--port-address", type=int, default=0, help="Art-Net Port-Address (0-32767)")
    sender.add_argument("--values", default="1,2", help="comma-separated channel values (even count, 2-512)")
    sender.add_argument("--sequence", type=int, default=1, help="sequence number 0-255")
    sender.add_argument("--count", type=int, default=1, help="number of packets to send")
    sender.add_argument("--interval", type=float, default=0.0, help="seconds between packets")
    sender.set_defaults(function=command_send)

    receiver = subparsers.add_parser("receive", help="receive and decode ArtDmx packets")
    receiver.add_argument("--udp-port", type=int, default=6454, help="local UDP port")
    receiver.add_argument("--port-address", type=int, default=-1, help="filter by Port-Address; -1 accepts all")
    receiver.add_argument("--timeout", type=float, default=10.0, help="seconds to listen; 0 means forever")
    receiver.add_argument("--count", type=int, default=0, help="stop after this many matching packets; 0 means unlimited")
    receiver.add_argument("--show", type=int, default=16, help="number of channels to print; 0 prints all")
    receiver.set_defaults(function=command_receive)

    args = parser.parse_args()
    try:
        return args.function(args)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
