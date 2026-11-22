# Protocol and equations

[Home](../../README.md) · [Setup and field test](BENCH_RUNBOOK.md) · [Results](PROGRESS.md) · [简体中文](../zh-CN/SCHEDULED_LINK.md)

## Wire format

![Order of control, DATA and retry slots on the single radio](../research/figures/adaptive-frame.svg)

Protocol 4 uses little-endian fields in a 28-byte header and a 96-byte
maximum frame. The header carries the network, source, destination,
sender boot ID, frame ID, sequence and schedule version. The Gateway
sends control frames to the group. Requests and DATA go to the Gateway.

| Frame | Sender | Body after the header |
|---|---|---|
| Beacon | Gateway | Period in µs (u32), roster count (u8), then node ID (u16) and byte limit (u8) per Client |
| Request | Client | Gateway boot ID (u32), requested bytes (u8), last ACKed frame in this session (u32, zero if absent) |
| Grant | Gateway | Count (u8), then granted bytes (u8), Client boot ID (u32) and DATA SF (u8) per entry |
| DATA | Client | Gateway boot ID (u32), then the granted application bytes |
| ACK bitmap | Gateway | Count, accepted mask and retry mask (u8 each), then one u32 Client boot ID per position. Unaccepted entries carry zero. |

Empty Grant entries carry length `0xFF` and zero boot ID and SF. The
codec checks length, wire version, network, destination, and payload
shape. The runtime also checks the current frame, the expected session
IDs, the Grant and the receive window. Control uses a shared SF. DATA
uses the granted SF for both its first transmission and retries.

## Airtime and slot budget

