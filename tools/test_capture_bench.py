"""Acceptance-requirement tests for host capture only; never opens real ports."""
import contextlib
import hashlib
import io
import json
import tempfile
import threading
import time
import types
import unittest
from pathlib import Path
from unittest.mock import patch

import capture_bench as capture


class Port:
    def __init__(self, owner, **kwargs):
        self.owner = owner
        self.kwargs = kwargs
        self.port = None
        self.dtr = self.rts = True
        self.writes = []
        self.closed = threading.Event()
        self.data = owner.data
        self.offset = 0
        self.reads = 0

    def open(self):
        self.owner.opened.append(self)
        if self.port.startswith("MISSING"):
            raise OSError("Simulated missing port")
        self.owner.lines.append((self.dtr, self.rts))

    def write(self, data):
        self.writes.append(data)
        return len(data) - 1 if self.owner.short_write else len(data)

    def read(self, size):
        self.owner.read_sizes.append(size)
        self.owner.entered.set()
        if self.owner.block:
            self.owner.release.wait(2)
        if self.owner.disconnect and self.reads:
            raise OSError("Simulated disconnect")
        self.reads += 1
        result = self.data[self.offset:self.offset + size]
        self.offset += len(result)
        if not result:
            time.sleep(0.001)
        return result

    def close(self):
        self.closed.set()


