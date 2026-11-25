# SPDX-License-Identifier: GPL-3.0-only
"""Run the rotating v7 scheduler on a seeded shared-channel model."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--frames", type=int, default=5760)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sources = ["tools/secure_rotation_model.cpp", "src/app/adr_policy.cpp",
               "src/common/protocol/multiclient_protocol.cpp", "src/common/protocol/scheduled_protocol.cpp",
               "src/common/protocol/frame_codec.cpp"]
    with tempfile.TemporaryDirectory(prefix="adaptive-lora-model-") as directory:
        program = Path(directory) / ("rotation.exe" if os.name == "nt" else "rotation")
        subprocess.run([
            os.environ.get("CXX", "g++"), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-Iinclude", "-Isrc", "-DADAPTIVE_LORA_LINK_ADAPTATION=1", "-DADAPTIVE_LORA_ROTATING=1",
            "-DADAPTIVE_LORA_SECURE=1", *sources, "-o", str(program),
        ], cwd=root, check=True)
        subprocess.run([str(program), str(args.seed), str(args.frames)], check=True)


if __name__ == "__main__":
    main()
