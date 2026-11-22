import unittest

from model_multiclient import adaptive_budget, adaptive_report, budget, extended_sf_rows, mixed_sf_budget, report


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

    def test_adaptive_matrix_and_bench_headroom(self):
        cases = adaptive_report()["cases"]
        self.assertEqual([4, 4, 4, 4, 6, 6, 6, 3, 8, 8, 5, 2],
                         [case["most_sf9_that_fit"]["sf9_clients"] for case in cases])
        bench = adaptive_budget(8, 16, 8)
        self.assertEqual(14_083_936, bench["reserved_us"])
        self.assertEqual(916_064, bench["headroom_us"])
        self.assertEqual(2_361_344, bench["gateway_tx_us"])
        self.assertEqual(15_634_272, adaptive_budget(8, 64, 3)["reserved_us"])

    def test_adaptive_no_extrapolation_past_wire_limit(self):
        for values in ((70, 16, 0), (8, 65, 0), (4, 16, 5)):
            with self.subTest(values=values), self.assertRaises(ValueError):
                adaptive_budget(*values)

    def test_extended_sf_includes_ldro_and_shared_retries(self):
        rows = extended_sf_rows()
        self.assertEqual([97_536, 174_592, 308_224, 575_488, 1_232_896, 2_301_952],
                         [row["data_air_us"] for row in rows])
        self.assertEqual([8, 8, 8, 2, 1, 0], [row["max_at_sf_with_others_at_sf7"] for row in rows])
        self.assertEqual(-3_687_840, rows[-1]["headroom_one_us"])
        self.assertEqual(-152_992, mixed_sf_budget([9] * 7 + [10], 16)["headroom_us"])
        self.assertEqual(mixed_sf_budget([7] * 7 + [12], 16), mixed_sf_budget([12] + [7] * 7, 16))


if __name__ == "__main__":
    unittest.main()
