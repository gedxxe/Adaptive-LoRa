# 开发进度

<!-- current-state:start -->
## 当前状态 — Iteration 021

本地 屏幕 集成分支从已发布的 Iteration 020 提交
`75a99dfb0474680f90eb9d9685e5a079595f8e26` 开始。

| 项目 | 当前证据 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `3` |
| 硬件 | Heltec WiFi LoRa 32 V3.4，ESP32-S3/SX1262，V3.2 电气参考 |
| ESP32 构建 | `heltec_wifi_lora_32_V3_multiclient` 和两套旧版对照镜像均通过。 |
| 开发名单 | Gateway 1、Client 2–9，每个 Client 预留 64 字节。超帧周期为 6766 ms。 |
| 时序 | Beacon 和授权以无线完成时间戳为锚点，发射启动仍使用当前处理时间。 |
| ADR 研究 | Gateway 记录各 Client 首轮收到、重传收到或未收到；离线工具使用实验提供的阈值。 |
| Host | 356/356 项 native 回归、15/15 项 v3 运行时测试、23/23 项最终 v3 链路专项测试 |
| Physical acceptance | IN PROGRESS; NOT RUN |

PHY、NVS schema、上报周期、M2 ACK 超时、v3 线格式和相对无线时隙保持原样。
镜像选择见 [启动指南](STARTUP_AND_PROVISIONING.md)，
Client 开始监听前最多预留 400 ms 绘制 WAIT。屏幕含义见 [屏幕 诊断](DISPLAY_DIAGNOSTICS.md)，
完整回归见 [构建记录](../en/BUILD_MEMORY.md)。
操作者报告独立 SF9/CR 4/6 实验约增加五米，期间仍偶发 STALE。暂无原始采集日志可分析。
v3 开发镜像使用 SF7。
<!-- current-state:end -->

## Iteration 020 历史快照

`codex/iteration-020-multiclient-payload-scheduling` 接续已发布的 Iteration 019
`979d873fad629aedcdf0e8a565b4ad3681bbea61`。规范 `main` 仍为
`541e2e8b985f7cd67ca4d1f1af323c46aed4333e`。

| 项目 | 当前证据 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `1` 为默认，另有 v2 和 v3 研究镜像 |
| 硬件 | Heltec WiFi LoRa 32 V3.4，ESP32-S3/SX1262，V3.2 电气参考 |
| 时序 | 默认上报 3000 ms，M2 ACK 1000 ms。v3 目标 1000 ms、上限 10000 ms，按 50 ms 试验保护时间计算当前名单周期 6516 ms。 |
| 射频与存储 | 915.0 MHz、SF7/BW125/CR4/5、请求功率 22 dBm、PA OCP 140 mA。默认网络/Gateway/Client 为 1/1/2，已有 NVS schema 1 配置保留。 |
| ESP32 构建 | `heltec_wifi_lora_32_V3`、`_oled`、`_research`、`_multiclient` 均通过 |
| Native | `native` 353/353，`native_multiclient` 3/3 均通过 |
| 主机工具 | 采集 13/13、旧模型 16/16、v3 模型 3/3、文档测试 9/9、直接检查均通过 |
| Physical acceptance | IN PROGRESS; NOT RUN |

主机执行 v3 Gateway 及 1、2、4、8 个 Client 的实际状态机。DATA 按提交长度授权，
反馈只向欠缺位置分配共享重传。十节点在模型假设下满足十秒预算，固件当前接纳上限
为八个。未操作或测量实物。见 [启动指南](STARTUP_AND_PROVISIONING.md)、
[定时协议](SCHEDULED_LINK.md) 和 [构建记录](../en/BUILD_MEMORY.md)。

## Iteration 019 历史快照

审查分支 `codex/iteration-019-research-implementation` 从已发布的 Iteration 018
`0735bb8ef5cb7c1d677c98f2123c4a3f6cb7a1ea` 开始。规范 `main` 基线仍为
`541e2e8b985f7cd67ca4d1f1af323c46aed4333e`。

