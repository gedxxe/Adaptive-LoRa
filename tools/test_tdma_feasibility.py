import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from tdma_feasibility import Phy, airtime_us, main, scenario_report, schedule_budget


class AirtimeTests(unittest.TestCase):
    def test_hand_calculated_packet_vectors(self):
        # SF7/28B: ceil(240/28)=9 coding blocks; (8+4.25+8+45)*1024 us.
        for size, sf, expected in ((28, 7, 66816), (37, 7, 82176), (44, 7, 92416),
                                   (96, 7, 164096), (96, 8, 297472), (96, 9, 533504)):
            with self.subTest(size=size, sf=sf):
                self.assertEqual(expected, airtime_us(size, Phy(sf=sf)))

    def test_low_data_rate_optimization_and_override(self):
        self.assertFalse(Phy(sf=10).effective_ldro)
        self.assertTrue(Phy(sf=11).effective_ldro)
        self.assertFalse(Phy(sf=11, bandwidth_hz=250000).effective_ldro)
        self.assertEqual(1646592, airtime_us(28, Phy(sf=12)))
        self.assertLess(airtime_us(28, Phy(sf=12, ldro=False)), airtime_us(28, Phy(sf=12)))

    def test_header_crc_coding_and_preamble_terms(self):
        # 9B at SF7 crosses a coding block when CRC or explicit header is added.
        self.assertEqual(41216, airtime_us(9, Phy()))
        self.assertEqual(36096, airtime_us(9, Phy(crc=False)))
        self.assertEqual(36096, airtime_us(9, Phy(explicit_header=False)))
        self.assertEqual(53504, airtime_us(9, Phy(coding_rate_denominator=8)))
        self.assertEqual(airtime_us(8, Phy()) + 2048, airtime_us(8, Phy(preamble_symbols=10)))

    def test_zero_and_phy_maximum_lengths(self):
        self.assertEqual(25856, airtime_us(0, Phy()))
        self.assertGreater(airtime_us(255, Phy()), airtime_us(96, Phy()))
        for invalid in (-1, 256, True, 1.5):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                airtime_us(invalid, Phy())

    def test_rejects_unsupported_or_invalid_phy(self):
        for kwargs in ({"sf": 6}, {"sf": 13}, {"sf": True}, {"bandwidth_hz": 100000},
                       {"coding_rate_denominator": 4}, {"preamble_symbols": 0},
                       {"crc": 1}, {"ldro": 1}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                Phy(**kwargs)


class ScheduleTests(unittest.TestCase):
    def budget(self, payloads=(16,), **kwargs):
        return schedule_budget(payloads, Phy(), superframe_us=10000000, **kwargs)

    def test_application_header_and_heterogeneous_payloads(self):
        result = self.budget((0, 16, 68))
        self.assertEqual([66816, 92416, 164096], result["per_client_data_us"])
        self.assertEqual(sum(result["per_client_data_us"]), result["initial_uplink_us"])

    def test_complete_bitmap_size_and_control_phy(self):
        for clients, bitmap, packet in ((1, 1, 31), (10, 2, 32), (70, 9, 39)):
            result = self.budget([0] * clients)
            self.assertEqual(bitmap, result["bitmap_bytes"])
            self.assertEqual(packet, result["ack_packet_bytes"])
            self.assertEqual(airtime_us(packet, Phy(sf=9)), result["initial_ack_us"])
        self.assertEqual(37, self.budget(ack_strategy="individual")["ack_packet_bytes"])

    def test_retry_budget_includes_ack_each_round(self):
        base = self.budget((16, 68), ack_strategy="individual", beacon_airtime_us=123)
        retry = self.budget((16, 68), ack_strategy="individual", beacon_airtime_us=123, retry_opportunities=2)
        self.assertEqual(2 * (base["initial_uplink_us"] + base["initial_ack_us"]), retry["retry_airtime_us"])
        self.assertEqual(3 * base["total_ack_us"], retry["total_ack_us"])
        self.assertEqual(3 * base["total_uplink_us"], retry["total_uplink_us"])
        self.assertEqual(13, retry["gateway_actions"])
        self.assertEqual(6, retry["guard_windows"])

    def test_unknown_is_not_zero_or_a_fit_claim(self):
        result = self.budget()
        self.assertEqual("UNDETERMINED", result["feasibility"])
        self.assertEqual(4, len(result["unknown_inputs"]))
        self.assertIsNone(result["remaining_us"])
        self.assertIsNone(result["total_occupancy_us"])

    def test_airtime_floor_can_disprove_fit_with_unknown_timings(self):
        result = self.budget([68] * 70, retry_opportunities=2)
        self.assertEqual("INFEASIBLE", result["feasibility"])
        self.assertGreater(result["known_occupancy_floor_us"], 10000000)

    def test_explicit_hypothetical_costs_are_all_charged(self):
        result = self.budget((0, 0), beacon_airtime_us=100, switch_us=10, processing_us=20, guard_us=30)
        # beacon + two RX + bitmap TX; two guards.
        expected = 2 * 66816 + 246784 + 100 + 4 * 30 + 2 * 30
        self.assertEqual(expected, result["total_occupancy_us"])
        self.assertEqual(10000000 - expected, result["remaining_us"])
        self.assertEqual("CONDITIONAL_FIT", result["feasibility"])

    def test_exact_superframe_boundary_and_one_microsecond_over(self):
        known = dict(beacon_airtime_us=0, switch_us=0, processing_us=0, guard_us=0)
        cost = self.budget(**known)["total_occupancy_us"]
        for duration, state in ((cost, "CONDITIONAL_FIT"), (cost - 1, "INFEASIBLE")):
            result = schedule_budget([16], Phy(), superframe_us=duration, **known)
            self.assertEqual(state, result["feasibility"])

    def test_rejects_invalid_schedule_inputs(self):
        for payloads in ([], [0] * 71, [-1], [69], [True]):
            with self.subTest(payloads=payloads), self.assertRaises(ValueError):
                self.budget(payloads)
        for kwargs in ({"retry_opportunities": 3}, {"guard_us": -1}, {"switch_us": float("nan")},
                       {"beacon_airtime_us": True}, {"ack_strategy": "none"}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                self.budget(**kwargs)

    def test_scenario_grid_is_complete_and_deterministic(self):
        report = scenario_report()
        self.assertEqual(216, report["scenario_count"])
        self.assertEqual(report, scenario_report())
        keys = {(r["clients"], r["sf"], r["application_bytes"], r["retry_opportunities"], r["ack_strategy"])
                for r in report["scenarios"]}
        self.assertEqual(216, len(keys))
        self.assertEqual({1, 5, 10, 20, 35, 70}, {k[0] for k in keys})
        self.assertNotIn("CONDITIONAL_FIT", {r["feasibility"] for r in report["scenarios"]})

    def test_cli_writes_parseable_report(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.json"
            main(["--scenarios", "--out", str(output)])
            self.assertEqual(scenario_report(), json.loads(output.read_text(encoding="utf-8")))

    def test_cli_rejects_invalid_client_count_before_allocating(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
            main(["--clients", "1000000000"])
        self.assertEqual(2, error.exception.code)


if __name__ == "__main__":
    unittest.main()
