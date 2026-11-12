"""Finite instrumentation of the exact Iteration 011 production generator.

Requires Git, native g++, and existing native SDK shims. Does not run a device.
The baseline source is obtained from Git, not duplicated in this script.
"""
from pathlib import Path
import subprocess
import tempfile

BASELINE = "8fa0a473b6cc0905ddff53e4b9a5f22806e058e2"
ROOT = Path(__file__).resolve().parents[1]


def main():
    baseline = subprocess.check_output(
        ["git", "show", BASELINE + ":src/app/firmware_runtime.cpp"], cwd=ROOT
    ).decode("utf-8")
    probe = r'''
#include <cstdio>
#include <esp_random.h>
int main() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        hostRandom() = HostRandom{};
        auto& r = hostRandom(); r.cycle = true; r.guardAfter = 64;
        r.length = mode == 2 ? 2 : 1;
        r.values[0] = mode == 1 ? 100 : 0; r.values[1] = 100;
        try {
            (void)adaptive_lora::freshBootId(mode == 0 ? 0 : 100);
            std::puts("UNEXPECTED generator return"); return 1;
        } catch (const HostRandomLimit&) {
            std::printf("baseline mode=%u: guard interrupted after %u rejected raw calls; no return\n", mode, unsigned(r.calls));
        }
    }
    return 0;
}
'''
    sources = [str(p.relative_to(ROOT)) for p in (ROOT / "src").rglob("*.cpp")
               if p.name not in {"main.cpp", "firmware_runtime.cpp", "llcc68_hardware.cpp", "preferences_config_storage.cpp"}]
    with tempfile.TemporaryDirectory(prefix="adaptive-lora-rng-probe-") as tmp:
        source = Path(tmp) / "probe.cpp"
        source.write_text(baseline + probe, encoding="utf-8")
        binary = Path(tmp) / "probe.exe"
        command = ["g++", "-std=c++17", "-DADAPTIVE_LORA_NATIVE_TEST=1", "-Iinclude", "-Isrc", "-Isrc/app", "-Itest/support/platform", str(source), *sources, "-o", str(binary)]
        print("Compile exact baseline runtime generator plus production dependencies, native SDK guard64")
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        output = (result.stdout + result.stderr).replace(str(ROOT), "<WORKSPACE>").replace(tmp, "<PROBE_TEMP>")
        print(output.rstrip()); print("compile exit=" + str(result.returncode))
        if result.returncode:
            return result.returncode
        result = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=10)
        print(result.stdout.rstrip()); print(result.stderr.rstrip()); print("probe exit=" + str(result.returncode))
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
