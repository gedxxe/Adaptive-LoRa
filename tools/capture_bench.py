"""Stream one MCU or a radio pair to a new evidence directory (no flashing).

Only status commands are sent unless --no-status is used. Opening a port can
reset some boards despite preconfigured DTR/RTS; finish backup/RF gates first.
Run with PlatformIO's Python, which includes pyserial.
"""
import argparse
import hashlib
import json
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path

import serial

FINAL_STATUS_SECONDS = 1.0
JOIN_SECONDS = 3.0


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def port_key(port):
    # Windows COM names can also be written with the Win32 device prefix.
    name = port.removeprefix("\\\\.\\")
    return name.casefold() if name.upper().startswith("COM") else str(Path(name).resolve())


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gateway", help="Gateway COM/device port (requires --client)")
    parser.add_argument("--client", help="Client COM/device port (requires --gateway)")
    parser.add_argument("--board", help="Single MCU port; mutually exclusive with pair")
    parser.add_argument("--seconds", type=int, default=1200)
    parser.add_argument("--out", type=Path, required=True, help="New run directory")
    parser.add_argument("--metadata", type=Path, help="Operator JSON object, at most 64 KiB")
    parser.add_argument("--no-status", action="store_true", help="Passive capture; send no commands")
    args = parser.parse_args(argv)
    if args.seconds < 1:
        parser.error("Use a positive duration")
    if args.board and not (args.gateway or args.client):
        ports = {"board": args.board}
    elif not args.board and args.gateway and args.client:
        if port_key(args.gateway) == port_key(args.client):
            parser.error("Use distinct ports")
        ports = {"gateway": args.gateway, "client": args.client}
    else:
        parser.error("Select --board OR both --gateway and --client")
    operator = {}
    if args.metadata:
        try:
            with args.metadata.open("rb") as source:
                raw = source.read(65537)
            if len(raw) > 65536:
                raise ValueError("Metadata exceeds 64 KiB")
            operator = json.loads(raw)
            if not isinstance(operator, dict):
                raise ValueError("Metadata must be a JSON object")
        except (OSError, ValueError) as exc:
            parser.error(str(exc))
    try:
        args.out.mkdir(parents=True, exist_ok=False)
    except OSError as exc:
        print(f"Cannot create new capture directory: {exc}", file=sys.stderr)
        return 1

    stop = threading.Event()
    reason = {"value": "RUNNING"}
    streams = {role: {"port": port, "bytes": 0, "chunks": 0, "newlines": 0,
                      "trailing_bytes": 0, "sha256": None, "error": None,
                      "opened_utc": None, "closed_utc": None,
                      "status_requests": [], "raw_file": role + ".log",
                      "chunk_file": role + ".chunks.jsonl"} for role, port in ports.items()}
    record = {"capture_schema": 2, "state": "RUNNING", "started_utc": utc_now(),
              "started_monotonic_ns": time.monotonic_ns(), "requested_seconds": args.seconds,
              "run_id": args.out.name, "streams": streams, "operator_metadata": operator,
              "serial": {"baud": 115200, "read_timeout_s": 0.2, "write_timeout_s": 1,
                         "dtr_before_open": False, "rts_before_open": False,
                         "open_reset_possible": True, "status_only": not args.no_status},
              "python": sys.version.split()[0], "pyserial": serial.VERSION,
              "capture_script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "hardware_measurements": "NOT MEASURED by this capture utility",
              "acceptance": "NOT EVALUATED: analyze logs/window and complete RUN_RECORD.md"}

    def save_record():
        temporary = args.out / "capture.json.tmp"
        temporary.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        temporary.replace(args.out / "capture.json")

    try:
        save_record()  # A hard-killed run retains RUNNING, never a completed claim.
    except OSError as exc:
        print(f"Cannot write initial capture metadata: {exc}", file=sys.stderr)
        return 1

    def capture(role, port):
        state = streams[role]
        link = None
        digest = hashlib.sha256()
        last_newline_end = 0
        try:
            link = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=1)
            link.dtr = False
            link.rts = False
            link.port = port
            link.open()
            state["opened_utc"] = utc_now()
            with (args.out / state["raw_file"]).open("xb") as output, \
                    (args.out / state["chunk_file"]).open("x", encoding="utf-8") as journal:
                def status():
                    written = link.write(b"status\n")
                    state["status_requests"].append({"utc": utc_now(), "written": written})
                    if written != 7:
                        raise OSError("Short status command write")

                def read_chunk():
                    nonlocal last_newline_end
                    chunk = link.read(4096)
                    if not chunk:
                        return
                    observed = {"utc": utc_now(), "monotonic_ns": time.monotonic_ns(),
                                "offset": state["bytes"], "length": len(chunk)}
                    if output.write(chunk) != len(chunk):
                        raise OSError("Short raw-file write")
                    output.flush()
                    journal.write(json.dumps(observed) + "\n")
                    journal.flush()
                    digest.update(chunk)
                    at = chunk.rfind(b"\n")
                    if at >= 0:
                        last_newline_end = state["bytes"] + at + 1
                    state["bytes"] += len(chunk)
                    state["chunks"] += 1
                    state["newlines"] += chunk.count(b"\n")
                    state["trailing_bytes"] = state["bytes"] - last_newline_end

                if not args.no_status:
                    status()
                while not stop.is_set():
                    read_chunk()
                if reason["value"] == "COMPLETE" and not args.no_status:
                    status()
                    until = time.monotonic() + FINAL_STATUS_SECONDS
                    while time.monotonic() < until:
                        read_chunk()
        except Exception as exc:
            state["error"] = f"{type(exc).__name__}: {exc}"
            stop.set()
        finally:
            state["sha256"] = digest.hexdigest()
            if link is not None:
                try:
                    link.close()
                except Exception as exc:
                    state["error"] = f"Close {type(exc).__name__}: {exc}"
                    stop.set()
            state["closed_utc"] = utc_now()

    workers = {role: threading.Thread(target=capture, args=(role, port), daemon=True)
               for role, port in ports.items()}
    try:
        for worker in workers.values():
            worker.start()
        reason["value"] = "ERROR" if stop.wait(args.seconds) else "COMPLETE"
    except KeyboardInterrupt:
        reason["value"] = "INTERRUPTED"
    finally:
        record["measurement_stop_utc"] = utc_now()
        record["measurement_stop_monotonic_ns"] = time.monotonic_ns()
        stop.set()
        for worker in workers.values():
            worker.join(JOIN_SECONDS)
        alive = [role for role, worker in workers.items() if worker.is_alive()]
        errors = [f"{role}: {state['error']}" for role, state in streams.items() if state["error"]]
        errors += [f"{role}: worker did not close within shutdown budget" for role in alive]
        record.update(state="ERROR" if errors else reason["value"], errors=errors,
                      unjoined_workers=alive, ended_utc=utc_now(), ended_monotonic_ns=time.monotonic_ns())
        record["quality_flags"] = [f"{role}: {flag}" for role, state in streams.items()
                                   for flag, present in (("empty_stream", not state["bytes"]),
                                                         ("partial_final_line", state["trailing_bytes"] > 0)) if present]
        if alive:
            record["quality_flags"].append("Unjoined writers: hashes/lengths are not finalized")
        try:
            save_record()
        except OSError as exc:
            print(f"Cannot finalize capture metadata: {exc}", file=sys.stderr)
            return 1
    for error in record["errors"]:
        print(error, file=sys.stderr)
    print(f"Capture {record['state']}: {args.out}")
    if record["state"] == "ERROR":
        return 1
    return 130 if record["state"] == "INTERRUPTED" else 0


if __name__ == "__main__":
    raise SystemExit(main())
