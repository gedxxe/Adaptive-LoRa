# Reading the board display

| Page | What to look for |
|---|---|
| LINK | Role, node ID, peer, last RSSI/SNR and sample age. The period is the Gateway's advertised superframe. |
| DATA | Transmissions or received DATA, retries, application delivery and radio errors. On an adaptive Client, `ACKED DATA SF` shows the most recently acknowledged DATA profile. |
| ACK | Frame, round, received and retry bitmaps, radio state and dropped logs. |

The Gateway samples the first valid DATA for a slot. The Client samples a
matching Grant or ACK bitmap. `WAIT` means it has no qualifying sample.
`ONLINE` means a sample arrived within two superframes, and `STALE` means
the sample is older. On the Gateway, the sample may be from any Client.
`SEARCH` means the Client is listening for a Beacon. A screen can briefly
show both `SEARCH` and `LAST ONLINE`. The latter refers to the retained sample.
Use `status` and its `V3_LINK_STATUS` event for the current state.

Pages rotate LINK → DATA → ACK. A completed page stays up for at least
five seconds. While synchronized, the next update can wait until the
quiet end of the frame. During search, the loop polls the radio between
display transfers. A newly received Beacon restores protection for
upcoming slots. The Client gives the first screen up to 400 ms before
starting Beacon reception.

The runtime checks timing and pending radio interrupts before I²C work.
Each step requests at most 31 bytes with a 3 ms Wire timeout. If a step
exceeds the 10 ms software allowance, the runtime reports
`service_budget_overrun` and defers recovery for at least two seconds.
`OLED_STATUS` reports display I/O and step times. For a physical timing
check, compare these logs with SCL, NSS and DIO1 traces.
[Walk-test procedure](BENCH_RUNBOOK.md#adaptive-v4-walk-test)