| 项目 | 当前证据 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `1`（默认 M2）；可选定时 v2 |
| 构建环境 | `heltec_wifi_lora_32_V3`、`_oled`、`_research` 均退出 0 |
| 硬件 | 用户报告两块 Heltec WiFi LoRa 32 V3.4，ESP32-S3/SX1262、原装单极天线、采购 902–928 MHz，电气参考为 V3.2 |
| 恢复配置 | 915.0 MHz、SF7/BW125/CR4/5、请求 22 dBm、PA OCP 140 mA、网络/Gateway/Client 1/1/2、默认上报和 M2 ACK 超时均 10000 ms、NVS schema 1 |
| 定时软件 | 单 Client、按载荷计算时隙、累计位图反馈和三次重传。主机测试执行真实状态机。 |
| 软件验证 | Native 333/333、采集 13/13、空口模型 16/16、文档测试 7/7 及直接一致性检查均通过。 |
| Physical acceptance | IN PROGRESS; NOT RUN |

先按 [M2 台架流程](BENCH_RUNBOOK.md) 操作。M3/M4 验收需两块板运行 v2，并测量传导
功率、收发切换、IRQ 延迟及漂移。见 [v2 规范](SCHEDULED_LINK.md)、
[仓库审查](../research/ITERATION_019_AUDIT.md) 与 [构建记录](../en/BUILD_MEMORY.md)。
M5–M8 仍在计划中。本轮未操作硬件。

## Iteration 018 历史快照

main 已合入 PR #6，提交 `541e2e8b985f7cd67ca4d1f1af323c46aed4333e`。
研究分支为 `codex/iteration-018-research-traceability`。

| 项目 | 当前证据 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `1` |
| 构建环境 | `heltec_wifi_lora_32_V3` 与可选 `heltec_wifi_lora_32_V3_oled` |
| 硬件 | Heltec WiFi LoRa 32 V3.4，ESP32-S3/SX1262，V3.2 电气参考。 |
| 软件范围 | M2 HELLO/ACK，扩充拒绝预留 ID 测试，生产固件不变。 |
| 研究 | 保留四篇论文，登记十五图，三幅授权裁图、六幅原创 SVG。 |
| 软件验证 | 两种 S3 编译通过，与 016.1 二进制一致。Native 316/316、采集 13/13、空口模型 16/16、文档 7/7 及 checker 通过。 |
| Physical acceptance | IN PROGRESS; NOT RUN |

[论文与图表](../references/README.md) 和 [追溯矩阵](../research/ALGORITHM_TRACEABILITY.md)
区分论文结论、项目设计、软件检查与实物测量。M3–M8 生产功能未启动。实物下一步见
[台架流程](BENCH_RUNBOOK.md)。

## Iteration 016.1 历史快照

客户端日期 2026-11-19。Iteration 015 经 PR #5 合入 main，合并提交为
`e34e7296135b47ecd3af479fe0593f4118bc734b`。当前审查分支
`iteration-016-v34-physical-validation`。

| 项目 | 证据 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `1` |
| 构建环境 | `heltec_wifi_lora_32_V3`，可选 `heltec_wifi_lora_32_V3_oled` |
| 板卡 | 两块 Heltec WiFi LoRa 32 V3.4，ESP32-S3/SX1262，配原装单极天线。用户确认采购 902–928 MHz，电气参考为 V3.2。 |
| Native | 316/316 PASS，其中九项 OLED/遥测新测试 |
| Capture / 模型 | 13/13 和 16/16 PASS，仅主机 |
| 固件构建 | 两种均 PASS。OLED 关 RAM 32,184 / flash 342,852 bytes，开 33,960 / 377,488 bytes。 |
| 文档 | 7/7 PASS，直接检查 PASS |
| Physical acceptance | IN PROGRESS; NOT RUN |

用户确认 V3.4 沿用 V3.2 引脚和线路，改进涉及 SMT 和晶振。实物 SKU、天线特性、USB、
flash 安全与布局、外设行为及实际时序仍待核实。本机只列出六个蓝牙虚拟串口，未见
CP2102。重建两种镜像均与 Iteration 016 二进制一致。固定 915.0 MHz 和可选 OLED 延续，
M3–M8 生产功能未开始。按 [台架流程](BENCH_RUNBOOK.md) 逐块识别 CP2102，再完成同板
备份、OLED 检查和两板各自 15000 ms 配置。硬件证据见 [HARDWARE](HARDWARE.md)，
软件结果见 [英文构建记录](../en/BUILD_MEMORY.md)。未取得实物验收结果。

