# Startup and provisioning

Use the same firmware environment on the gateway and every client in a
test. Open the folder containing `platformio.ini` in VS Code with the
pioarduino or PlatformIO IDE extension installed.

| Environment | `status` output | Use |
|---|---|---|
| `heltec_wifi_lora_32_V3` | `protocol=1`, `profile=m2` | HELLO/ACK |
| `heltec_wifi_lora_32_V3_oled` | `protocol=1`, `profile=m2` | HELLO/ACK with OLED |
| `heltec_wifi_lora_32_V3_research` | `protocol=2`, `profile=scheduled` | Iteration 019 single-client schedule |
| `heltec_wifi_lora_32_V3_multiclient` | `protocol=3`, `profile=multiclient` | Iteration 020 multi-client payload scheduling |

## Identify the board and upload

1. With the board unpowered, attach a suitable 50-ohm load or antenna. The
   radio can transmit immediately with a saved valid role. Verify the RF
   setup and take the private same-board firmware/NVS backup before first
   upload. Follow the [bench runbook](BENCH_RUNBOOK.md).
2. Connect one board directly with a USB-C data cable. Record the CP210x
   entry and COM number under **Ports (COM & LPT)** in Device Manager, or
   list the ports in PowerShell using the command below. Close any monitor
   using that COM port. If Windows shows no CP210x entry, use the cable and
   driver checks in the bench runbook before attempting upload.
3. In **PlatformIO → Project Tasks**, open the chosen environment and run
   **Build**, then **Upload** under **General**. Connect one board at a
   time or specify the exact port. Keep the standard partition layout and
   use the ordinary upload task to preserve saved NVS. Avoid **Erase Flash**.
   Check partition addresses and security state if changing layouts or
   restoring a backup. Existing configurations are loaded after upload and
   keep their saved intervals.
4. Open **Monitor** for the same environment and COM port at **115200 baud**.
   Send commands with LF or CRLF. The monitor can also be opened with
   `& $pioPython -m platformio device monitor --port COMx --baud 115200`,
   substituting the observed port for `COMx`.

```powershell
$pioPython = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
& $pioPython -m platformio device list
```

See the [PlatformIO VS Code Project Tasks documentation](https://docs.platformio.org/en/stable/integration/ide/vscode.html#project-tasks).

## Provision the v3 network

The shipped gateway admits Node IDs 2–9 in that order, with up to 64 bytes
per application payload. Choose a unique ID for each client. To change
which IDs and budgets the gateway offers, edit `include/research_roster.h`
and rebuild the v3 firmware. If you have only two boards, the same image
can be used with just client 2 present. The other clients are simply absent.
There is no serial roster command. An NVS Node ID identifies the board;
the gateway beacon and matching grant determine when that board may send.

Gateway, on its COM port:

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
reboot
status
```

Client 1, on its COM port:

```text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
reboot
status
```

For each additional client, substitute its admitted ID on the `node-id`
line and repeat on that board. All nodes use network 1 and gateway ID 1.
Check staged values with `config show`. `CONFIG_SAVED` means save/readback
succeeded. After reboot, `status` shows the active values and `protocol=3`.
`CONFIG_INVALID` at startup leads to provisioning with radio TX disabled.
`CONFIG_ERROR` after save shows why the staged configuration was rejected.
If the gateway reports `invalid_schedule_roster`, check its ID, table and
period before retrying. `SCHEDULE_CONFIG` reports roster size and period.

The client’s example source offers one eight-byte report every configured
interval if the queue has room. Look for `APP_PAYLOAD` on the gateway and
`V3_PAYLOAD_RESULT` on the client. A report needs an active grant, so the
3-second configuration does not promise one transmission every 3 seconds.
For wire layout and loss handling, see the [scheduled link specification](SCHEDULED_LINK.md).
