# Adaptive-LoRa

[简体中文](README.zh-CN.md)

Adaptive-LoRa is a scheduled LoRa network for industrial telemetry. One
Gateway with a single SX1262 serves up to eight Clients. The Gateway assigns
each Client a transmit slot and can choose a different spreading factor
for its data. All boards run the same firmware and load their role from
saved settings.

![Architecture of one Gateway and up to eight Clients, with common SF9 control and separate DATA slots](docs/research/figures/adaptive-architecture.svg)

## How it works

The Gateway broadcasts a Beacon. Clients with data request time to send,
and the Gateway replies with a Grant specifying their payload length and
DATA SF. Once the Clients have sent in turn, one ACK bitmap reports which
packets arrived and which Clients may retry. Beacon, Request, Grant and ACK
all use SF9. DATA and its retries use the SF7–SF9 assigned in the Grant.

![Transmission order for a Beacon, slot requests, a Grant, DATA, an ACK bitmap, and a selective retry](docs/research/figures/adaptive-frame.svg)

When a Client's first attempts repeatedly fail, the Gateway can raise its SF
if the schedule has room. It can try a lower SF when delivery and signal
quality improve. A failed lower-SF trial returns to the previous setting.
The Gateway marks an SF stable after three first-attempt deliveries and
a matching receipt from the Client confirming it received an ACK.

```math
\begin{aligned}
&\textbf{Gateway per superframe}\\
&\quad\text{Send Beacon and collect Requests on SF9}\\
&\quad\text{If requested, fit slots and send Grant}\\
&\quad\textbf{For each DATA or retry round}\\
&\qquad\text{Receive DATA at each assigned SF}\\
&\qquad\text{Send ACK bitmap with retry grants on SF9}\\
&\quad\text{Update SF from delivery, SNR and receipts}\\[3pt]
&\textbf{Client}\\
&\quad\text{Request a slot and echo the last ACKed frame}\\
&\quad\text{Send DATA under a matching Grant}\\
&\quad\text{Return to SF9 and check the ACK bitmap}
\end{aligned}
```

### Airtime budget

The airtime model uses application payload $b$ in bytes and spreading
factor $s$. The extra 32 bytes carry the frame header and Gateway boot ID.
The formula uses BW 125 kHz, CR 4/5, eight preamble symbols and PHY CRC.

```math
\begin{aligned}
n(b,s)&=\left\lceil\frac{8(b+32)+44-4s}{4s}\right\rceil\\
T_{\mathrm{slot}}(b,s)&=\left(0.080+
\bigl(20.25+5n(b,s)\bigr)\frac{2^s}{125000}\right)\mathrm{s}.
\end{aligned}
```

To decide whether a new SF fits, the Gateway uses the payload ceilings
$B_i$ of the full roster $R$. Each retry round reserves time for the four
longest slots $H_4$. $T_{\mathrm{ctrl}}$ covers control frames and the
remaining guards.

```math
\begin{aligned}
T_{\mathrm{reserved}}={}&T_{\mathrm{ctrl}}+
\sum_{i\in R}T_{\mathrm{slot}}(B_i,s_i)\\
&+3\sum_{i\in H_4}T_{\mathrm{slot}}(B_i,s_i)+0.300\,\mathrm{s}
\ \leq\ 15\ \mathrm{s}.
\end{aligned}
```

At eight 64-byte ceilings, two SF9 links with the rest at SF7 reserve
14.157 s. Three SF9 links require 15.634 s. These are calculated durations.

The walk-test image uses a **15 s superframe**, **up to 64 bytes of DATA
per Client**, and **915.0 MHz / 125 kHz / CR 4/5**. Host tests cover mixed-SF
traffic, recovery after missing control packets, and a moving Client model
with three stationary peers. RF range and timing on the boards still need
measurement. [Research references and test results](docs/research/ALGORITHM_TRACEABILITY.md)

## Flash and configure

Use **Heltec WiFi LoRa 32 V3.4** boards. Connect a suitable 50-ohm antenna or
load before power and use an authorized RF setup. For a board with firmware or
settings to preserve, make the same-board backup before opening the monitor
or uploading. [First-power checks and backups](docs/en/BENCH_RUNBOOK.md)

Connect one board at a time with a USB data cable. In the PlatformIO terminal,
replace `COMx` with the port shown for that board. Close any existing monitor
before uploading. The same image is used for both roles.

```powershell
pio device list
$port = 'COMx' # Use the observed port
pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port $port
pio device monitor --port $port --baud 115200
```

In VS Code you can also use **PlatformIO → Project Tasks →
heltec_wifi_lora_32_V3_adaptive → General → Upload**, then **Monitor**.
If the board has no COM port, follow the [USB checks](docs/en/BENCH_RUNBOOK.md#windows-device-manager).

**Gateway**

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
```

**First Client**

```text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
```

Use unique node IDs **3–9** for additional Clients. Check the staged values
with `config show`. After `CONFIG_SAVED`, send `reboot`, reopen the monitor
if needed, then run `status`. Verify the role, IDs, `protocol=4` and
`profile=adaptive`. The 3000 ms setting feeds the example payload source.
Over-the-air transmissions still follow the Gateway's 15 s schedule.

## Verify the link

Start with one Gateway and one Client near each other. Match the Gateway's
`APP_PAYLOAD` event to the Client's `V3_PAYLOAD_RESULT` with
`result=accepted` for the same frame. The Client's DATA page shows
`ACKED DATA SF7` when its first SF7 payload has been acknowledged.
For a walk test, keep Clients 3–5 stationary and move Client 2. Record
where its SF changes and whether the stationary Clients continue delivering.
[Walk test and log capture](docs/en/BENCH_RUNBOOK.md#adaptive-v4-walk-test)

## Reference

- [Firmware architecture and module ownership](docs/en/ARCHITECTURE.md)
- [Radio settings, frame format and timing](docs/en/SCHEDULED_LINK.md)
- [Software checks, field results and comparison images](docs/en/PROGRESS.md)
