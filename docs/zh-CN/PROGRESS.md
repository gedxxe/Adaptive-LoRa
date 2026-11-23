# 测试结果与研究

[首页](../../README.zh-CN.md) · [现场测试](BENCH_RUNBOOK.md) · [协议](SCHEDULED_LINK.md) · [English](../en/PROGRESS.md)

<!-- current-state:start -->
| 检查 | 结果 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `5` |
| 硬件 | Heltec WiFi LoRa 32 V3.4，ESP32-S3/SX1262 |
| 自适应镜像 | `heltec_wifi_lora_32_V3_adaptive`，Legacy、EWMA 和 Kalman 的 ESP32 构建均通过，主机测试 33/33 |
| 固定 SF 镜像 | `heltec_wifi_lora_32_V3_multiclient`，ESP32 构建通过，运行时测试 15/15 |
| 共享回归 | 主机测试 356/356，Python 测试 64/64，全部通过 |
| 工具链 | PlatformIO 6.1.19、Espressif32 55.3.311、Arduino ESP32 3.3.11、Xtensa 14.2.0+20260121、RadioLib 7.8.1 |
| 现场试验 | 走测和无线时序仍待实测 |
<!-- current-state:end -->

用户报告早期双板走测可自适应到 SF10。当前 32 B 配置仍需现场测试。

| 结论 | 核对方式 |
|---|---|
| 自适应镜像发送恰好 32 字节的 ASCII 样本 | 主机测试解码两个样本，并核对每次重传的帧字节一致。 |
| 五个 SF9 加三个 SF7，或两个 SF10 加六个 SF7，均可交付 32 B 样本 | 使用模拟无线传输运行实际状态机。两个 SF10 的测试覆盖重传和拒绝第三个节点升档。 |
| 两个 SF10 的组合在帧尾预留之外还剩 77.408 毫秒 | 模型计算。真实交付和无线时序仍需实测。 |
| 控制链路丢失时 Client 在 SF9 等待 | SF10 成功交付后模拟断联。收到新的有效 Beacon 和 Grant 后恢复交付。 |
| 重新加入的 Client 可恢复已确认的 SF，并保留其他节点的预留 | 模拟无线传输覆盖离线额度释放、延迟回执和 SF10 额度有限的情况。 |
| 降档未确认时回到此前 SF | 模拟首传失败、ACK 回执缺失以及试用中断联。 |
| 最后一帧 ACK 时序故障仍保留交付记录，并跳过 ADR 更新 | 主机故障注入。 |
| Gateway 错过某个 Client 的首传接收窗时，另一 Client 的 ADR 可继续更新 | 模拟无线传输和 Gateway 主循环延迟。两个节点均完成交付，其中一个通过重传。 |
| 采集工具独立保存九路串口的时间戳和哈希 | 模拟串口测试。 |

