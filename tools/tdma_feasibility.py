"""Host-only airtime budget; no radio access or production scheduling policy.

Current equations: docs/en/SCHEDULED_LINK.md. The scenario grid below models a cached schedule.
All durations are integer microseconds. Unknown overheads remain None.
"""
import argparse
from dataclasses import asdict, dataclass
from fractions import Fraction
from itertools import product
import json
from pathlib import Path

HEADER_BYTES = 28
MAX_APPLICATION_BYTES = 68
ACK_BYTES = HEADER_BYTES + 9


def integer(name, value, minimum, maximum=None):
    if type(value) is not int or value < minimum or (maximum is not None and value > maximum):
        raise ValueError(f"{name} must be an integer in {minimum}..{maximum or 'unbounded'}")
    return value


def ceil_fraction(value):
    return -(-value.numerator // value.denominator)


@dataclass(frozen=True)
class Phy:
    sf: int = 7
    bandwidth_hz: int = 125000
    coding_rate_denominator: int = 5
    preamble_symbols: int = 8
    explicit_header: bool = True
    crc: bool = True
    ldro: bool | None = None

    def __post_init__(self):
        # SF5/6 use different SX126x coefficients; deliberately outside this model.
        integer("sf", self.sf, 7, 12)
        integer("bandwidth Hz", self.bandwidth_hz, 1)
        if self.bandwidth_hz not in (125000, 250000, 500000):
            raise ValueError("modeled bandwidths are 125000, 250000 and 500000 Hz")
        integer("coding rate denominator", self.coding_rate_denominator, 5, 8)
        integer("preamble", self.preamble_symbols, 1, 65535)
        if type(self.explicit_header) is not bool or type(self.crc) is not bool:
            raise ValueError("header and CRC settings must be bool")
        if self.ldro is not None and type(self.ldro) is not bool:
            raise ValueError("ldro must be bool or None")

    @property
    def symbol_us(self):
        return Fraction((1 << self.sf) * 1000000, self.bandwidth_hz)

    @property
    def effective_ldro(self):
        return self.symbol_us >= 16000 if self.ldro is None else self.ldro


def airtime_us(packet_bytes, phy):
    """Complete PHY payload length, including the protocol header; round up once."""
    integer("packet bytes", packet_bytes, 0, 255)
    numerator = 8 * packet_bytes - 4 * phy.sf + 28 + 16 * phy.crc - 20 * (not phy.explicit_header)
    denominator = 4 * (phy.sf - 2 * phy.effective_ldro)
    payload_symbols = 8 + max(ceil_fraction(Fraction(numerator, denominator)), 0) * phy.coding_rate_denominator
    symbols = phy.preamble_symbols + Fraction(17, 4) + payload_symbols
    return ceil_fraction(symbols * phy.symbol_us)


def schedule_budget(payload_bytes, data_phy, *, control_phy=Phy(sf=9),
                    beacon_airtime_us=None, ack_strategy="bitmap", retry_opportunities=0,
                    switch_us=None, processing_us=None, guard_us=None, superframe_us):
    """Reserve every client's DATA and acknowledgment in each worst-case round.

    One gateway preparation and processing allowance per beacon/RX/ACK action;
    one guard per uplink slot. Schedule distribution and rejoin are excluded.
    """
    count = len(payload_bytes)
    integer("clients", count, 1, 70)
    for size in payload_bytes:
        integer("application payload bytes", size, 0, MAX_APPLICATION_BYTES)
    integer("retry opportunities", retry_opportunities, 0, 2)
    integer("superframe", superframe_us, 1)
    overheads = dict(beacon_airtime_us=beacon_airtime_us, switch_us=switch_us,
                     processing_us=processing_us, guard_us=guard_us)
    for name, value in overheads.items():
        if value is not None:
            integer(name, value, 0)
    if ack_strategy not in ("individual", "bitmap"):
        raise ValueError("ack strategy must be individual or bitmap")
    rounds = 1 + retry_opportunities
    per_client = [airtime_us(HEADER_BYTES + size, data_phy) for size in payload_bytes]
    uplink_once = sum(per_client)
    bitmap_bytes = (count + 7) // 8
    ack_packet_bytes = ACK_BYTES if ack_strategy == "individual" else HEADER_BYTES + 2 + bitmap_bytes
    acks_per_round = count if ack_strategy == "individual" else 1
    ack_once = acks_per_round * airtime_us(ack_packet_bytes, control_phy)
    actions = 1 + rounds * (count + acks_per_round)
    guards = rounds * count
    known_floor = rounds * (uplink_once + ack_once) + (beacon_airtime_us or 0)
    known_floor += actions * ((switch_us or 0) + (processing_us or 0)) + guards * (guard_us or 0)
    unknown = [name for name, value in overheads.items() if value is None]
    total = None if unknown else known_floor
    state = "INFEASIBLE" if known_floor > superframe_us else "UNDETERMINED" if unknown else "CONDITIONAL_FIT"
    return {
        "clients": count, "payload_bytes": list(payload_bytes), "data_phy": asdict(data_phy),
        "control_phy": asdict(control_phy), "symbol_us": float(data_phy.symbol_us),
        "ldro": data_phy.effective_ldro, "per_client_data_us": per_client,
        "initial_uplink_us": uplink_once, "initial_ack_us": ack_once,
        "ack_strategy": ack_strategy, "ack_packet_bytes": ack_packet_bytes,
        "bitmap_bytes": bitmap_bytes if ack_strategy == "bitmap" else None,
        "retry_opportunities": retry_opportunities,
        "retry_airtime_us": retry_opportunities * (uplink_once + ack_once),
        "total_uplink_us": rounds * uplink_once, "total_ack_us": rounds * ack_once,
        **overheads, "gateway_actions": actions, "guard_windows": guards,
        "unknown_inputs": unknown, "known_occupancy_floor_us": known_floor,
        "total_occupancy_us": total, "superframe_us": superframe_us,
        "remaining_us": None if total is None else superframe_us - total, "feasibility": state,
    }


def scenario_report():
    control = Phy(sf=9)
    # Hypothetical cached schedule: 28-byte envelope + 17-byte proposed beacon base.
    # Enrollment/schedule-transfer/rejoin capacity is NOT represented.
    rows = []
    for clients, sf, payload, retries, ack in product((1, 5, 10, 20, 35, 70), (7, 8, 9),
                                                    (16, 68), (0, 1, 2), ("individual", "bitmap")):
        result = schedule_budget([payload] * clients, Phy(sf=sf), control_phy=control,
                                 beacon_airtime_us=airtime_us(45, control), ack_strategy=ack,
                                 retry_opportunities=retries, superframe_us=10000000)
        # Compact rows; shared assumptions and the full API carry the remaining inputs.
        rows.append({k: result[k] for k in ("clients", "ack_strategy", "ack_packet_bytes",
                    "retry_opportunities", "initial_uplink_us", "initial_ack_us", "retry_airtime_us",
                    "total_uplink_us", "total_ack_us", "known_occupancy_floor_us", "feasibility")} |
                    {"sf": sf, "application_bytes": payload, "data_us": result["per_client_data_us"][0]})
    return {"classification": "host-only analytical worst-case reservation, not physical capacity",
            "assumptions": {"superframe_us": 10000000, "beacon_bytes": 45,
                "beacon_airtime_us": airtime_us(45, control), "control_phy": asdict(control),
                "data_bw_hz": 125000, "coding_rate_denominator": 5, "preamble_symbols": 8,
                "explicit_header": True, "crc": True, "switch_us": None, "processing_us": None,
                "guard_us": None, "total_occupancy_us": None, "remaining_us": None,
                "schedule": "already cached; distribution/rejoin traffic excluded",
                "retries": "all clients and ACKs reserved again in each retry round"},
            "scenario_count": len(rows), "scenarios": rows}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenarios", action="store_true")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--clients", type=int, default=1)
    parser.add_argument("--payload-bytes", type=int, default=16)
    parser.add_argument("--sf", type=int, default=7)
    parser.add_argument("--bandwidth-hz", type=int, default=125000)
    parser.add_argument("--coding-rate", type=int, default=5)
    parser.add_argument("--preamble", type=int, default=8)
    parser.add_argument("--implicit-header", action="store_true")
    parser.add_argument("--no-crc", action="store_true")
    parser.add_argument("--ack-strategy", choices=("individual", "bitmap"), default="bitmap")
    parser.add_argument("--retry-opportunities", type=int, default=0)
    for name in ("beacon-airtime-us", "switch-us", "processing-us", "guard-us"):
        parser.add_argument("--" + name, type=int)
    parser.add_argument("--superframe-us", type=int, default=10000000)
    args = parser.parse_args(argv)
    try:
        if args.scenarios:
            result = scenario_report()
        else:
            integer("clients", args.clients, 1, 70)
            result = schedule_budget([args.payload_bytes] * args.clients,
                Phy(args.sf, args.bandwidth_hz, args.coding_rate, args.preamble,
                    not args.implicit_header, not args.no_crc),
                beacon_airtime_us=args.beacon_airtime_us, ack_strategy=args.ack_strategy,
                retry_opportunities=args.retry_opportunities, switch_us=args.switch_us,
                processing_us=args.processing_us, guard_us=args.guard_us, superframe_us=args.superframe_us)
    except ValueError as error:
        parser.error(str(error))
    output = json.dumps(result, separators=(",", ":")) + "\n"
    if args.out:
        args.out.write_text(output, encoding="utf-8", newline="\n")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
