# 板载屏幕读数

| 页面 | 要看什么 |
|---|---|
| LINK | 角色、节点 ID、对端、上次 RSSI/SNR 及距采样时间。周期取 Gateway 公告的超帧。 |
| DATA | 发送或接收次数、重传、应用交付及无线错误。自适应 Client 的 `ACKED DATA SF` 显示上次收到确认的 DATA SF。 |
| ACK | 帧号、轮次、已收到及重传位图、无线状态和日志丢弃数。 |

Gateway 在某个时隙首次收到有效 DATA 时采样，Client 在收到匹配 Grant 或 ACK 位图时采样。
`WAIT` 表示还没有有效样本，`ONLINE` 表示最近两个超帧内收到过，`STALE` 表示样本更早。
Gateway 的样本可能来自任意 Client。`SEARCH` 表示 Client 正在搜索 Beacon，屏幕上可能
短暂同时看到 `SEARCH` 和 `LAST ONLINE`，后者描述的是保留的上一次样本。
实时状态可执行 `status` 后查看 `V3_LINK_STATUS`。

页面按 LINK → DATA → ACK 轮换。完整显示后至少停留五秒。同步期间，更新可能等到超帧
尾部的空闲段。搜索 Beacon 时，主循环在显示操作之间轮询无线，收到 Beacon 后重新保护
后续时隙。Client 启动时给首屏最多预留 400 ms，再开始监听。

执行 I²C 前检查时序和待处理无线中断。每步最多请求 31 字节，Wire 超时为 3 ms。
超过 10 ms 软件预算时，记录 `service_budget_overrun`，并至少延迟两秒后恢复。
`OLED_STATUS` 记录显示 I/O 与处理耗时。实板检查应结合 SCL、NSS 和 DIO1 波形。
[走测流程](BENCH_RUNBOOK.md#自适应-v4-走测)
