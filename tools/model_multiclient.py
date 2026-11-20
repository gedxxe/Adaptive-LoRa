"""Budget the v3 calendar and extrapolate control sizes beyond its roster cap."""
import argparse
import json
from pathlib import Path

from tdma_feasibility import Phy, airtime_us


HEADER = 28
CAP = 96
MAX_IMPLEMENTED = 8
PERIOD = 10_000_000
TARGET = 1_000_000
PHY = Phy()


def budget(count, size, guard):
    retry_slots = min(count, 4)
    mask_bytes = (count + 7) // 8
    lengths = {
        "beacon": HEADER + 5 + 3 * count,
        "grant": HEADER + 1 + 5 * count,
        "feedback": HEADER + 1 + 2 * mask_bytes + 4 * count,
        "request": HEADER + 5,
        "data": HEADER + 4 + size,
    }
    air = {name: airtime_us(length, PHY) if length <= 255 else None for name, length in lengths.items()}
    data = count * air["data"]
    retry = 3 * retry_slots * air["data"]
    guard_total = (11 + 2 * count + 3 * retry_slots) * guard
    known = data + retry + guard_total + count * air["request"]
    known += (air["beacon"] or 0) + (air["grant"] or 0) + 4 * (air["feedback"] or 0)
    occupancy = known if None not in air.values() else None
    wire_fit = max(lengths.values()) <= CAP
    return {
        "clients": count, "payload_bytes_per_client": size, "guard_us": guard,
        "packet_bytes": lengths, "beacon_airtime_us": air["beacon"],
        "request_phase_airtime_us": count * air["request"], "grant_airtime_us": air["grant"],
        "primary_data_airtime_us": data, "total_data_airtime_us": data + retry,
        "aggregate_ack_airtime_us": None if air["feedback"] is None else 4 * air["feedback"],
        "guard_overhead_us": guard_total, "reserved_retry_data_airtime_us": retry,
        "reserved_retry_rounds_us": None if air["feedback"] is None else
        retry + 3 * (air["feedback"] + (retry_slots + 2) * guard),
        "minimum_known_occupancy_us": known, "estimated_occupancy_us": occupancy,
        "ten_second_overrun_lower_bound_us": max(0, known - PERIOD),
        "one_second_target_met": bool(wire_fit and occupancy is not None and occupancy <= TARGET),
        "fits_ten_seconds_and_wire": bool(wire_fit and occupancy is not None and occupancy <= PERIOD),
        "production_admits_roster": bool(count <= MAX_IMPLEMENTED and wire_fit and occupancy is not None and occupancy <= PERIOD),
    }


def report():
    assumptions = {
        "evidence": "analytical reservation, no measured timing or loss",
        "period_us": PERIOD, "target_us": TARGET, "phy": "SF7/BW125/CR4/5, explicit header and CRC, preamble 8",
        "rounds": 4, "shared_retry_slots_per_round": 4, "implemented_roster_cap": MAX_IMPLEMENTED,
        "description": "All listed clients request once per frame. Slots use exact equal lengths per scenario. "
                       "Feedback reserves four cumulative bitmaps. Rows above eight extrapolate v3 with wider masks "
                       "and do not activate larger rosters. Frames over 255 bytes have no LoRa packet airtime in this model.",
    }
    cases = []
    for guard in (25_000, 50_000, 75_000):
        for size in (8, 32, 64):
            max_wire = max(n for n in range(1, 71) if budget(n, size, guard)["fits_ten_seconds_and_wire"])
            for count in (1, 2, 4, 8, 10, 20, 70):
                case = budget(count, size, guard)
                case["max_implemented_admitted_within_10s"] = min(MAX_IMPLEMENTED, max_wire)
                case["hypothetical_wire_limited_clients_within_10s"] = max_wire
                case["configured_clients_rejected"] = 0 if case["production_admits_roster"] else count
                cases.append(case)
    return {"assumptions": assumptions, "cases": cases}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    details = report()
    if args.out:
        args.out.write_text(json.dumps(details, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Calculated {len(details['cases'])} analytical cases")
    for case in details["cases"]:
        if case["guard_us"] == 50_000 and case["payload_bytes_per_client"] == 64:
            print(case["clients"], case["estimated_occupancy_us"], case["minimum_known_occupancy_us"],
                  case["production_admits_roster"])


if __name__ == "__main__":
    main()
