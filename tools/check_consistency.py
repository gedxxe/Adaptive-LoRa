"""Check selected current-document facts; historical records are intentionally excluded."""
import argparse
import configparser
from pathlib import Path
import re

CURRENT_DOCS = ("AGENTS.md", "README.md", "README.zh-CN.md",
                "docs/en/PROGRESS.md", "docs/zh-CN/PROGRESS.md")
LINK_DOCS = CURRENT_DOCS + ("docs/en/TDMA_FEASIBILITY.md", "docs/zh-CN/TDMA_FEASIBILITY.md")
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
    protocol = (root / "PROTOCOL_VERSION").read_text(encoding="utf-8").strip()
    environment = config["platformio"]["default_envs"].strip()
    environments = {name[4:] for name in config.sections() if name.startswith("env:")}
    if environment not in environments or config[f"env:{environment}"].get("board") != environment:
        errors.append("platformio.ini: default firmware environment/board mismatch")
    header = (root / "include/project_config.h").read_text(encoding="utf-8")
    if not re.search(r'kFirmwareVersion\[\]\s*=\s*"' + re.escape(firmware) + '"', header):
        errors.append("project_config.h: firmware version differs from VERSION")
    if not re.search(r"kProtocolVersion\s*=\s*" + re.escape(protocol) + r"\s*;", header):
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
                errors.append(f"{name}: current protocol differs from PROTOCOL_VERSION")
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
