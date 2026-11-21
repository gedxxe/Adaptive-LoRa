# 屏幕诊断

见 [启动指南](STARTUP_AND_PROVISIONING.md) 及 [硬件检查](HARDWARE.md)。

## 页面与链路状态

| 页面 | 内容 |
|---|---|
| LINK | 角色、节点 ID、最近采样状态、对端 ID、RSSI/SNR、采样年龄、当前帧号及周期 |
| DATA | Gateway 显示每次授权内首个有效 DATA、首次在重传轮收到的包、应用接受/拒绝及完全重复包。Client 显示 DATA 发射启动、重传启动、确认接收数与最近载荷结果。两端均显示无线错误数。 |
| ACK | 最近位图的帧号、轮次、十六进制已接收与重传位图，以及无线状态和日志丢弃数。Gateway 显示收到 TX_DONE 的位图，Client 显示本次授权事务的最近有效位图。 |

Gateway 在当前授权时隙收到首个匹配 DATA 时刷新链路样本，即使应用拒绝载荷。
Client 收到与请求匹配的授权或与授权事务匹配的位图时刷新样本。单独的 Beacon、
无关授权、重复 DATA 和无效帧不刷新对应样本。

- `WAIT` 表示当前没有有效接收样本。Client 接受新的 Gateway 启动会话后也回到 WAIT。
- `ONLINE` 表示最近一次有效接收距今不足两个超帧周期。Gateway 可以由任一已调度 Client 刷新。
- `STALE` 从满两个周期起算。RSSI、SNR 与年龄均对应最近样本，缺失值显示 `N/A`。

`LAST` 标记最近一次获准更新时的快照。启动时 Client 在无线待机期间最多用 400 ms
绘制 `LAST WAIT`。屏幕就绪、报错或达到期限后，Client 开始监听；若先到期限，串口
记录 `OLED_BOOT` 的 `continue_while_searching`。尚未收到 Beacon 或错过预期 Beacon
时，Client 在每页显示 `SEARCH`。`LAST` 表示最近有效样本，超过新鲜度阈值后显示
`STALE`。串口 `status` 的 `V3_LINK_STATUS` 为 Client 提供 `beacon=search` 或
`beacon=synced`。刚开始搜索时，最近样本可能仍显示 `LAST ONLINE`。

页面按 LINK → DATA → ACK → LINK 轮换。整页画完后至少停留五秒，再在下一个允许的
窗口切换。窗口太短时可分段绘制，先画正文，最后更新标题，页面不会跳号。Client
搜索期间继续切页。同步后，使用默认名单时切页可能等到下一轮 6766 ms 超帧的帧尾。

## 刷新时序与故障

运行时先处理无线、角色逻辑和串口，再检查 v3 调度的安全窗口。排版后、调用 I²C 前再次
检查。Beacon、请求、授权、
DATA 和反馈阶段均不执行 I²C，即使本机 Client 已结束事务，也等待其余 Client 的重传时隙。
已同步 Client 用有效组授权判断帧尾，授权丢失时按名单最坏时序计算。保护下一次
Beacon 的预计接收完成时刻，再留 25 ms。超时后旧时隙不再授权发射。搜索时持续接收，
主循环在每个显示步骤之间轮询无线，待处理 IRQ 优先。收到有效 Beacon 后立即恢复时隙保护。

Beacon 发送失败时，Gateway 仍保护可能收到的请求时段；授权发送失败时，仍保护可能
发生的 DATA 和反馈阶段。完成这些窗口后可在帧尾刷新。配置模式和无线初始化失败时，
无线未启动，可独立更新显示。

默认 Gateway 广播 6766 ms 周期，在 6516 ms 无线预留基础上给显示留出 250 ms。
请求、授权、DATA 和反馈的相对时隙保持原样。若名单显式指定周期，直接使用该值。
窗口过短或错过时，绘制会暂停到下一个安全帧尾。整屏冷启动需 48 个数据块和两条
显示命令。主机压力测试按每 4 ms 一步，传输约需 200 ms。

每次服务预算为 10 ms，距下个预期 Beacon 另留 25 ms。SSD1306 的 Wire 传输超时为
3 ms，每步最多写一个 31 字节块。若已执行的步骤耗时超过 10 ms，记录
`service_budget_overrun`。v3 的 I²C 故障或超时会将复位和重绘延后至少两秒。
已初始化的总线保持开启，复位时保持共享 Vext 供电，只在允许显示操作时尝试恢复。
`OLED_STATUS` 给出
`worst_step_us`、`step_budget_us`、`worst_i2c_us` 和 `worst_refresh_io_us`。

台架上同时检查两块屏幕的页面顺序以及串口的授权、DATA 和 ACK 事件。画面停顿或
链路变为 STALE 时，比较两端的 `OLED_STATUS` 和 `V3_LINK_STATUS`。

主机测试覆盖软件调度与故障处理。SDK 锁和调度仍可能延长已开始的调用。实板上应采集
SCL、无线 NSS 和 DIO1 后再评估时延。V3.4 屏幕、供电及射频时序尚未实测。
