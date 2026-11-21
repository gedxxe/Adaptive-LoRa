"""Check selected current-document facts; historical records are intentionally excluded."""
import argparse
import configparser
from pathlib import Path
import re

READMES = ("README.md", "README.zh-CN.md")
CURRENT_DOCS = ("AGENTS.md", *READMES,
                "docs/en/PROGRESS.md", "docs/zh-CN/PROGRESS.md")
LINK_DOCS = CURRENT_DOCS + ("PACKAGE_MANIFEST.md", "docs/research/ALGORITHM_TRACEABILITY.md") + tuple(
    f"docs/{language}/{document}.md"
    for language in ("en", "zh-CN")
    for document in ("ARCHITECTURE", "BENCH_RUNBOOK", "LOGGING_GUIDE", "SCHEDULED_LINK",
                     "STARTUP_AND_PROVISIONING", "TDMA_FEASIBILITY")
)
APPROVED_SETTINGS = {
    "include/project_config.h": {"kDefaultHelloIntervalMs": "3000", "kAckTimeoutUs": "1000000", "kConfigSchema": "1"},
    "include/radio_config.h": {"frequencyMhz": "915.0f", "bandwidthKhz": "125.0f", "dataSf": "7", "controlSf": "9",
                               "codingRateDenominator": "5", "txPowerDbm": "22", "txCurrentLimitMa": "140.0f",
                               "preambleSymbols": "8", "privateSyncWord": "0x12"},
    "src/common/config/device_config.h": {"networkId": "1", "gatewayId": "1", "nodeId": "2",
                                          "helloIntervalMs": "kDefaultHelloIntervalMs"},
    "src/common/protocol/multiclient_protocol.h": {"kVersion": "3", "kAttempts": "4", "kRetrySlots": "4",
                                                  "kTargetPeriodUs": "1000000", "kMaxPeriodUs": "10000000",
                                                  "kGuardUs": "50000"},
    "src/common/protocol/scheduled_protocol.h": {"kVersion": "2", "kLegacyFeedbackBudgetUs": "10000000"},
}
START = "<!-- current-state:start -->"
END = "<!-- current-state:end -->"


def current_text(text):
    if START in text or END in text:
        if text.count(START) != 1 or text.count(END) != 1 or text.index(END) < text.index(START):
            raise ValueError("current-state markers must form one ordered pair")
        return text.split(START, 1)[1].split(END, 1)[0]
    return text


