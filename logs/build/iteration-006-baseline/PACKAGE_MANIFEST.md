# Package Manifest

Package: Adaptive-LoRa Codex Development Package
Revision: 2.0
Date: 2026-11-12

## Included

- Project-level Codex rules and constraints.
- First implementation prompt.
- Single-image PlatformIO starter configuration.
- English canonical architecture, hardware, protocol, test, progress,
  memory, error, decision, versioning, logging, and milestone documents.
- Simplified Chinese companion README and technical documents.
- Append-only build and error tracking templates.
- Raw log directory guidance.

## Scope

This is a development specification and starter documentation package.
It is not a completed firmware implementation. No firmware build, native
test, radio initialization, or physical RF test is claimed by this
package.

## Key architecture choices

1.  One codebase and one firmware image.
2.  NVS-backed role selection at boot.
3.  Invalid/unset role enters provisioning mode with normal TX disabled.
4.  Ai-Thinker Ra-01SCH uses LLCC68, not SX1262.
5.  M0–M2 fixed-PHY HELLO/ACK first.
6.  TDMA, ACK bitmap, selective retry, ADR, rejoin, and multi-node
    simulation are staged later.
7.  `main.cpp` remains thin.
8.  Every build iteration, including failed iterations, updates Markdown
    memory.
