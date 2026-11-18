# Adaptive-LoRa

开发基线：[Heltec 迁移 PR #4](https://github.com/gedxxe/Adaptive-LoRa/pull/4) 已合并，以 `main` 为规范基线。Iteration 015 另提 PR 审查，不自动合并。后续工作从最新 `main` 开始，保留历史迭代分支和工程记录；验证证据/发布状态见 [PROGRESS.md](docs/zh-CN/PROGRESS.md)。

**研究标题：** Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks.

Adaptive-LoRa 是独立的单射频 LoRa P2P 网络研究与嵌入式固件项目。工业能耗监测仅作为通用研究场景；协议不依赖具体电表、现场总线、回传、数据库或部署。它不是 LoRaWAN。

## 研究范围和当前状态

研究方向为可靠性感知的 Gateway 调度与保守链路自适应。**生产 TDMA、ACK bitmap、选择性重传调度和 ADR 均未开始。** Iteration 015 只增加主机可行性研究。 当前仅交付 M0–M2 固定 PHY HELLO/ACK 基线。

| 证据 | 当前基线 |
|---|---|
| 固件 / 协议 | `0.2.0-alpha.1` / `1` |
| ESP32 编译 | PASS |
| Native 测试 | 307/307 PASS：原 302 全部通过，加五项去重边界测试 |
| Capture / 模型测试 | 13/13 / 16/16 PASS，仅主机 |
| Documentation consistency | 7/7 tests PASS; checker exit 0 |
| 已实现 | 有界显式帧、角色配置、HELLO/ACK 身份检查、去重、超时/恢复、sequence/session 处理 |
| 实物 NVS/重启/SPI/IRQ/RF/天线 | NOT VERIFIED |

Host 执行实际 Client/Gateway 状态机，使用有界 fake transport、时钟及串口适配。丢包、延迟、对象重建、radio 返回码与 IRQ 输入均不等于实测，见 [driver 审查](docs/zh-CN/RADIO_DRIVER_AUDIT.md)。精确证据见 [进度](docs/zh-CN/PROGRESS.md)、[软件审查](docs/zh-CN/SOFTWARE_AUDIT.md) 及英文 [BUILD_MEMORY](docs/en/BUILD_MEMORY.md)。

## 硬件与暂定PHY

唯一活动目标为 **Heltec WiFi LoRa 32 V3.2 HF**，ESP32-S3FN8、SX1262、8 MB Flash和CP2102/UART0；不假定PSRAM。采购选型是HF902–928 MHz选项，到货数量和PCB标记尚未确认。RadioLib固定7.8.1，GPIO8–14接radio，DIO3 TCXO1.8 V，DC-DC；串口115200，原生USB CDC关闭。OLED、电池监测未实现。

保留915 MHz/BW125/SF7/CR4/5/preamble8/explicit/CRC/sync0x12/10 dBm/SPI1 MHz，SF9控制仍预留。915 MHz Moxon作为设计基线，制作、gain和matching未实测。上电前阅读 [硬件说明](docs/zh-CN/HARDWARE.md) 和 [迁移审查](docs/zh-CN/HARDWARE_MIGRATION_AUDIT.md)。过去硬件描述在历史记录/Git中保留。

## 架构原则

一份镜像从 NVS 加载有效 GATEWAY/CLIENT 角色。配置缺失、损坏或不支持时进入 provisioning，禁止应用 TX。保存并受控重启后才应用编辑，不通过相同硬件自动推断角色。

RadioTransport 引用分离 app 与 RadioDriver 状态策略；非拥有 RadioHardware 引用指向唯一 SX1262/SPI/GPIO owner Sx1262Hardware，native 故障测试执行实际 driver；NvsConfigStore 也通过五操作 ConfigStorage 在 native 执行，PreferencesConfigStorage 唯一拥有实际 Preferences，见 [存储审查](docs/zh-CN/CONFIG_STORE_AUDIT.md)，host readback 不证明实物持久性；Esp32Firmware先拥有两个concrete adapter，再构造引用注入FirmwareRuntime，native执行实际协调器/store/driver/apps/console，见 [runtime审查](docs/zh-CN/RUNTIME_AUDIT.md)；SDK shim仅模拟时钟/串口/reset/raw RNG，不证明实物熵或重启。随机生成最多8次raw调用；身份不可用禁止普通TX、记录SESSION_ERROR并按1秒间隔重试。修正时间边界和SDK/BUSY/分配限制见 [依赖预算审查](docs/zh-CN/DEPENDENCY_BUDGET_AUDIT.md)。codec/config/sequence 可在 host 测试，main.cpp 为 15 行入口。[架构文档](docs/zh-CN/ARCHITECTURE.md) 区分协议核、radio adapter、platform 服务和应用 payload/event 接口四个目标边界。当前 platform 仍依赖 ESP32/Arduino，通用应用提交/交付 API 尚不存在。协议核没有 Modbus、电表模型、Wi-Fi/MQTT、PostgreSQL 或 dashboard 集成；不能声称目标架构已经完成。

## 仓库结构

`include/` 集中 pins/PHY/限制；`src/app/` 为 runtime/NVS/console；`src/common/` 含 protocol/radio/config/diagnostics；`src/gateway/` 与 `src/client/` 为生产状态机；`test/` 为 native 单元/integration 和 fake；`docs/en/` 为规范、`docs/zh-CN/` 为中文配套；`logs/` 为脱敏证据，`tools/` 为 bench 捕获工具。

## 编译与测试

安装 PlatformIO Core 和支持 C++17 的 host 编译器，在根目录执行：

```sh
pio run -e heltec_wifi_lora_32_V3
pio test -e native
```

目标为 Arduino / heltec_wifi_lora_32_V3，RadioLib7.8.1。已有Core6.1.19、pioarduino/espressif32 55.03.311和Arduino3.3.11/IDF5.5.5保持，使用Xtensa GCC14.2.0；host GCC6.3.0、native1.2.1、Unity2.6.1。platform尚未固定，其他机器须记录实际依赖；hash清单和日志标识各次验证快照。

## 配置与后续实物流程

串口 115200。支持 help、status、config show、config set role gateway|client、network-id/node-id/gateway-id、config save、reboot；可设 hello-interval-ms 1000–3600000，默认事务结束后等待 5000 ms。

初始 network/node/gateway 为 Gateway 1/1/1、Client 1/2/1；角色命令不修改 ID，应全部明确设置。config show 显示暂存值，status 显示启动加载值。Network 1–65535，node/gateway 1–65534。实物流程见暂缓的 [BENCH_RUNBOOK](docs/zh-CN/BENCH_RUNBOOK.md) 和 [模板](logs/test/RUN_RECORD_TEMPLATE.md)；当前没有烧录或 RF 验证。

## 开发路线

1. M0–M2 继续软件验证及依赖时延/分配边界分析。
2. 采购/制作/组装后，验证 NVS/重启、电气/IRQ 和双设备 HELLO/ACK。
3. M3 测量切换/漂移后实现单 Client beacon/TDMA。
4. M4–M6 逐步实现 bitmap/选择性重试、版本化保守 ADR、同步恢复。
5. M7–M8 受控天线对比及明确标注的逻辑节点仿真。
6. 其他嵌入式应用集成前，另行审查 payload/event 和 platform-service 接口。

验收见英文 [MILESTONE_TASKS](docs/en/MILESTONE_TASKS.md) 和 [TEST_PLAN](docs/zh-CN/TEST_PLAN.md)。本次不激活后续 MAC。候选 M3 合同、airtime 预算和未知时序见 [TDMA_FEASIBILITY](docs/zh-CN/TDMA_FEASIBILITY.md)。

## 研究和复现原则

保留时间顺序、失败尝试、错误修复、命令、版本及 compile/host/physical 的证据区别。历史公开副本只脱敏身份和路径，不改结果；旧 hash 标识原始工件，详见 [身份审查](docs/en/IDENTITY_AUDIT.md) 和 [日志说明](logs/README.md)。公开假设、受控条件和限制，不虚构容量、天线增益或实物验证。未经授权不分发论文全文。依赖外置，许可见 [THIRD_PARTY](THIRD_PARTY.md)；本次未选择项目统一再分发许可证。

修改前阅读 [AGENTS.md](AGENTS.md)。英文为主文档，简体中文为第二语言。

实物准备入口：[M0-M2 readiness](docs/zh-CN/PHYSICAL_ACCEPTANCE_READINESS.md)，含集成射频板与 pair 门槛、采集/CSV和ERR-021/SDK启动限制。流程未执行；上传/改配置前先私有备份，证据用新目录。


其他主机检查使用已安装的 PlatformIO Python，分别执行：

```sh
python -m unittest discover -s tools -p test_capture_bench.py -v
python -m unittest discover -s tools -p test_tdma_feasibility.py -v
python -m unittest discover -s tools -p test_check_consistency.py -v
python tools/check_consistency.py
```

一致性检查从源文件读取当前版本和环境，排除历史记录；分析模型包含 216 个场景，
未知切换/处理/guard 不填零。两者都不属于实物测试，M0–M2 实物验收仍 IN PROGRESS。
