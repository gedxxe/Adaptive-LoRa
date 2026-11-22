"""Budget the fixed-SF calendar or the eight-Client adaptive calendar."""
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
ADAPTIVE_PERIOD = 15_000_000
ADAPTIVE_GUARD = 80_000
ADAPTIVE_TAIL = 300_000
CONTROL_PHY = Phy(sf=9)


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


def mixed_sf_budget(profiles, size):
    """Reserve the full adaptive calendar, including modeled SF11/12 options."""
    count = len(profiles)
    if not 1 <= count <= MAX_IMPLEMENTED or not 0 <= size <= CAP - HEADER - 4:
        raise ValueError("expected 1–8 clients and 0–64 payload bytes")
    beacon = airtime_us(HEADER + 5 + 3 * count, CONTROL_PHY)
    requests = count * airtime_us(HEADER + 9, CONTROL_PHY)
    grant = airtime_us(HEADER + 1 + 6 * count, CONTROL_PHY)
    feedback = 4 * airtime_us(HEADER + 3 + 4 * count, CONTROL_PHY)
    gateway_tx = beacon + grant + feedback
    control_span = requests + gateway_tx + (count + 11) * ADAPTIVE_GUARD
    initial = [airtime_us(HEADER + 4 + size, Phy(sf=sf)) for sf in profiles]
    first_span = sum(initial) + count * ADAPTIVE_GUARD
    retry_slots = min(count, 4)
    retry_span = 3 * (sum(sorted(initial, reverse=True)[:retry_slots]) + retry_slots * ADAPTIVE_GUARD)
    reserved = control_span + first_span + retry_span + ADAPTIVE_TAIL
    return {"clients": count, "payload_ceiling_bytes": size,
            "gateway_tx_us": gateway_tx, "control_with_guards_us": control_span,
            "primary_data_with_guards_us": first_span, "retry_data_with_guards_us": retry_span,
            "reserved_us": reserved, "headroom_us": ADAPTIVE_PERIOD - reserved}


def adaptive_budget(count, size, sf9):
    if not 0 <= sf9 <= count <= MAX_IMPLEMENTED:
        raise ValueError("invalid roster or SF9 count")
    return {**mixed_sf_budget([9] * sf9 + [7] * (count - sf9), size), "sf9_clients": sf9}


def extended_sf_rows():
    """Eight 16-byte payload ceilings, SF9 control; vary DATA SF only."""
    rows = []
    for sf in range(7, 13):
        headroom = [mixed_sf_budget([sf] * n + [7] * (8 - n), 16)["headroom_us"] for n in range(9)]
        rows.append({"sf": sf, "data_air_us": airtime_us(HEADER + 4 + 16, Phy(sf=sf)),
                     "headroom_one_us": headroom[1], "headroom_all_us": headroom[8],
                     "max_at_sf_with_others_at_sf7": max(n for n, spare in enumerate(headroom) if spare >= 0)})
    return rows


def adaptive_report():
    cases = []
    for count in (4, 6, 8):
        for size in (16, 24, 32, 64):
            rows = [adaptive_budget(count, size, sf9) for sf9 in range(count + 1)]
            cases.append({"all_sf7": rows[0], "most_sf9_that_fit": next(r for r in reversed(rows)
                          if r["headroom_us"] >= 0), "all_sf9": rows[-1]})
    return {"evidence": "analytical full-roster reservation; not measured", "period_us": ADAPTIVE_PERIOD,
            "guard_us": ADAPTIVE_GUARD, "tail_us": ADAPTIVE_TAIL, "cases": cases,
            "eight_client_sf_sweep": extended_sf_rows()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--adaptive", action="store_true", help="Run the adaptive SF7/SF9 capacity sweep")
    args = parser.parse_args()
    details = adaptive_report() if args.adaptive else report()
    if args.out:
        args.out.write_text(json.dumps(details, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Calculated {len(details['cases'])} analytical cases")
    if args.adaptive:
        print("clients ceiling_bytes max_sf9 reserved_us headroom_us")
        for case in details["cases"]:
            row = case["most_sf9_that_fit"]
            print(*(row[key] for key in ("clients", "payload_ceiling_bytes", "sf9_clients", "reserved_us", "headroom_us")))
        print("8 x 16 B, control SF9; SF11-12 model only")
        rows = details["eight_client_sf_sweep"]
        print(*rows[0].keys())
        for row in rows:
            print(*row.values())
        return
    for case in details["cases"]:
        if case["guard_us"] == 50_000 and case["payload_bytes_per_client"] == 64:
            print(case["clients"], case["estimated_occupancy_us"], case["minimum_known_occupancy_us"],
                  case["production_admits_roster"])


if __name__ == "__main__":
    main()
