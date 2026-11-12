"""Capture two serial ports without flashing or changing configuration.

Run with the PlatformIO virtual-environment Python (includes pyserial).
Opening a serial port can toggle DTR/RTS on some boards; record any reset.
"""
import argparse
import json
import threading
import time
from datetime import datetime, timezone
from pathlib import Path

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gateway", required=True, help="Gateway COM port")
    parser.add_argument("--client", required=True, help="Client COM port")
    parser.add_argument("--seconds", type=int, default=1200)
    parser.add_argument("--out", type=Path, required=True, help="New run directory")
    args = parser.parse_args()
    if args.gateway.lower() == args.client.lower() or args.seconds < 1:
        parser.error("Use distinct ports and a positive capture duration")
    args.out.mkdir(parents=True, exist_ok=False)
    stop = threading.Event()
    errors = []
    counts = {"gateway": 0, "client": 0}
    started = datetime.now(timezone.utc).isoformat()

    def capture(role, port):
        try:
            with serial.Serial(port, 115200, timeout=0.2, write_timeout=1) as link:
                with (args.out / f"{role}.log").open("wb") as output:
                    link.write(b"status\n")  # Read-only diagnostics; no config changes.
                    while not stop.is_set():
                        chunk = link.read(4096)
                        if chunk:
                            output.write(chunk)
                            output.flush()
                            counts[role] += len(chunk)
        except Exception as exc:  # Preserve port/file failures in the test record.
            errors.append(f"{role}: {type(exc).__name__}: {exc}")
            stop.set()

    workers = [threading.Thread(target=capture, args=(role, port)) for role, port in
               (("gateway", args.gateway), ("client", args.client))]
    for worker in workers:
        worker.start()
    try:
        stop.wait(args.seconds)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        for worker in workers:
            worker.join()
        record = {"started_utc": started, "ended_utc": datetime.now(timezone.utc).isoformat(),
                  "gateway_port": args.gateway, "client_port": args.client,
                  "requested_seconds": args.seconds, "bytes_captured": counts, "errors": errors,
                  "hardware_measurements": "NOT MEASURED by this capture utility",
                  "acceptance": "TBD: analyze logs and complete RUN_RECORD.md"}
        (args.out / "capture.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    for error in errors:
        print(error)
    print(f"Capture saved to {args.out.resolve()}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
