# M0–M2 physical acceptance readiness

The two V3.4-marked boards and stock monopoles are reported in hand. The
firmware builds for the documented V3 pinout, with an opt-in OLED candidate.
The delivered RF variant, pin mapping, USB bridge, flash and OLED have yet
to be confirmed. Windows listed only Bluetooth serial ports during the
Iteration 016 inspection. No firmware was uploaded and no RF or NVS capture
was collected.

Identify each board and verify RF termination before power. Make separate
same-board ROM backups and check flash layout/security before application
boot. Follow the [bench runbook](BENCH_RUNBOOK.md) for provisioning, OLED
checks, the authorized 3 m link and distance survey. ERR-021 BUSY recovery
and ERR-023 SDK startup formatting remain open. M0–M2 physical acceptance
is in progress. M3 development needs measured timing and M2 evidence.