$\lambda$ counts all bytes presented to the LoRa modem before its PHY
header and CRC. $\sigma$ is the spreading factor and $\beta$ is the channel
bandwidth in Hz. The following symbol count includes the modulation
overhead and coding groups. [Semtech LoRa Calculator](https://www.semtech.com/design-support/lora-calculator)

```math
\begin{aligned}
\tau_{\mathrm{sym}}(\sigma)&=\frac{2^\sigma}{\beta},\\
\nu_{\mathrm{coded}}(\lambda,\sigma)
  &=\max\!\left(0,\left\lceil
     \frac{\kappa\lambda-\alpha\sigma+\zeta_0+
        \zeta_{\mathrm{CRC}}\iota_{\mathrm{CRC}}-
        \zeta_{\mathrm{IH}}\iota_{\mathrm{IH}}}
      {\alpha(\sigma-\mu\iota_{\mathrm{DE}})}
     \right\rceil\right).
\end{aligned}
```

```math
\begin{aligned}
\nu_{\mathrm{sym}}(\lambda,\sigma)
  &= \pi+\psi+\xi+\gamma\nu_{\mathrm{coded}}(\lambda,\sigma),\\
\tau_{\mathrm{air}}(\lambda,\sigma)
  &= \nu_{\mathrm{sym}}(\lambda,\sigma)\,\tau_{\mathrm{sym}}(\sigma).
\end{aligned}
```

| Symbol | Meaning | Current profile or formula coefficient |
|---|---|---|
| $\kappa$, $\alpha$, $\mu$ | Bits per byte, SF coefficient, LDRO denominator penalty | 8, 4, 2 |
| $\zeta_0$, $\zeta_{\mathrm{CRC}}$, $\zeta_{\mathrm{IH}}$ | Baseline, CRC and implicit-header terms in the symbol numerator | 28, 16, 20 |
| $\iota_{\mathrm{CRC}}$, $\iota_{\mathrm{IH}}$, $\iota_{\mathrm{DE}}$ | Flags for PHY CRC, implicit header and low-data-rate optimization | 1, 0, 0 for this SF range |
| $\pi$, $\psi$, $\xi$ | Programmed preamble symbols, fixed synchronization suffix, baseline payload symbols | 8, 4.25, 8 |
| $\gamma$ | Symbols per coding group, the denominator of the LoRa coding rate | 5 for CR 4/5 |
| $\beta$, $\sigma_{\mathrm{ctrl}}$, $\sigma_\iota$ | Bandwidth, common control SF, per-Client DATA SF | 125 kHz, SF9, SF7–SF9 |

$\Omega$ is the advertised roster and $\nu_\Omega=|\Omega|$ includes idle
Clients with Request windows. Use $\Theta=\Omega$ and each payload ceiling
$\lambda_\iota$ when reserving airtime. For the active schedule, $\Theta$
contains only granted Clients and $\lambda_\iota$ is the granted length.
$\Theta^\star$ contains up to $\varkappa$ of the longest slots in $\Theta$.

```math
\begin{aligned}
\eta &= \eta_{\mathrm{header}}+\eta_{\mathrm{session}},\\
\tau_{\mathrm{slot},\iota}
   &=\tau_{\mathrm{air}}(\lambda_\iota+\eta,\sigma_\iota)+\delta,\\
\tau_{\mathrm{data}}&=\sum_{\iota\in\Theta}\tau_{\mathrm{slot},\iota},\\
\tau_{\mathrm{retry}}&=\sum_{\iota\in\Theta^\star}\tau_{\mathrm{slot},\iota}.
\end{aligned}
```

```math
\begin{aligned}
\tau_{\mathrm{requests}} &= \delta+\nu_\Omega(\tau_{\mathrm{R}}+\delta)+\delta,\\
\tau_{\mathrm{bitmaps}} &= (\varrho+1)\tau_{\mathrm{A}} + \delta_{\mathrm{first}}
    +\varrho(\delta_{\mathrm{before}}+\delta_{\mathrm{after}})\\
    &\quad+\delta_{\mathrm{last}}+\delta_{\mathrm{close}},\\
\tau_{\mathrm{ctrl}} &= \tau_{\mathrm{B}}+\tau_{\mathrm{requests}}
    +\tau_{\mathrm{G}}+\tau_{\mathrm{bitmaps}}.
\end{aligned}
```

```math
\tau_{\mathrm{ctrl}}+\tau_{\mathrm{data}}+
\varrho\,\tau_{\mathrm{retry}}+\tau_{\mathrm{tail}}
\le \tau_{\mathrm{frame}}.
```

$\tau_{\mathrm{B}}$, $\tau_{\mathrm{R}}$, $\tau_{\mathrm{G}}$ and
$\tau_{\mathrm{A}}$ are the airtimes of one Beacon, Request, Grant and
bitmap at $\sigma_{\mathrm{ctrl}}$. Their encoded lengths come from the
wire table plus the common header. $\tau_{\mathrm{requests}}$ includes
the Request windows and the gaps before those windows and the Grant.
$\tau_{\mathrm{bitmaps}}$ covers all feedback frames, the gap before the
first DATA round, the gaps around the retry rounds, the gap before
the final bitmap and the closing guard. Each of these gaps uses $\delta$.
The Gateway uses the inequality before accepting a higher-SF
reservation. Lower-SF trials retain the previous reservation.

| Symbol | Meaning | Current adaptive image |
|---|---|---|
| $\eta_{\mathrm{header}}$, $\eta_{\mathrm{session}}$ | Frame header and Gateway boot ID in DATA | 28 B, 4 B |
| $\nu_\Omega$, $\lambda_\iota$ | Shipped roster and per-Client payload ceiling | 8 Clients, 64 B |
| $\varrho$, $\varkappa$ | Retry rounds and shared retry slots per round | 3 rounds, up to 4 slots |
| $\delta$, $\tau_{\mathrm{tail}}$, $\tau_{\mathrm{frame}}$ | Slot guard, end reserve, superframe period | 80 ms, 300 ms, 15 s |

The retune lead and transmit launch window are one quarter of the guard.
The receive window is half a guard on either side of the expected packet
completion. The implementation measures from packet-completion
timestamps. It skips a transmit slot if software reaches it too late.
Physical retune time and drift still need measurement.
[Radio settings](../../include/radio_config.h) ·
[Airtime and sizes](../../src/common/protocol/multiclient_protocol.h) ·
[Slot scheduling](../../src/common/protocol/multiclient_protocol.cpp)

## Adaptation and recovery

After two consecutive degraded reports, the Gateway may try the next
higher SF if the reservation fits. Degraded means the first attempt
failed, even if a retry arrived. A lock requires three first-slot
deliveries and an ACK receipt at the same SF. Once locked, six
first-slot deliveries and enough filtered SNR can open a lower-SF trial
(at least −3.5 dB for SF7 or −6 dB for SF8). A degraded trial rolls back
and holds off another trial for eight first-slot deliveries.

A Client retries only when a valid bitmap grants a retry. The Gateway
opens those slots after that bitmap completes TX. Later cumulative
feedback can acknowledge an earlier delivery. If the Beacon expires,
the Client stops scheduled TX and searches on the control SF. The
Gateway clears its per-Client SF state on a new Client boot ID or four frames
without its Request. Synchronous radio dependencies can still overrun
software windows. [Runtime](../../src/app/multiclient_link.cpp) · [Known timing risks](ERROR_LOG.md)

## Board reference

Heltec V3.4 uses ESP32-S3, SX1262 and CP2102 UART0. Use the V3.2 electrical
reference when checking the board. Radio GPIOs 8–14 are NSS, SCK, MOSI,
MISO, reset, BUSY and DIO1. DIO2 controls the RF switch and DIO3 powers
the 1.8 V TCXO. The display uses SDA 17, SCL 18, reset 21 and active-low
Vext 36. The research profile uses provisional 915.0 MHz and requests
22 dBm with 140 mA PA overcurrent protection.
Verify actual power and RF variant before a field run.
[Board pins](../../include/board_pins.h) ·
[Heltec schematic](https://s.heltec.cn/download/WiFi_LoRa_32_V3/WiFi_LoRa_32_V3.2_Schematic_Diagram.pdf)
