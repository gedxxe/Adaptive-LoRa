from pathlib import Path
import shutil
import tempfile
import unittest

from check_consistency import APPROVED_SETTINGS, LINK_DOCS, check, current_text


class ConsistencyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        source = Path(__file__).resolve().parents[1]
        # Empty targets suffice for link existence checks; copy only inspected inputs.
        for folder in ("docs", "tools", "src", "include", "test"):
            for original in (source / folder).rglob("*"):
                if original.is_file() and original.suffix in (".md", ".py", ".svg", ".png", ".cpp", ".h"):
                    target = self.root / original.relative_to(source)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.touch()
        names = set(LINK_DOCS) | set(APPROVED_SETTINGS) | {"CODEX_START_PROMPT.md", "VERSION", "PROTOCOL_VERSION",
                "platformio.ini", "THIRD_PARTY.md", "CHANGELOG.md", "include/project_config.h",
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
        self.mutate("AGENTS.md", f"protocol `{protocol}`", "protocol `99`")
        self.assertTrue(any("AGENTS.md: current protocol" in e for e in check(self.root)))

    def test_bad_build_environment_is_reported(self):
        self.mutate("README.md", "pio run -e heltec_wifi_lora_32_V3", "pio run -e absent_board")
        self.assertTrue(any("unknown build environment" in e for e in check(self.root)))

    def test_default_build_requires_display(self):
        self.mutate("platformio.ini", "-D ADAPTIVE_LORA_OLED=1", "")
        self.assertTrue(any("default firmware must include" in e for e in check(self.root)))

    def test_broken_current_link_is_reported(self):
        self.mutate("AGENTS.md", "docs/en/HARDWARE.md)", "docs/en/MISSING.md)")
        self.assertTrue(any("broken relative link" in e for e in check(self.root)))

    def test_physical_pass_claim_is_rejected(self):
        self.mutate("docs/en/PROGRESS.md", "| Physical acceptance | IN PROGRESS; NOT RUN |",
                    "| Physical acceptance | PASS |")
        self.assertTrue(any("physical acceptance" in e for e in check(self.root)))

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
        self.mutate("README.zh-CN.md", "hello-interval-ms 3000", "hello-interval-ms 15000")
        failures = check(self.root)
        self.assertTrue(any("approved dataSf" in error for error in failures))
        self.assertTrue(any("README.zh-CN.md" in error and "3000 ms" in error for error in failures))

    def test_client_example_requires_distinct_node_id(self):
        self.mutate("README.md", "config set node-id 2", "config set node-id 1")
        self.assertTrue(any("provisioning examples" in error for error in check(self.root)))

    def test_diagram_translations_must_preserve_edges(self):
        self.mutate("README.zh-CN.md", "requests --> active", "requests --> beacon")
        self.assertTrue(any("shared Mermaid flow" in error for error in check(self.root)))


if __name__ == "__main__":
    unittest.main()