def check(root):
    root = Path(root).resolve()
    errors = []
    config = configparser.ConfigParser()
    config.read(root / "platformio.ini", encoding="utf-8")
    firmware = (root / "VERSION").read_text(encoding="utf-8").strip()
    legacy_protocol = (root / "PROTOCOL_VERSION").read_text(encoding="utf-8").strip()
    protocol = APPROVED_SETTINGS["src/common/protocol/multiclient_protocol.h"]["kVersion"]
    environment = config["platformio"]["default_envs"].strip()
    environments = {name[4:] for name in config.sections() if name.startswith("env:")}
    board = "heltec_wifi_lora_32_V3"
    base = config[f"env:{board}"]
    if (environment != board + "_multiclient" or environment not in environments
            or config[f"env:{environment}"].get("extends") != f"env:{board}" or base.get("board") != board):
        errors.append("platformio.ini: default firmware environment/board mismatch")
    if "jgromes/RadioLib@7.8.1" not in base.get("lib_deps", ""):
        errors.append("platformio.ini: RadioLib must remain pinned to 7.8.1")
    if (environment in environments
            and ("ADAPTIVE_LORA_MULTICLIENT=1" not in config[f"env:{environment}"].get("build_flags", "")
                 or "ADAPTIVE_LORA_OLED=1" not in base.get("build_flags", ""))):
        errors.append("platformio.ini: default firmware must include v3 and the board display")
    for name, settings in APPROVED_SETTINGS.items():
        contents = (root / name).read_text(encoding="utf-8")
        for key, value in settings.items():
            if not re.search(rf"\b{key}\s*=\s*{re.escape(value)}\s*;", contents):
                errors.append(f"{name}: approved {key} must equal {value}")
    ids = APPROVED_SETTINGS["src/common/config/device_config.h"]
    interval = APPROVED_SETTINGS["include/project_config.h"]["kDefaultHelloIntervalMs"]
    expected_commands = [
        [f"config set role {role}", f"config set network-id {ids['networkId']}",
         f"config set node-id {node}", f"config set gateway-id {ids['gatewayId']}",
         f"config set hello-interval-ms {interval}", "config show", "config save"]
        for role, node in (("gateway", ids["gatewayId"]), ("client", ids["nodeId"]))
    ]
    diagrams = []
    for name in READMES:
        text = (root / name).read_text(encoding="utf-8")
        blocks = re.findall(r"^```text\n(config set role [^\n]+\n.*?)^```", text, re.M | re.S)
        if [block.splitlines() for block in blocks] != expected_commands:
            errors.append(f"{name}: gateway and client provisioning examples must match IDs and {interval} ms")
        upload = f"pio run -e {environment} -t upload --upload-port $port"
        monitor = f"pio device monitor --port $port --baud {base.get('monitor_speed')}"
        if not {"pio device list", upload, monitor} <= set(text.splitlines()):
            errors.append(f"{name}: default upload and serial commands must match platformio.ini")
        graphs = re.findall(r"^```mermaid\n(flowchart TD\n.*?)^```", text, re.M | re.S)
        # Compare diagram structure while allowing translated labels.
        diagrams.append([re.sub(r'"[^"\n]*"|\|[^|\n]*\|', "", graph) for graph in graphs])
    if len(diagrams[0]) != 1 or diagrams[0] != diagrams[1]:
        errors.append("README diagrams: expected one shared Mermaid flow in both languages")
    header = (root / "include/project_config.h").read_text(encoding="utf-8")
    if not re.search(r'kFirmwareVersion\[\]\s*=\s*"' + re.escape(firmware) + '"', header):
        errors.append("project_config.h: firmware version differs from VERSION")
    if not re.search(r"kProtocolVersion\s*=\s*" + re.escape(legacy_protocol) + r"\s*;", header):
        errors.append("project_config.h: protocol version differs from PROTOCOL_VERSION")
    adapter = (root / "src/common/radio/sx1262_hardware.h").read_text(encoding="utf-8")
    composition = (root / "src/app/esp32_firmware.h").read_text(encoding="utf-8")
    if not re.search(r"\bSX1262\s+radio_", adapter) or not re.search(r"\bSx1262Hardware\s+radioHardware_", composition):
        errors.append("production composition: expected sole SX1262 adapter")
    for name in CURRENT_DOCS:
        try:
            text = current_text((root / name).read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            errors.append(f"{name}: {error}")
            continue
        versions = set(re.findall(r"(?:\bFirmware|固件)(?: / (?:protocol|协议) \|)?\s*`([^`]+)`", text))
        if versions != {firmware}:
            errors.append(f"{name}: current firmware references differ from VERSION")
        if name == "AGENTS.md":
            role_protocol = re.search(r"\bprotocol `([^`]+)`", text)
            if not role_protocol or role_protocol.group(1) != protocol:
                errors.append(f"{name}: current protocol differs from the v3 profile")
        if environment not in text or "SX1262" not in text:
            errors.append(f"{name}: current board environment or radio missing")
        for found in re.findall(r"pio run -e ([A-Za-z0-9_]+)", text):
            if found not in environments:
                errors.append(f"{name}: unknown build environment {found}")
        if name.endswith("PROGRESS.md") or name.startswith("README"):
            row = re.search(r"\| (?:Firmware / protocol|固件 / 协议) \| `([^`]+)` / `([^`]+)`", text)
            if not row or row.groups() != (firmware, protocol):
                errors.append(f"{name}: current firmware/protocol row mismatch")
        if name.endswith("PROGRESS.md"):
            if "| Physical acceptance | IN PROGRESS; NOT RUN |" not in text:
                errors.append(f"{name}: physical acceptance must remain explicitly open")
    for name in LINK_DOCS:
        path = root / name
        if not path.exists():
            errors.append(f"{name}: missing current document")
            continue
        text = current_text(path.read_text(encoding="utf-8"))
        # Local Markdown links only; external URLs, anchors and code paths are not crawled.
        for target in re.findall(r"\]\(([^\s)]+)(?:\s+\"[^\"]*\")?\)", text):
            if ":" in target or target.startswith("#"):
                continue
            target = target.split("#", 1)[0]
            if target and not (path.parent / target).exists():
                errors.append(f"{name}: broken relative link {target}")
    archive = (root / "CODEX_START_PROMPT.md").read_text(encoding="utf-8").splitlines()[:8]
    notice = "\n".join(archive)
    if "<!-- archived-contract -->" not in notice or "AGENTS.md" not in notice or "docs/en/PROGRESS.md" not in notice:
        errors.append("CODEX_START_PROMPT.md: archival notice/current-contract links missing")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        errors = check(args.root)
    except (OSError, KeyError, ValueError) as error:
        errors = [str(error)]
    for error in errors:
        print(error)
    print(f"{'FAIL' if errors else 'PASS'}: selected current-document consistency ({len(errors)} errors)")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
