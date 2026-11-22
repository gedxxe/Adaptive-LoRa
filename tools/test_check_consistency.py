from pathlib import Path
import shutil
import tempfile
import unittest

from check_consistency import ADR_FIGURE, ALGORITHM_FIGURES, APPROVED_SETTINGS, LINK_DOCS, check, current_text


class ConsistencyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        source = Path(__file__).resolve().parents[1]
        # Empty targets suffice for link existence checks; copy only inspected inputs.
        for folder in ("docs", "tools", "src", "include", "test"):
            for original in (source / folder).rglob("*"):
                if original.is_file() and original.suffix in (".md", ".py", ".svg", ".png", ".cpp", ".h", ".tex"):
                    target = self.root / original.relative_to(source)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.touch()
        names = set(LINK_DOCS) | set(APPROVED_SETTINGS) | set(ALGORITHM_FIGURES) | {ADR_FIGURE, "CODEX_START_PROMPT.md", "VERSION", "PROTOCOL_VERSION",
                "platformio.ini", "THIRD_PARTY.md", "CHANGELOG.md", "include/project_config.h", "include/research_roster.h",
                "src/common/radio/sx1262_hardware.h", "src/app/esp32_firmware.h",
                "logs/README.md", "logs/test/RUN_RECORD_TEMPLATE.md"}
        for name in names:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / name, target)

    def mutate(self, name, old, new):
        path = self.root / name
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def test_current_repository_passes(self):
        self.assertEqual([], check(self.root))

    def test_history_cannot_hide_wrong_current_version(self):
        version = (self.root / "VERSION").read_text().strip()
        self.mutate("docs/en/PROGRESS.md", f"`{version}`", "`9.9.9-alpha.9`")
        self.assertTrue(any("current firmware" in e for e in check(self.root)))
        protocol = APPROVED_SETTINGS["src/common/protocol/multiclient_protocol.h"]["kVersion"]
        self.mutate("docs/zh-CN/PROGRESS.md", f"`heltec_wifi_lora_32_V3_multiclient` | {protocol} |",
                    "`heltec_wifi_lora_32_V3_multiclient` | 99 |")
        self.assertTrue(any("comparison wire versions differ" in e for e in check(self.root)))

    def test_bad_build_environment_is_reported(self):
        self.mutate("docs/en/BENCH_RUNBOOK.md", "pio run -e heltec_wifi_lora_32_V3", "pio run -e absent_board")
        self.assertTrue(any("adaptive upload" in e for e in check(self.root)))

    def test_default_build_requires_display(self):
        self.mutate("platformio.ini", "-D ADAPTIVE_LORA_OLED=1", "")
        self.assertTrue(any("default firmware must include" in e for e in check(self.root)))

    def test_broken_current_link_is_reported(self):
        self.mutate("AGENTS.md", "docs/en/SCHEDULED_LINK.md)", "docs/en/MISSING.md)")
        self.assertTrue(any("broken relative link" in e for e in check(self.root)))

    def test_physical_pass_claim_is_rejected(self):
        self.mutate("docs/en/PROGRESS.md", "| Field test | Walk test and radio timing measurements pending |",
                    "| Field test | PASS |")
        self.assertTrue(any("field test and timing" in e for e in check(self.root)))

    def test_archived_body_is_preserved_but_notice_is_required(self):
        path = self.root / "CODEX_START_PROMPT.md"
        with path.open("a", encoding="utf-8") as output:
            output.write("\nHistorical board and pio run -e historical_board\n")
        self.assertEqual([], check(self.root))
        self.mutate("CODEX_START_PROMPT.md", "<!-- archived-contract -->", "<!-- missing -->")
        self.assertTrue(any("archival notice" in e for e in check(self.root)))

    def test_bad_current_region_markers_are_rejected(self):
        with self.assertRaises(ValueError):
            current_text("<!-- current-state:start --> no end")

    def test_timing_constants_are_independent(self):
        self.mutate("include/project_config.h", "kAckTimeoutUs = 1000000", "kAckTimeoutUs = 3000000")
        self.mutate("src/common/protocol/multiclient_protocol.h", "kTargetPeriodUs = 1000000", "kTargetPeriodUs = 3000000")
        failures = check(self.root)
        self.assertTrue(any("approved kAckTimeoutUs" in error for error in failures))
        self.assertTrue(any("approved kTargetPeriodUs" in error for error in failures))

    def test_phy_and_chinese_provisioning_regressions(self):
        self.mutate("include/radio_config.h", "dataSf = 7", "dataSf = 9")
        self.mutate("docs/zh-CN/BENCH_RUNBOOK.md", "hello-interval-ms 3000", "hello-interval-ms 15000")
        failures = check(self.root)
        self.assertTrue(any("approved dataSf" in error for error in failures))
        self.assertTrue(any("docs/zh-CN/BENCH_RUNBOOK.md" in error and "3000 ms" in error for error in failures))

    def test_client_example_requires_distinct_node_id(self):
        self.mutate("docs/en/BENCH_RUNBOOK.md", "config set node-id 2", "config set node-id 1")
        self.assertTrue(any("provisioning examples" in error for error in check(self.root)))

    def test_readmes_share_the_same_figures(self):
        self.mutate("README.zh-CN.md", "adaptive-architecture.svg", "adaptive-frame.svg")
        self.assertTrue(any("shared architecture figure" in e for e in check(self.root)))

    def test_readmes_share_the_two_role_algorithm(self):
        self.mutate("README.zh-CN.md", "adaptive-algorithm-client.png", "missing-client.png")
        self.assertTrue(any("embed both algorithm figures" in e for e in check(self.root)))

    def test_algorithm_requires_a_png(self):
        (self.root / ALGORITHM_FIGURES[0]).write_text("", encoding="utf-8")
        self.assertTrue(any("missing or invalid algorithm PNG" in e for e in check(self.root)))

    def test_both_protocol_guides_show_the_sf_recovery_algorithm(self):
        self.mutate("docs/zh-CN/SCHEDULED_LINK.md", Path(ADR_FIGURE).name, "adaptive-algorithm-gateway.png")
        self.assertTrue(any("expected the SF recovery figure" in e for e in check(self.root)))

    def test_protocol_pages_share_the_symbolic_reservation(self):
        self.mutate("docs/zh-CN/SCHEDULED_LINK.md", r"\le \tau_{\mathrm{frame}}", r"\le 15")
        self.assertTrue(any("same airtime equations" in e for e in check(self.root)))

    def test_walk_test_serial_status_must_match_wire_version(self):
        self.mutate("docs/zh-CN/BENCH_RUNBOOK.md", "protocol=5", "protocol=4")
        self.assertTrue(any("adaptive serial status" in e for e in check(self.root)))

    def test_bench_ceiling_and_capacity_rows_must_match(self):
        self.mutate("docs/en/BENCH_RUNBOOK.md", "bench_payload_ceiling_bytes=16", "bench_payload_ceiling_bytes=64")
        self.mutate("docs/zh-CN/SCHEDULED_LINK.md", "| 8 | 8 | 8 | 5 | 2 |", "| 8 | 8 | 7 | 5 | 2 |")
        failures = check(self.root)
        self.assertTrue(any("bench payload ceiling" in e for e in failures))
        self.assertTrue(any("capacity row for 8 Clients" in e for e in failures))

    def test_extended_capacity_and_matched_comparison_in_both_languages(self):
        self.mutate("docs/zh-CN/SCHEDULED_LINK.md", "2.302 | -3.688", "2.302 | 3.688")
        self.mutate("docs/en/BENCH_RUNBOOK.md", ".pio/comparison-build", ".pio/build")
        failures = check(self.root)
        self.assertTrue(any("SF12 study row" in e for e in failures))
        self.assertTrue(any("missing matched comparison setting" in e for e in failures))


if __name__ == "__main__":
    unittest.main()