class Backend:
    def __init__(self):
        self.data = b"event=STATUS,log_dropped=0\n"
        self.opened, self.lines, self.read_sizes = [], [], []
        self.short_write = self.disconnect = self.block = False
        self.entered, self.release = threading.Event(), threading.Event()

    def Serial(self, **kwargs):
        return Port(self, **kwargs)


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="adaptive-lora-capture-test-")
        self.directory = Path(self.temp.name)
        self.out = self.directory / "run"
        self.backend = Backend()
        self.interrupt = False
        backend = self.backend

        def event():
            value = threading.Event()
            wait = value.wait

            def bounded_wait(seconds):
                if backend.block:
                    backend.entered.wait(0.5)
                result = wait(min(seconds, 0.03))
                if self.interrupt:
                    raise KeyboardInterrupt
                return result
            value.wait = bounded_wait
            return value

        self.patches = [patch.object(capture, "serial", types.SimpleNamespace(Serial=backend.Serial, VERSION="fake")),
                        patch.object(capture, "threading", types.SimpleNamespace(Event=event, Thread=threading.Thread)),
                        patch.object(capture, "FINAL_STATUS_SECONDS", 0.01), patch.object(capture, "JOIN_SECONDS", 0.1)]
        for p in self.patches:
            p.start()

    def tearDown(self):
        self.backend.release.set()
        for port in self.backend.opened:
            self.assertTrue(port.closed.wait(1), "Fake port did not close")
        for p in reversed(self.patches):
            p.stop()
        self.temp.cleanup()

    def run_capture(self, args=None):
        args = args if args is not None else ["--board", "FAKE_BOARD"]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return capture.main(args + ["--seconds", "1", "--out", str(self.out)])

    def record(self):
        return json.loads((self.out / "capture.json").read_text())

    def test_pair_raw_timestamps_hashes_and_preopen_modem_lines(self):
        self.assertEqual(0, self.run_capture(["--gateway", "FAKE_GW", "--client", "FAKE_CLIENT"]))
        record = self.record()
        self.assertEqual("COMPLETE", record["state"])
        self.assertEqual({"gateway", "client"}, set(record["streams"]))
        self.assertEqual([(False, False), (False, False)], self.backend.lines)
        for port in self.backend.opened:
            self.assertEqual([b"status\n", b"status\n"], port.writes)
            self.assertIsNone(port.kwargs["port"])
        for role, state in record["streams"].items():
            raw = (self.out / (role + ".log")).read_bytes()
            self.assertEqual(self.backend.data, raw)
            self.assertEqual(hashlib.sha256(raw).hexdigest(), state["sha256"])
            rows = [json.loads(l) for l in (self.out / (role + ".chunks.jsonl")).read_text().splitlines()]
            self.assertEqual(0, rows[0]["offset"])
            self.assertEqual(len(raw), sum(r["length"] for r in rows))
            self.assertIn("+00:00", rows[0]["utc"])
            self.assertGreaterEqual(rows[0]["monotonic_ns"], record["started_monotonic_ns"])
            self.assertEqual(0, state["trailing_bytes"])

    def test_single_board_passive_sends_no_bytes(self):
        self.assertEqual(0, self.run_capture(["--board", "FAKE_BOARD", "--no-status"]))
        self.assertEqual({"board"}, set(self.record()["streams"]))
        self.assertEqual([], self.backend.opened[0].writes)

    def test_interrupt_is_incomplete_exit130(self):
        self.interrupt = True
        self.assertEqual(130, self.run_capture())
        self.assertEqual("INTERRUPTED", self.record()["state"])
        self.assertEqual([b"status\n"], self.backend.opened[0].writes)

    def test_missing_port_stops_pair_with_error(self):
        self.assertEqual(1, self.run_capture(["--gateway", "FAKE_GW", "--client", "MISSING_CLIENT"]))
        self.assertEqual("ERROR", self.record()["state"])
        self.assertIn("Simulated missing port", " ".join(self.record()["errors"]))

    def test_disconnect_preserves_partial_bytes_and_error(self):
        self.backend.disconnect = True
        self.assertEqual(1, self.run_capture())
        self.assertEqual("ERROR", self.record()["state"])
        self.assertEqual(self.backend.data, (self.out / "board.log").read_bytes())
        self.assertIn("Simulated disconnect", " ".join(self.record()["errors"]))

    def test_long_partial_line_is_streamed_with_4096_byte_reads(self):
        self.backend.data = b"first\n" + bytes([255]) + b"x" * 12000
        self.assertEqual(0, self.run_capture(["--board", "FAKE_BOARD", "--no-status"]))
        state = self.record()["streams"]["board"]
        self.assertEqual(self.backend.data, (self.out / "board.log").read_bytes())
        self.assertEqual(12001, state["trailing_bytes"])
        self.assertEqual(1, state["newlines"])
        self.assertEqual({4096}, set(self.backend.read_sizes))
        self.assertIn("board: partial_final_line", self.record()["quality_flags"])
        rows = [json.loads(l) for l in (self.out / "board.chunks.jsonl").read_text().splitlines()]
        offset = 0
        for row in rows:
            self.assertEqual(offset, row["offset"])
            offset += row["length"]
        self.assertEqual(len(self.backend.data), offset)

    def test_distinct_port_check_understands_win32_alias(self):
        with self.assertRaises(SystemExit) as result:
            self.run_capture(["--gateway", "COM4", "--client", "\\\\.\\com4"])
        self.assertEqual(2, result.exception.code)
        self.assertEqual([], self.backend.opened)

    def test_existing_directory_is_not_overwritten(self):
        self.out.mkdir()
        marker = self.out / "marker"
        marker.write_text("preserve")
        self.assertEqual(1, self.run_capture())
        self.assertEqual("preserve", marker.read_text())
        self.assertEqual([], self.backend.opened)

    def test_operator_metadata_is_bounded_and_validated(self):
        meta = self.directory / "metadata.json"
        meta.write_text("[]")
        with self.assertRaises(SystemExit):
            self.run_capture(["--board", "FAKE_BOARD", "--metadata", str(meta)])
        meta.write_bytes(b" " * 65537)
        with self.assertRaises(SystemExit):
            self.run_capture(["--board", "FAKE_BOARD", "--metadata", str(meta)])
        self.assertEqual([], self.backend.opened)
        meta.write_text('{"fixture":"synthetic"}')
        self.assertEqual(0, self.run_capture(["--board", "FAKE_BOARD", "--metadata", str(meta)]))
        self.assertEqual({"fixture": "synthetic"}, self.record()["operator_metadata"])

    def test_short_status_write_is_error(self):
        self.backend.short_write = True
        self.assertEqual(1, self.run_capture())
        self.assertIn("Short status command write", " ".join(self.record()["errors"]))

    def test_initial_metadata_failure_never_opens_ports(self):
        with patch.object(Path, "write_text", side_effect=OSError("Simulated disk error")):
            self.assertEqual(1, self.run_capture())
        self.assertEqual([], self.backend.opened)

    def test_unjoined_reader_is_incomplete_not_success(self):
        self.backend.block = True
        self.assertEqual(1, self.run_capture(["--board", "FAKE_BOARD", "--no-status"]))
        self.assertEqual("ERROR", self.record()["state"])
        self.assertEqual(["board"], self.record()["unjoined_workers"])
        self.assertIn("Unjoined writers: hashes/lengths are not finalized", self.record()["quality_flags"])
        self.backend.release.set()

    def test_final_metadata_failure_leaves_running_marker(self):
        original = Path.replace
        calls = 0

        def replace(path, target):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("Simulated final metadata failure")
            return original(path, target)
        with patch.object(Path, "replace", replace):
            self.assertEqual(1, self.run_capture())
        self.assertEqual("RUNNING", self.record()["state"])


if __name__ == "__main__":
    unittest.main()