[容量表](SCHEDULED_LINK.md#15-秒容量) · [构建和测试记录](../en/BUILD_MEMORY.md)

## 实验 ADR 模型

`python tools/adr_sim.py --seed 42` 对每种场景、丢包率和规则运行 128 组配对试验，
每组 480 帧。假定单个 Client、SF9 控制始终成功、各次发送的干扰相互独立，以及暂定解码曲线。
最终丢包率按最多四次 DATA 发送计算。

| 代表性模型场景 | Legacy | EWMA | Kalman |
|---|---:|---:|---:|
| 20% 独立干扰，高于参考 SF 的时间占比 | 78.77% | 0.00% | 0.00% |
| 同一场景，最终丢包率 | 0.174% | 0.179% | 0.179% |
| 9 dB 遮挡、10% 干扰，到达参考 SF 的平均时间 | 56.5 秒 | 92.5 秒 | 92.5 秒 |
| 同一遮挡场景，最终丢包率 | 0.524% | 0.874% | 0.874% |

参考 SF 为达到模型暂定 50% 解码门限的最低 SF。遮挡开始时若已经达到或超过参考 SF，
该组的反应时间记为零。所列场景中实验规则高于参考 SF 的时间较少，遮挡后到达参考 SF
也较慢。现场比较需要匹配测试条件并标定 Request 与 DATA 的 SNR 偏移。

## 论文与对照

[论文到代码的对应关系](../research/ALGORITHM_TRACEABILITY.md) · [论文和图源](../references/README.md)

| 用途 | PlatformIO 环境 | 空口版本 |
|---|---|---:|
| 自适应走测及 PlatformIO 默认 | `heltec_wifi_lora_32_V3_adaptive` | 5 |
| 早期固定 SF 对照 | `heltec_wifi_lora_32_V3_multiclient` | 3 |
| 单 Client TDMA 对照 | `heltec_wifi_lora_32_V3_research` | 2 |
| HELLO/ACK 对照 | `heltec_wifi_lora_32_V3` | 1 |

同一次试验的各板使用同一个空口版本。[同条件 SF7 Gateway](BENCH_RUNBOOK.md#双板检查与走测)
沿用自适应镜像的空口格式、控制 PHY、帧周期及重传规则。对比时保持天线位置、负载和路线一致。
较早的固定 SF 镜像还会改变控制 PHY 和帧时序。
`PROTOCOL_VERSION` 对应 HELLO/ACK 镜像，其余空口版本由构建配置选择。
所有镜像共用 `VERSION` 和 NVS schema 1。

## 70 节点研究目标

研究单颗半双工无线电在什么条件下可以保持低速遥测的新鲜度。目标为一台 Gateway 服务
70 个 Client，采样间隔 2–5 分钟。当前固件运行一份八节点名单。未来如按每组 15 秒、
每个样本 32 B 轮转，模型结果为每组八个 SF7 或 SF8 节点时每 135 秒一轮，
七个 SF9 节点时每 150 秒一轮，三个 SF10 节点时每 360 秒一轮。混合分组取决于各节点的
SF 预留。丢包、重传竞争、停机和采样时刻都可能拉长两次新读数之间的间隔。

```math
\tau_{\mathrm{cycle}}=\left\lceil\frac{\nu}{\nu_{\mathrm{group}}}\right\rceil\tau_{\mathrm{frame}},
\qquad
\Delta_{\iota,\jmath}=\tau^{\mathrm{rx}}_{\iota,\jmath}-\tau^{\mathrm{rx}}_{\iota,\jmath-1}.
```

$\nu$ 为节点总数，$\nu_{\mathrm{group}}$ 为每组节点上限，$\Delta_{\iota,\jmath}$
是 Client $\iota$ 两次有效交付的时间间隔。候选方案还须满足评估周期内 Gateway 可用的
发射时间。轮转能分配接入机会，但 Gateway 仍需承担每组的控制开销。

1. 使用 1、4、8 个 Client 加一台 Gateway，实测时序、首传交付、最终交付、重传次数与
   每节点两次有效读数之间的间隔。多轮独立重复，报告最差节点及按轮次计算的区间。
   评估信息年龄前加入可对齐的采样时间戳。
2. 按各节点 SF 和载荷上限建模分组轮转。修改空口前先测试组外等待、跨组状态保留、
   公平性和恢复。区分 70 个逻辑身份与 70 条独立真实链路。扩展 SF 或启用 LDRO 前，
   重新核算空口时间并测量切换时间。
3. 用相同 Heltec 比较匹配条件下的固定 SF TDMA 和竞争接入基线，分别测量准入、共享
   重传以及每帧 Request/Grant 相对固定时隙的开销。无实物对照时，将集中器结果标为模拟。

无人值守试点前需要设计报文认证和重放保护、验证看门狗恢复，并加入应用层去重和遥测缓存。
CRC 与启动和帧编号无法认证通信。单 Gateway、单信道仍受共同故障和外部干扰影响。
天线位置与朝向、跨班次丢包、SF 降档门限，以及 28 字节包头对交付的影响均需要现场数据。
当前配置用于获准环境中的受控测试。论文结论、期刊选择与披露要求应以实测及投稿时的规则为准。

## 历史记录

[整合前的手册、硬件检查与测试计划](https://github.com/gedxxe/Adaptive-LoRa/tree/4ab3655dc8ca3b7f9ddc382d8ebeeeadceb6d2c7/docs)保存在固定的 Git 快照。项目继续保留
[构建记录](../en/BUILD_MEMORY.md)、[决策](DECISIONS.md)和[错误记录](../en/ERROR_LOG.md)。
[日志索引](../../logs/README.md)说明历史文件的用途。
