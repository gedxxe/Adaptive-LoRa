import unittest

from model_multiclient import budget, report


class MulticlientBudgetTests(unittest.TestCase):
    def test_bench_roster_matches_firmware_reservation(self):
        case = budget(8, 64, 50_000)
        self.assertEqual(6_515_504, case["estimated_occupancy_us"])
        self.assertTrue(case["production_admits_roster"])
        self.assertFalse(case["one_second_target_met"])

    def test_extrapolation_does_not_admit_large_rosters(self):
        ten = budget(10, 64, 50_000)
        self.assertTrue(ten["fits_ten_seconds_and_wire"])
        self.assertFalse(ten["production_admits_roster"])
        twenty = budget(20, 64, 50_000)
        self.assertEqual(998_448, twenty["ten_second_overrun_lower_bound_us"])
        seventy = budget(70, 64, 50_000)
        self.assertIsNone(seventy["estimated_occupancy_us"])
        self.assertEqual(379, seventy["packet_bytes"]["grant"])

    def test_guard_and_payload_sweep(self):
        cases = report()["cases"]
        self.assertEqual(63, len(cases))
        self.assertTrue(all(case["max_implemented_admitted_within_10s"] == 8 for case in cases))
        self.assertLess(budget(8, 32, 25_000)["estimated_occupancy_us"], budget(8, 64, 75_000)["estimated_occupancy_us"])


if __name__ == "__main__":
    unittest.main()
