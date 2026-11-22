# 测试结果与研究

[首页](../../README.zh-CN.md) · [现场测试](BENCH_RUNBOOK.md) · [协议](SCHEDULED_LINK.md) · [English](../en/PROGRESS.md)

<!-- current-state:start -->
| 检查 | 结果 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `4` |
| 硬件 | Heltec WiFi LoRa 32 V3.4，ESP32-S3/SX1262 |
| 自适应镜像 | `heltec_wifi_lora_32_V3_adaptive`，ESP32 构建通过，主机测试 8/8 |
| 固定 SF 镜像 | `heltec_wifi_lora_32_V3_multiclient`，ESP32 构建通过，运行时测试 15/15 |
| 共享回归 | Iteration 022 主机测试 356/356 |
| 现场试验 | 走测和无线时序仍待实测 |
<!-- current-state:end -->

主机测试覆盖不同 SF、移动与固定 Client 同时通信、八节点时隙、ACK 回执和无线恢复。
实板覆盖和时序仍需测量。[构建和测试记录](../en/BUILD_MEMORY.md)

按八个 Client 各预留 64 字节、其余节点用 SF7 计算，两个 SF9 预留用时 **14.156640 秒**，
三个用时 **15.634272 秒**。先与配置周期比较，再上板测量收发切换和漂移。
[模型与参数](SCHEDULED_LINK.md#空口时间和时隙预算)

## 论文与对照

[论文到代码的对应关系](../research/ALGORITHM_TRACEABILITY.md) · [论文和图源](../references/README.md)

| 用途 | PlatformIO 环境 | 空口版本 |
|---|---|---:|
| 自适应走测 | `heltec_wifi_lora_32_V3_adaptive` | 4 |
| 固定 SF 对照及 PlatformIO 默认 | `heltec_wifi_lora_32_V3_multiclient` | 3 |
| 单 Client TDMA 对照 | `heltec_wifi_lora_32_V3_research` | 2 |
| HELLO/ACK 对照 | `heltec_wifi_lora_32_V3` | 1 |

同一次试验的各板使用同一个空口版本。若要单独比较 DATA SF 的影响，应统一控制 PHY、
超帧、重传规则、天线位置和负载。仓库所带对照镜像在部分设置上有差异。

## 历史记录

[整合前的手册、硬件检查与测试计划](https://github.com/gedxxe/Adaptive-LoRa/tree/4ab3655dc8ca3b7f9ddc382d8ebeeeadceb6d2c7/docs)保存在固定的 Git 快照。项目继续保留
[构建记录](../en/BUILD_MEMORY.md)、[决策](DECISIONS.md)和[错误记录](../en/ERROR_LOG.md)。
