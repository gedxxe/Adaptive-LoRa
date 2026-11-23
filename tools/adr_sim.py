"""Seeded single-Client channel experiment using the firmware ADR policy."""

import argparse
import csv
import io
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def compile_model(compiler, directory):
    program = Path(directory) / ("adr_sim.exe" if os.name == "nt" else "adr_sim")
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O2",
                    "-I", str(ROOT / "src"), "-I", str(ROOT / "include"),
                    str(ROOT / "src/app/adr_policy.cpp"), str(ROOT / "tools/adr_sim_driver.cpp"),
                    "-o", str(program)], check=True)
    return program


def simulate(program, seed, trials, frames, policy_snr_shift_db=0.0):
    output = subprocess.check_output([program, str(seed), str(trials), str(frames), str(policy_snr_shift_db)], text=True)
    rows = []
    for row in csv.DictReader(io.StringIO(output)):
        rows.append({key: value if key in ("rule", "scenario") else
                     int(value) if key in ("attempt_loss_pct", "reacted_trials") else
                     float(value) if value else None for key, value in row.items()})
    return {
        "mode": "MODEL-ONLY",
        "seed": seed,
        "trials_per_cell": trials,
        "frames_per_trial": frames,
        "policy_snr_shift_db": policy_snr_shift_db,
        "assumptions": [
            "One Client, ideal scheduling and SF9 control, 15 s frames, up to four DATA attempts.",
            "Independent per-attempt interference and logistic decoding around provisional SNR_50.",
            "True SNR starts at -2 dB. A 9 dB blockage occupies the middle third of blockage trials.",
            "Channel SNR_50 is independent of policy tuning. The reference SF meets the channel SNR_50 at the true SNR.",
            "Blockage starts from SF7 and a fresh policy state for every rule.",
            "Reaction is the first scheduled SF at or above the channel reference after blockage begins.",
        ],
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--trials", type=int, default=128)
    parser.add_argument("--frames", type=int, default=480)
    parser.add_argument("--policy-snr-shift-db", type=float, default=0.0)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    if not (0 <= args.seed <= 0xFFFFFFFF and 1 <= args.trials <= 1000 and 6 <= args.frames <= 10000):
        parser.error("expected a 32-bit seed, 1–1000 trials and 6–10000 frames")
    if not math.isfinite(args.policy_snr_shift_db):
        parser.error("the policy SNR shift must be finite")
    try:
        with tempfile.TemporaryDirectory() as directory:
            program = compile_model(args.cxx, directory)
            print(json.dumps(simulate(program, args.seed, args.trials, args.frames, args.policy_snr_shift_db),
                             indent=2, allow_nan=False))
    except (OSError, subprocess.CalledProcessError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