## Iteration 015 历史快照

### Iteration 015 合入前审查记录

客户端日期：2026-11-18。规范 main 基线为已合并的 Iteration 014：
`0d0e71c368e88093b081bf9182e1f1babd189a9a`（[PR #4](https://github.com/gedxxe/Adaptive-LoRa/pull/4)）。
当前分支为 `iteration-015-prehardware-stabilization`，提交 PR 供审查，
本任务不自动合并；合并前 main 仍为规范固件基线。

| 项目 | 当前证据 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `1` |
| 硬件 | Heltec WiFi LoRa 32 V3.2 HF，ESP32-S3FN8/SX1262 |
| 构建环境 | `heltec_wifi_lora_32_V3` |
| 板配置 | 8 MB flash，不假定 PSRAM；CP2102/UART0 115200，TCXO 1.8 V，DC-DC |
| S3 构建 | PASS，退出 0 |
| Native | 307/307 PASS：原 302 加五项去重边界测试，无跳过 |
| Capture | 13/13 PASS，仅 fake serial |
| Airtime/model | 16/16 PASS，仅主机分析模型 |
| Documentation consistency | 7/7 tests PASS; checker exit 0 |
| Physical acceptance | IN PROGRESS; NOT RUN |
| M3–M8 生产功能 | NOT STARTED；主机初步研究单独记录 |
| 静态资源 | RAM 32,096 / flash 342,560 bytes，与 Iteration 014 相同 |

单镜像、NVS 角色选择、schema 1、固定 SF7/BW125 和协议 v1 HELLO/ACK 均不变。
38 个生产/配置/版本输入指纹及固件二进制与基线一致，main.cpp 仍为 15 行。
命令日志与结果见 logs/build/iteration-015-verification-esp32* 和
logs/test/iteration-015-verification-{native,capture,model}*；文档检查结果见 BUILD_MEMORY。

AGENTS 精简为当前工程合同，修正活动架构图和启动顺序，旧 CODEX_START_PROMPT
加归档声明。下方历史快照保留原硬件、结果和发布上下文。
[TDMA_FEASIBILITY.md](TDMA_FEASIBILITY.md) 给出 216 个主机分析场景，
假设十秒超帧，切换/处理/guard 均未知，不证明节点容量。八 session 缓存的淘汰、
boot churn 和重启限制已有测试，未修改生产去重策略。

实际工具链：Core 6.1.19、espressif32 55.03.311、Arduino 3.3.11、
IDF libs 5.5.5+sha.b774170ff46、Xtensa GCC 14.2.0+20260121、RadioLib 7.8.1、
native 1.2.1、Unity 2.6.1、host GCC 6.3.0。platform 未固定，复现需核对记录清单。

ERR-008/009/010 同步调用和内存分配限制、ERR-021 TX BUSY 等待、ERR-023
Runtime 前 NVS 格式化风险仍开放。没有复现新的生产缺陷，也未打开真实端口、烧录、
擦除或进行 NVS/RF/重启实物测试。到货标识、供电、IRQ/TCXO、heap/stack 峰值、
熵、天线匹配/增益和链路可靠性均未测量。

下一步核对两块 V3.2 HF 实物，上电前接 RF 终端，核实同板 ROM 私有备份、安全状态
和分区，然后按 [BENCH_RUNBOOK.md](BENCH_RUNBOOK.md) 采集正常、负向和恢复证据。
测量切换、处理和漂移后再审查 M3 实现；ERR-021 恢复仍是独立验收门槛。


## 截至 Iteration 014 的历史快照

下方库存、命令和“当前”均只描述原迭代，不代表活动目标。详细历史保留在英文 BUILD_MEMORY。

<details>
<summary>历史进度与验收快照</summary>

# 开发进度

## Iteration014：Heltec V3.2 HF软件迁移完成

客户端日期2026-11-17。用户明确授权立即实施并保留工程历史。唯一活动目标为V3.2 HF、S3FN8/SX1262，CP2102/UART0、TCXO1.8 V、DC-DC；基线main075b8faf包含PR #3，分支iteration-014-heltec-sx1262-migration-audit。审查后按直接授权通过普通merge PR发布，最终SHA以PR状态为准。

S3编译退出0 PASS，native退出0 **302/302 PASS**（原290+新adapter12），capture另13/13 fake-port通过。首次fixture重复常量编译失败日志保留，修正后全套通过（ERR-025）。协议1/NVSschema1/固件0.2.0-alpha.1不变，25个protected原文件完全一致，main仍15行。RAM32096、flash342560；不是heap/stack实测。证据见HARDWARE_MIGRATION_AUDIT及iteration-014日志/指纹。

保留915 MHz/BW125/SF7/10 dBm，未做OLED/电池/TDMA/bitmap/选择性重传/ADR。实物到货/板标识、供电、UART/reset、NVS/Flash、SPI/IRQ/TCXO/RF、天线/熵/运行内存均未验证。ERR-021/023开放，旧CDC后端证据属历史。M0–M2实物IN PROGRESS，M3–M8未开始。下一步按已选目标采购核对、接RF终端、同板ROM私有备份，再走当前runbook，不能沿用旧拆外置radio的单MCU流程。

## 截至Iteration013的历史快照

下文保留各自硬件和发布上下文，旧库存/命令不是当前活动配置。

# 开发进度

## Iteration013 main 整合 - 验证完成

2026-11-13，PR [#3](https://github.com/gedxxe/Adaptive-LoRa/pull/3) 将已核实013 `e431e3f8ffd6e2d95dc126bb68e8231723a13c4d` 合入初始main `5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861`，只用普通merge commit。审查113个差异路径；main独有2个合并记录相对共同祖先无tree变化，故无冲突候选tree与013相同。本次不改生产/测试/工具输入，仅加整合文档/日志。本快照记录已测试候选；链接PR提供最终发布状态和merge SHA，并独立核对remote main。合并后 **main 为规范开发基线**（ADR-017），保留全部历史分支/记录。

重新执行ESP32退出0 PASS、native退出0 **290/290 PASS**、capture退出0 **13/13 PASS**（仅fake serial）。55项输入及firmware.bin与013完全一致；RAM24,564/flash364,308、固件0.2.0-alpha.1/协议1/NVSschema1/RadioLib7.8.1/pins/固定PHY不变，main.cpp仍15行。证据见 `logs/build/iteration-013-main-integration-validation.json`、review.json及相应build/native/capture日志/逐例记录。保留e431e3f祖先，不squash/rebase/强推/删分支。

真实NVS/reset/USB/SPI/IRQ/RF/天线/熵/heap/stack仍NOT VERIFIED/NOT MEASURED。ERR-005/008/009/010/021/023保留；M0–M2实物IN PROGRESS、M3–M8 NOT STARTED。后续从同步main开发，既有备份/单MCU/pair门槛与依赖恢复限制继续适用。

## Iteration013准备包验证通过，实施已发布

2026-11-13 client date，直接基线9341e7c0016c66fd9316d489ccfca00a646ae2cb，分支iteration-013-bench-readiness，不基于main5bc607c。ESP32退出0 PASS13.053s，native退出0 **290/290 PASS**61.980s，所有旧cases/source不变；capture另有13/13 fake-port通过，不并入native/RF数。firmware/bin/native tests/pins/PHY/watchdog/库不改，RAM24,564/flash364,308，main15行。完整diff/安全/历史检查通过；实施 `1343e45ae642d0de7c4015ae19575e4e005f1646` commit/push及独立remote013核实均退出0，main/012不变，回执iteration-013-publication-receipt.json在仅文档后续提交保存。ERR-024初次review拒绝经重读Stage8明确授权后重试获批，无merge/PR/tag。

PHYSICAL_ACCEPTANCE_READINESS.md串联备份单MCU、pair gates/仪器、ERR-021和采集/CSV。官方V1.1.0 module pin功能已核对；GPIO提案/手中板/continuity仍未验证。只读枚举无port，未开口/烧录/擦除/复位/RF。ERR-022中断false-success已修；ERR-023是SDK app前可能格式化NVS源码风险，backup须先于boot。ERR-021阻挡无人值守/卡BUSY恢复，不能假定默认watchdog reset；replay/错身份/corrupt-NVS fixture未提供/执行。

M0/M1/M2实物IN PROGRESS/NOT RUN，M3–M8 NOT STARTED。下一步识别连接板、授权且backup verified后按单MCU流程，采购第二MCU/天线或负载/仪器，核实布线/供电/USB/合法RF，完成fixture和恢复处置再关闭pair gates；不继续推测性抽象/tests。

## 历史Iteration012软件验证PASS，实施已发布

直接基线8fa0a473b6cc0905ddff53e4b9a5f22806e058e2，分支iteration-012-runtime-reliability，不基于main5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861。native退出0 **290/290 PASS**，77.575s（旧266不变+新24）；ESP32退出0 PASS31.748s。ERR-019以8次/0失败/保持旧session/1s恢复/禁TX修正；ERR-020刷新poll后app、stop后100ms、drain后reset。保留RED9FAIL/1PASS及修正10/10证据；ERR-018工具修正保留。完整暂存审查通过；实施 `bdab1cec5de6e38f84b524a8189945dec1ee7dd2` commit/push及独立remote012核实均退出0，main/011不变。回执 `logs/build/iteration-012-publication-receipt.json` 记录已验证实施，后续仅文档提交保存回执，无merge/PR/tag。

RAM24,564（+16）、flash364,308（+466）；固件0.2.0-alpha.1/协议1/RadioLib7.8.1/NVS/wire/pins/PHY/命令/main15行不变；有意故障/deadline修正见ADR-015。无新warning。DEPENDENCY_BUDGET_AUDIT.md及iteration-012记录精确源码/ELF/分配/时延；ERR-021是上游TX BUSY-ramp无显式timeout的源码风险，未实测；ERR-005/008/009/010保留，不承诺硬实时/熵/唯一或实物性能。

数值有限性和延迟边界为实际软件验证，clock/serial/raw RNG/backend/reset为模拟。真实NVS/reset/USB/SPI/IRQ/RF/heap/stack/熵NOT VERIFIED/NOT MEASURED，M0–M2实物IN PROGRESS，M3–M8 NOT STARTED。下一软件范围建议另行审查有界上游BUSY-launch方案与SDK分配失败；采购/组装后按计划测量。

## 历史Iteration 011 软件验证PASS，实施已发布

最终 `pio run -e esp32c3_supermini` 退出0，PASS（143.01s）；`pio test -e native` 退出0，**266/266 PASS**（36.68s），实际runtime新增47，既有219全部不变且通过。直接基线 `3902de90058ec775587105c5c3edb4914853a9d8`，分支 `iteration-011-runtime-verification`；main是外部合并 `5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861`。实施提交 `04280ed3d36e97924a7644c47b1e9cea09b438fd` 已push并独立核实与remote011一致（commit/push/ls-remote均退出0），main和010不变。回执 `logs/build/iteration-011-publication-receipt.json`；仅文档后续提交记录已验证实施SHA，无merge/PR/tag。

固件0.2.0-alpha.1/协议1/RadioLib7.8.1、NVS/wire/pins/PHY/app策略不变。仅引用seam与Esp32Firmware组合，main15行。RAM24,548（+16）、flash363,842（-12）。首次native配置失败ERR-017保留，启用完整64位Unity断言后CLOSED；ERR-016 runner workaround已验证、根因未知。无新增warning，ERR-005/008/009/010保留。细节见RUNTIME_AUDIT.md及 `logs/test/iteration-011-scenarios.md`。

协调器顺序/分派/deadline/预算为实际生产软件验证；storage/radio/串口/时钟/reset/raw RNG为模拟。真实NVS/USB/重启/SPI/IRQ/RF/熵均NOT VERIFIED。stop失败仍可reset，日志可未排空，永久坏随机源可无限loop。M0–M2实物门槛IN PROGRESS，M3–M8 NOT STARTED。下一软件任务建议依赖时延/分配预算和boot-ID故障活性分析；采购/组装/全部物理测量待完成。

历史 Iteration 010 软件检查 PASS：ESP32 编译退出 0（146.83 s），native **219/219 PASS**、退出 0（28.62 s）：新增实际 NvsConfigStore 56，既有 regression 163 全部不变且通过。基线 remote Iteration 009 为 `3176e766d3876c86253e7694a7694560be72d1bf`；preflight main 已由外部合并变为 `2a7a68120552a8037d796cf61b0af11e91abd0f2`。本轮直接基于指定009，不基于 main。分支 `iteration-010-config-store-verification` 已发布，实施提交 `01370ac3a6fcc04e9b2bd626a46c9c1b95eb73d4` 与 remote010 一致（push/核实退出 0），remote009 保持 `3176e766d3876c86253e7694a7694560be72d1bf`，main 保持 `2a7a68120552a8037d796cf61b0af11e91abd0f2`。回执 `logs/build/iteration-010-publication-receipt.json` 在仅文档后续提交记录，无合并/PR。证据见 CONFIG_STORE_AUDIT.md、`logs/test/iteration-010-scenarios.md`；实物 NOT VERIFIED。

历史 Iteration 009 软件检查 PASS：ESP32 编译退出 0（147.15 s），native **163/163 PASS**、退出 0（26.04 s）：新增实际生产 RadioDriver 故障用例 71 个，既有 regression 92 个全部通过。修改前审查/覆盖见 RADIO_DRIVER_AUDIT.md，逐条结果 `logs/test/iteration-009-scenarios.md`。分支 `iteration-009-radio-driver-verification` 已发布，实施提交 `4055a947bdfcd518cdff36d6ae76acf3fabeed93` 与远端分支一致（push/核实退出 0）。回执 `logs/build/iteration-009-publication-receipt.json` 在后续仅文档提交记录；main 保持 `b040ef111b82db96ab81a3c7377efbcf46385221`，无合并、无 PR。实物仍 NOT VERIFIED。

项目：**Adaptive-LoRa**。研究标题：Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks。

历史 Iteration 008 建立独立仓库并按授权脱敏历史身份/路径，保留时间顺序、结果及错误解决状态。该轮 ESP32 编译 PASS、退出码 0（`logs/build/iteration-008-esp32.log`）；native 92/92 PASS、退出码 0（`logs/test/iteration-008-native.log`），仍为 38 integration 场景。固件 0.2.0-alpha.1、协议 1、PHY、packet/state 和旧存储字节均不变；四个架构目标与缺口只记录，不新增实现。

Git 发布已核实：main 已推送到明确授权的公共仓库，建立提交 `1d7ab54379221dff8b4eb9347833282657b6e1dc` 与远端 main 一致（git ls-remote 退出码 0）。远端原为空，无需合并历史；暂存与安全审查通过。后续仅文档回执提交记录这一已核实 SHA，见英语 BUILD_MEMORY.md 和 logs/build/iteration-008-publication-receipt.json；不声称实物验证。

历史结果（iteration 007）：ESP32 编译退出码 0，native **92/92 PASS**（实际生产状态机 38、codec 18、role/config 13、console 10、sequence/timing 7、logger 6）。首轮旧 MinGW PRI 宏问题（ERR-011）已通过 `<cinttypes>` 修复。实际库存两个模块/一个 MCU；第二个 MCU 尚未购买、Moxon 尚未制作，未组装双设备。Host 场景不是实物 NVS/RF 验证；没有完整 milestone PASS。历史记录保留。

状态：`NOT STARTED`、`IN PROGRESS`、`BLOCKED`、`PASS`。只有记录验收标准和证据后才能标记为
PASS。

## 当前基线

| 项目              | 当前值                                         |
|-------------------|------------------------------------------------|
| 固件架构          | 一份固件，启动时从 NVS 读取角色                |
| MCU 库存          | 1 个 ESP32-C3 Super Mini；第二个尚未购买，未组装双设备 |
| 无线模块库存      | 2 个 RA-01SCH/LLCC68；Moxon 尚未制作 |
| RadioLib          | 7.8.1                                          |
| 频率              | 915.0 MHz，临时测试值                          |
| 初始数据 PHY      | SF7 / BW125 / CR4/5                            |
| 控制 PHY          | SF9 / BW125 / CR4/5                            |
| 初始 TX 功率      | 10 dBm                                         |
| Network ID        | 1                                              |
| Gateway ID        | 1                                              |
| Client ID         | 2                                              |
| Firmware baseline | 0.2.0-alpha.1 实现候选                         |
| Protocol version  | 1                                              |

## Milestone 状态

| ID  | Milestone                          | 状态        | 证据                   |
|-----|------------------------------------|-------------|------------------------|
| M0  | 硬件、引脚、电源、天线和工具链确认 | IN PROGRESS | 工具链已记录；实物检查待完成 |
| M1  | 两台设备 LLCC68 初始化             | IN PROGRESS | 固件编译通过；NVS/无线实测待完成 |
| M2  | 固定 PHY HELLO/ACK                 | IN PROGRESS | 290 个 native 测试通过，含生产 app/driver/store；RF 待完成 |
| M3  | Beacon 和单客户端 TDMA             | NOT STARTED | 等待 M2                |
| M4  | ACK Bitmap 和选择性重传            | NOT STARTED | 等待 M3                |
| M5  | SF7/SF8/SF9 ADR                    | NOT STARTED | 等待 M4                |
| M6  | 同步恢复和重新加入                 | NOT STARTED | 尚未开始               |
| M7  | Moxon 射频对比                     | NOT STARTED | 尚无受控测试数据       |
| M8  | 5–70 个逻辑节点仿真                | NOT STARTED | 尚无仿真记录           |

## 待确认事项

- [ ] ESP32-C3 Super Mini 实际版本和 Flash 容量。
- [ ] Ra-01SCH 模块标记和引脚方向。
- [ ] GPIO 连通性。
- [ ] 无线模块 3.3 V 电源及发射时电压。
- [ ] 天线接口和 50 欧姆馈线。
- [ ] 当地频率、信道和功率要求。
- [x] 实际工具链版本：Core 6.1.19，已安装 pioarduino/espressif32 55.03.311，Arduino-ESP32 3.3.11，RISC-V GCC 14.2.0 (esp-14.2.0_20260121)，RadioLib 7.8.1；host GCC 6.3.0 / native 1.2.1 / Unity 2.6.1。

历史 iteration 007 候选固件通过编译及 92 个 native 测试，版本为 0.2.0-alpha.1 / 协议 1。实际 app 状态语句未改，只改 transport 构造参数及 C++ include；main.cpp 仍 15 行。同步 NVS/BUSY、库临时分配、SDK 串口等待、IRQ/随机源假设仍未验证。硬件采购/制作/组装完成前继续软件工作，不执行实物流程，不进入 M3。详细结果及所有 case 见 SOFTWARE_AUDIT.md 与 `logs/test/iteration-007-scenarios.md`。

## 2026-11-12 实现检查点（ITERATION-001）

已添加 `0.2.0-alpha.1` 候选源码，协议仍为 1。M0/M1/M2 均为 IN PROGRESS，尚未通过实物验收。Native 测试因缺少标准库 include 编译失败（ERR-002）；ESP32 SDK 安装/编译仍在进行。没有执行烧录、NVS 实物测试或射频测试。

NVS 使用带完整性校验的 24 字节单 blob；配置编辑只影响暂存值，保存后重启生效。新增 `config set hello-interval-ms <1000..3600000>`，默认事务结束后等待 5000 ms 再发新 HELLO。角色命令不自动修改 ID；按 README 明确设置全部 ID。Network ID 可为 65535，节点地址 65535 保留。

固定数据 PHY SF7、私有 sync word 0x12；XTAL、LDO、USB CDC 和全部引脚仍需实物确认。去重缓存保留 8 个 session，每个 64 个 sequence；旧 session 淘汰和 Gateway 重启后不保留历史。状态日志报告队列丢弃计数，不能将驱动类型视为独立芯片识别。

ITERATION-002：显式 include 已修复，4 个 native suite 均已编译运行；44 个测试中 43 个通过、1 个失败。失败原因是 Unity 不允许对长度为 0 的数组执行内容比较（ERR-003），将修正该测试。ESP32 SDK 安装/编译仍在进行。未执行实物测试。

ITERATION-003：44/44 native 测试通过，退出码 0。第一次 ESP32 编译在并发头文件修改期间遇到 GatewayApp 缺少直接 include（ERR-004）；当前已补充直接 include，等待源码冻结后的编译验证。USB CDC 的睡眠相关依赖警告保留（ERR-005），本固件不请求 MCU 睡眠。发射失败与 ACK 超时分别计数；DIO1 卡高时仍受 TX 截止时间限制。未执行实物测试，M0/M1/M2 尚未 PASS。

ITERATION-004：冻结源码 ESP32 编译成功，退出码 0（`logs/build/iteration-004-esp32.log`），ERR-004 已验证关闭。仅剩 USB CDC 实物检查和全部硬件/RF 验收未完成。包列表编码/命令选项及沙箱内 compiler 探测失败记录在 ERR-006，已用 UTF-8 普通包列表/沙箱外 compiler 输出解决。只枚举到蓝牙串口，未识别 ESP32；未执行烧录或实物测量。

最终源码审查还发现 ERR-007：HEADER_ERR 必须结合 HEADER_VALID 判断，避免比 RadioLib 更严格的误拒绝；当前修正待 iteration-005 编译验证。这是静态审查发现，未声称观测到 RF 失败率。

ITERATION-005 最终结果：HEADER_ERR 条件已与固定版本 RadioLib 对齐；固件编译退出码 0（`logs/build/iteration-005-esp32.log`），native 44/44 通过、退出码 0（`logs/test/iteration-005-native.log`）。源码缺陷 ERR-007 已经静态/编译验证关闭；实物 IRQ 行为仍未测试。最终文件清单、每个 cpp 职责、SHA-256 和 M3–M8 计划见英文 IMPLEMENTATION_REPORT.md；最终源码 hash 清单为 `logs/build/iteration-005-source-manifest.sha256`。USB CDC 与全部实物门槛仍待验证，M0/M1/M2 保持 IN PROGRESS。

Iteration 008 暂存安全检查初次把 HTTPS 链接误判为本地路径（ERR-014），修正边界后检查通过，无固件修改。英语错误/构建记录保留失败及修复结果；暂存源文件与已编译清单一致，main.cpp 仍为 15 行。

最终日志检查因 Windows 默认编码读取 UTF-8 失败，显式 UTF-8 重读通过（ERR-006 再现）；编译和 native PASS 证据不变，没有固件修改。

Iteration 009 保留性：driver 仅改硬件转发/常量，状态/deadline/错误优先级不变，34 个保护文件逐字节不变；Runtime 先构造唯一 Llcc68Hardware 再构造引用注入 driver，main.cpp 仍 15 行。新 seam 无显式堆分配。RAM 24,532 / 327,680、flash 363,730 / 1,310,720 bytes。真实 adapter/SPI/BUSY/ISR 竞态、Runtime/随机源/NVS/USB/RF 不属于 native 证据，M0–M2 整体 IN PROGRESS、M3–M8 NOT STARTED。

下一动作是013备份单MCU流程、采购/仪器及真实M0核对；ERR-021和replay/NVS fixture须在完整M1/M2验收前解决，已有software/native证据不代替实物。实物工作等待第二 MCU、天线与组装。

Iteration 009 审查：PlatformIO 日志显示尾空格/末尾空行使暂存检查退出 2（ERR-015）。仅规范化本轮公开日志的 EOL/显示空白，私有原输出保留，源码/编译/测试结果不变；修正后暂存检查退出 0，ERR-015 已验证关闭。

Iteration 010：实际 store 只做引用转发，错误/count/end/readback、数值 namespace/device key/schema/24-byte 格式不变；Runtime header 先构造唯一 PreferencesConfigStorage 后 store，runtime 实现和 39 个保护文件（含163旧tests）逐字节不变，48 个编译输入 hash 相符，main.cpp 15 行。无显式新分配，RAM 24,532 不变，flash 363,854（+124）。无新增 warning，ERR-005/008/009/010 保留；实际 NVS/Flash commit、断电、重启/损坏恢复、磨损/保持时间及 RF 未验证，M0–M2 整体 IN PROGRESS、M3–M8 NOT STARTED。

</details>
