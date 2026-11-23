import os
import shutil
import tempfile
import unittest

from adr_sim import compile_model, simulate


@unittest.skipUnless(shutil.which(os.environ.get("CXX", "g++")), "C++ compiler required for firmware policy")
class AdrSimTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        cls.program = compile_model(os.environ.get("CXX", "g++"), directory.name)

    def test_seeded_reports_cover_the_full_sweep(self):
        first = simulate(self.program, 42, 4, 90)
        self.assertEqual(first, simulate(self.program, 42, 4, 90))
        self.assertEqual("MODEL-ONLY", first["mode"])
        rows = first["rows"]
        self.assertEqual({(scenario, rate, rule) for scenario in ("interference", "blockage")
                          for rate in (1, 3, 5, 10, 20) for rule in ("LEGACY", "MARGIN_EWMA", "MARGIN_KALMAN")},
                         {(r["scenario"], r["attempt_loss_pct"], r["rule"]) for r in rows})
        self.assertTrue(all(0 <= r["final_loss_pct"] <= 100 and 0 <= r["above_required_pct"] <= 100 for r in rows))
        self.assertTrue(all(r["mean_reaction_s"] is None for r in rows if r["scenario"] == "interference"))
        self.assertTrue(all(1 <= r["reacted_trials"] <= 4 for r in rows if r["scenario"] == "blockage"))


if __name__ == "__main__":
    unittest.main()
