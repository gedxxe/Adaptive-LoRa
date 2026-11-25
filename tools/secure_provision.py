# SPDX-License-Identifier: GPL-3.0-only
"""Generate offline, per-board NVS CSV files for the wire-v7 image."""
import argparse
import csv
import os
from pathlib import Path
import secrets
import struct

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat


DEVICE_NAMESPACE = "alora_v7cfg"
MAX_CLIENTS = 128


def registry(gateway, clients):
    return [i + (2 if i + 1 >= gateway else 1) for i in range(clients)]


def config_blob(role, network, node, gateway, interval):
    body = struct.pack("<IHBBHHHHI", 0x4344464D, 1, role, 0, network, node, gateway, 0, interval)
    checksum = 2166136261
    for byte in body:
        checksum = ((checksum ^ byte) * 16777619) & 0xFFFFFFFF
    return body + struct.pack("<I", checksum)


def credential_blob(role, network, node, gateway, clients, generation, key_material):
    return struct.pack("<4sBHHHB", b"AL07", role, network, gateway, node, clients) + generation + key_material


def write_board(path, device, credential):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerows([
            ("key", "type", "encoding", "value"),
            (DEVICE_NAMESPACE, "namespace", "", ""),
            ("device", "data", "hex2bin", device.hex()),
            ("alora_sec", "namespace", "", ""),
            ("v7", "data", "hex2bin", credential.hex()),
        ])


def provision(directory, network, gateway, clients, revoked, interval):
    if not 1 <= network <= 65535 or not 1 <= gateway <= 65534:
        raise ValueError("network or gateway ID is out of bounds")
    if not 1 <= clients <= MAX_CLIENTS or not 1000 <= interval <= 3600000:
        raise ValueError("client count or sampling interval is out of bounds")
    ids = registry(gateway, clients)
    if not revoked.issubset(ids) or len(revoked) == clients:
        raise ValueError("revocation must leave at least one registered Client")
    directory = directory.resolve()
    checkout = Path(__file__).resolve().parents[1]
    if directory == checkout or checkout in directory.parents:
        raise ValueError("the credential directory must be outside the checkout")

    key = ec.generate_private_key(ec.SECP256R1())
    signing = key.private_numbers().private_value.to_bytes(32, "big")
    verifying = key.public_key().public_bytes(Encoding.X962, PublicFormat.UncompressedPoint)
    generation = secrets.token_bytes(16)
    credentials = {node: secrets.token_bytes(16) for node in ids if node not in revoked}
    if (generation == bytes(16) or bytes(16) in credentials.values() or
            len(set(credentials.values())) != len(credentials)):
        raise RuntimeError("the random source did not produce distinct live credentials")
    directory.mkdir(mode=0o700)  # A new directory prevents mixing generations or overwriting keys.
    gateway_keys = signing + b"".join(credentials.get(node, bytes(16)) for node in ids)
    write_board(directory / f"gateway-{gateway}.csv", config_blob(1, network, gateway, gateway, interval),
                credential_blob(1, network, gateway, gateway, clients, generation, gateway_keys))
    for node, pair_key in credentials.items():
        write_board(directory / f"client-{node}.csv", config_blob(2, network, node, gateway, interval),
                    credential_blob(2, network, node, gateway, clients, generation, verifying + pair_key))
    return len(credentials)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True, help="new directory on protected offline storage")
    parser.add_argument("--network", type=int, default=1)
    parser.add_argument("--gateway", type=int, default=1)
    parser.add_argument("--clients", type=int, required=True)
    parser.add_argument("--revoke", type=int, action="append", default=[], help="Client ID to exclude, repeatable")
    parser.add_argument("--interval-ms", type=int, default=3000)
    args = parser.parse_args()
    count = provision(args.out, args.network, args.gateway, args.clients, set(args.revoke), args.interval_ms)
    print(f"Created one Gateway and {count} Client CSV files in the private output directory.")


if __name__ == "__main__":
    main()
