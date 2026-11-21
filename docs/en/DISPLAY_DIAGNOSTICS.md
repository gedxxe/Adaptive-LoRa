# Display diagnostics

See [startup](STARTUP_AND_PROVISIONING.md) and [hardware checks](HARDWARE.md).

## Reading the v3 display

| Page | What it shows |
|---|---|
| LINK | Role and node ID, last sampled freshness, peer ID, RSSI/SNR, sample age, current frame ID and superframe period |
| DATA | Gateway first valid DATA per assignment, retries first received in a retry round, application accepts/refusals and exact duplicates. Client DATA TX starts, retry TX starts, confirmed application acceptance and last payload outcome. Both roles show radio errors. |
| ACK | Frame and round of the most recent bitmap, accepted and retry masks in hexadecimal, radio state and dropped log count. The gateway shows its last bitmap with TX_DONE. The client shows its last valid bitmap for its granted transaction. |

On the gateway, a first valid DATA packet that matches an active grant and
slot establishes freshness, even if the application refuses it. On the
client, a grant matching its request or a valid bitmap for its granted
transaction establishes freshness. An unrelated grant, beacon alone,
duplicate DATA or invalid packet does not refresh the corresponding sample.

- `WAIT` means there is no qualifying receive sample. A client also returns
  to WAIT when it accepts a new gateway boot session.
- `ONLINE` means a qualifying event was seen less than two advertised
  superframe periods ago. On a gateway this can be any scheduled client.
- `STALE` starts at two periods after the last qualifying event. RSSI, SNR
  and age belong to that last sample. Missing measurements show `N/A`.

The `LAST` label marks a snapshot taken during the last permitted display
update. At startup, a v3 client gets up to 400 ms to draw `LAST WAIT` while
the radio is in standby. It starts listening once the display is ready,
reports an error, or reaches the deadline. If the deadline arrives first,
`OLED_BOOT` logs `continue_while_searching`. A Client that has never heard
a beacon or misses its next expected beacon shows `SEARCH` on every page.
`LAST` describes the last valid sample, which ages into `STALE`. The
`V3_LINK_STATUS` line from `status` shows the live freshness and adds
`beacon=search` or `beacon=synced` on the Client. A fresh sample may still
appear as `LAST ONLINE` briefly after the Client starts searching.

The pages advance LINK → DATA → ACK → LINK. Once a page is fully drawn, it
stays for at least five seconds. The next permitted window can start the
following page. A delayed or split repaint preserves the order and draws
the new title after the body. Rotation continues while the Client searches.
While synchronized to the default roster, a page change may wait until
the next 6,766 ms superframe's quiet tail.

## Display timing and faults

The runtime checks the v3 scheduler after radio polling, role work and
serial output. It checks again after formatting and before any I²C call.
It defers I²C work during beacon, request, grant, DATA
and feedback activity, including peer retry slots after the local client
has finished. A synchronized client uses the latest valid group grant
to find the quiet tail, or the worst-case roster budget if the grant
was lost. It protects the next expected beacon through its expected RX
completion plus 25 ms. After that, its expired schedule authorizes no TX.
During search, the radio stays in continuous RX and the loop polls it
between single display steps. A pending radio IRQ takes precedence. A
newly accepted beacon restores slot protection before more I²C work.

The Gateway preserves the possible request interval when beacon TX fails
and the possible peer DATA and feedback interval when grant TX fails.
After those intervals, the remaining tail can service the display.
Provisioning and failed radio initialization allow display work with the
radio inactive.

The default gateway advertises 6,766 ms, which adds 250 ms of display
margin to the 6,516 ms radio reservation. Requests, grants, DATA and
feedback use the same relative slots. An explicitly configured roster
period is used as written. A short or missed window can pause a repaint
until the next safe tail. A full cold draw uses 48 chunks and two display
commands, around 200 ms at one step per 4 ms in the host stress test.

Each service step uses a 10 ms software budget plus 25 ms of lead time
before the next expected beacon. The SSD1306 path configures a 3 ms Wire
transfer timeout and writes at most one 31-byte chunk per step. A measured
step over 10 ms reports `service_budget_overrun`. On v3, an I²C fault or
overrun defers the next reset and redraw for at least two seconds. An
initialized bus stays open and the shared Vext rail stays on during the
Display reset. Recovery runs only when a display step is permitted.
`OLED_STATUS` reports `worst_step_us`,
`step_budget_us`, `worst_i2c_us` and `worst_refresh_io_us`.

On the bench, check that both displays advance in order while the serial
log records valid grant, DATA and ACK events. Compare `OLED_STATUS` and
`V3_LINK_STATUS` on both boards if the display pauses or the link goes stale.

The software gate and failure response are covered by native tests. SDK
locks and scheduling can extend a call already in progress. Measure SCL,
radio NSS and DIO1 before drawing conclusions about on-board latency.
Check the delivered V3.4 panel and power rail before enabling the v3 image.
