"""Evaluate candidate spreading factors offline from v3 gateway logs."""

import argparse
from collections import defaultdict
from dataclasses import dataclass
import json
import math
from pathlib import Path
from statistics import mean


EVENT = "event=V3_LINK_SAMPLE,"
IDENTITY = ("network_id", "gateway_id", "gateway_boot", "source_id", "client_boot", "sf")


@dataclass(frozen=True)
class Policy:
    window: int
    weak_windows: int
    healthy_windows: int
    first_pass_target: float
    snr_low: float
    snr_recover: float

    def __post_init__(self):
        if min(self.window, self.weak_windows, self.healthy_windows) < 1:
            raise ValueError("window lengths must be positive")
        if not 0 < self.first_pass_target <= 1:
            raise ValueError("first-pass target must be in (0, 1]")
        if not all(map(math.isfinite, (self.snr_low, self.snr_recover))) or self.snr_low >= self.snr_recover:
            raise ValueError("SNR thresholds must be finite and recovery must exceed the low threshold")


def read_samples(lines):
    sessions = defaultdict(dict)
    for line in lines:
        if EVENT not in line:
            continue
        fields = dict(item.split("=", 1) for item in line.strip().split(EVENT, 1)[1].split(","))
        key = tuple(int(fields[name]) for name in IDENTITY)
        frame = int(fields["frame_id"])
        outcome = fields["outcome"]
        if min(*key, frame) < 1 or key[-1] not in (7, 8, 9) or outcome not in ("first", "retry", "missed"):
            raise ValueError("invalid v3 link sample")
        metrics = tuple(None if fields[name] == "N/A" else float(fields[name]) for name in ("snr_db", "rssi_dbm"))
        if any(value is not None and (not math.isfinite(value) or outcome == "missed") for value in metrics):
            raise ValueError("invalid radio metrics in v3 link sample")
        sample = (outcome, *metrics)
        if sessions[key].setdefault(frame, sample) != sample:
            raise ValueError("conflicting observations for the same frame")
    return sessions


def evaluate(lines, policy):
    for key, frames in sorted(read_samples(lines).items()):
        records = sorted(frames.items())
        active = candidate = key[-1]
        weak = healthy = 0
        for offset in range(0, len(records) - policy.window + 1, policy.window):
            block = records[offset:offset + policy.window]
            observations = [sample for _, sample in block]
            snrs = [snr for _, snr, _ in observations if snr is not None]
            rssis = [rssi for _, _, rssi in observations if rssi is not None]
            snr = mean(snrs) if snrs else None
            first_pass = sum(outcome == "first" for outcome, _, _ in observations) / policy.window
            poor = first_pass < policy.first_pass_target or (snr is not None and snr <= policy.snr_low)
            sound = first_pass >= policy.first_pass_target and snr is not None and snr >= policy.snr_recover
            weak = weak + 1 if poor else 0
            healthy = healthy + 1 if sound else 0
            if weak >= policy.weak_windows:
                candidate = min(active + 1, 9)
            elif healthy >= policy.healthy_windows:
                candidate = active
            yield dict(zip(IDENTITY, key), first_frame=block[0][0], last_frame=block[-1][0],
                       samples=policy.window, first_pass=first_pass, mean_snr_db=snr,
                       mean_rssi_dbm=mean(rssis) if rssis else None, candidate_sf=candidate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gateway_log", type=Path)
    parser.add_argument("--window", type=int, required=True)
    parser.add_argument("--weak-windows", type=int, required=True)
    parser.add_argument("--healthy-windows", type=int, required=True)
    parser.add_argument("--first-pass-target", type=float, required=True)
    parser.add_argument("--snr-low", type=float, required=True)
    parser.add_argument("--snr-recover", type=float, required=True)
    args = parser.parse_args()
    try:
        policy = Policy(args.window, args.weak_windows, args.healthy_windows, args.first_pass_target,
                        args.snr_low, args.snr_recover)
        with args.gateway_log.open(encoding="utf-8-sig") as source:
            rows = list(evaluate(source, policy))
        if not rows:
            parser.error("no complete per-session observation window")
    except (KeyError, OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps({"mode": "offline_advisory", "windows": rows}, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
