import unittest

from adr_shadow import Policy, evaluate


def sample(frame, outcome="first", snr="6", session=1, client=2, sf=7):
    rssi = "N/A" if outcome == "missed" else "-90"
    return (f"event=V3_LINK_SAMPLE,network_id=1,gateway_id=1,gateway_boot={session},frame_id={frame},"
            f"source_id={client},client_boot=100,sf={sf},outcome={outcome},app=accepted,rssi_dbm={rssi},snr_db={snr}")


class AdrShadowTests(unittest.TestCase):
    def setUp(self):
        self.policy = Policy(2, 2, 2, 1, -5, 2)

    def test_weak_windows_raise_then_healthy_windows_clear_the_candidate(self):
        weak = [sample(i, "missed", "N/A") for i in range(1, 5)]
        healthy = [sample(i) for i in range(5, 9)]
        self.assertEqual([7, 8, 8, 7], [row["candidate_sf"] for row in evaluate(weak + healthy, self.policy)])

    def test_low_snr_counts_even_when_all_data_arrive(self):
        rows = list(evaluate([sample(i, snr="-8", sf=9) for i in range(1, 5)], self.policy))
        self.assertEqual([9, 9], [row["candidate_sf"] for row in rows])
        self.assertEqual(-8, rows[-1]["mean_snr_db"])
        self.assertEqual(-90, rows[-1]["mean_rssi_dbm"])

    def test_clients_and_reboot_sessions_keep_separate_windows(self):
        lines = [sample(1), sample(1, session=2), sample(2, client=3), sample(2)]
        self.assertEqual(1, len(list(evaluate(lines, self.policy))))

    def test_missing_capture_lines_do_not_imply_packet_loss(self):
        rows = list(evaluate([sample(1), sample(1), sample(20)], self.policy))
        self.assertEqual(1, rows[0]["first_pass"])
        self.assertEqual(20, rows[0]["last_frame"])

    def test_invalid_metrics_and_conflicting_observations_are_rejected(self):
        for lines in ([sample(1, snr="nan")], [sample(1), sample(1, snr="0")]):
            with self.subTest(lines=lines), self.assertRaises(ValueError):
                list(evaluate(lines, self.policy))


if __name__ == "__main__":
    unittest.main()
