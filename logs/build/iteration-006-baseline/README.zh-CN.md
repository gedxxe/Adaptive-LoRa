# Adaptive-LoRa 固件

本项目用于开发基于 ESP32-C3 Super Mini 与 Ai-Thinker
Ra-01SCH（LLCC68）的私有 LoRa P2P 通信固件。

## 当前测试硬件

| 设备   | 硬件                           | 目标角色 |
|--------|--------------------------------|----------|
| Unit A | ESP32-C3 Super Mini + Ra-01SCH | Gateway  |
| Unit B | ESP32-C3 Super Mini + Ra-01SCH | Client   |

两台设备使用同一份固件。设备角色保存在 NVS
中，并在启动时读取。若角色配置缺失或无效，设备进入串口配置模式，不发送无线数据。

当前测试天线为 915 MHz
Moxon。频率、发射功率和天线使用仍需完成当地法规及射频验证。

## 硬件注意事项

当前模块是 **LLCC68，不是 SX1262**。必须使用 RadioLib 的 `LLCC68`
类，不要直接复制其他 Heltec SX1262 项目的接线或驱动设置。

## 软件包内容

- `AGENTS.md`：Codex 必须遵守的工程规则。
- `CODEX_START_PROMPT.md`：第一阶段开发任务。
- `platformio.ini`：一个 ESP32 固件环境和一个 native 测试环境。
- `src/`、`include/`、`test/`：M0–M2 实现及 native 测试。
- `docs/zh-CN/BENCH_RUNBOOK.md`：硬件前置检查、配置和双串口记录流程。
- `docs/en/`：英文技术文档，作为唯一规范版本。
- `docs/zh-CN/`：简体中文配套文档。
- `docs/en/BUILD_MEMORY.md`：每次开发/构建迭代的追加式记录。
- `docs/en/ERROR_LOG.md`：错误、复现步骤、原因、修复和验证状态。
- `docs/en/DECISIONS.md`：架构和协议决策记录。
- `logs/`：保存完整构建和测试输出。

## 第一阶段目标

第一轮只实现 M0–M2：

1.  确认实际开发板、模块、引脚、电源和天线连接。
2.  在两台设备上初始化 LLCC68。
3.  实现串口角色配置和 NVS 保存。
4.  实现固定 PHY 的 `HELLO` / `ACK`。
5.  为帧编解码、错误帧、角色配置和重复包处理添加本地单元测试。
6.  记录实际构建与测试证据。

第一轮不要实现完整 TDMA、ACK bitmap、重传调度或 ADR。

## 构建命令

``` bash
pio run -e esp32c3_supermini
pio test -e native
pio run -e esp32c3_supermini -t upload
pio device monitor -b 115200
```

候选版本 `0.2.0-alpha.1`：ESP32 编译通过（退出码 0），44 个 native 测试通过（退出码 0）。完整记录见 `docs/en/BUILD_MEMORY.md`。未执行烧录、NVS 实物配置或双设备射频测试，M0–M2 保持 IN PROGRESS。

本机安装的 `espressif32` 实际为 pioarduino 55.03.311 / Arduino-ESP32 3.3.11；跨机器复现时需核对包版本。RadioLib 固定为 7.8.1。原生 USB CDC 是待实物验证的板卡假设。

`esp32-c3-devkitm-1` 是通用 PlatformIO 板卡目标。正式使用前必须确认实际
Super Mini 版本、Flash 容量和 USB 串口行为。

## 配置角色示例

Gateway：

``` text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config save
reboot
```

Client：

``` text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config save
reboot
```

`config show` 显示暂存配置；`status` 显示启动时加载的配置及有效性。`config save` 验证完整配置并读回 NVS；重启后才生效。角色命令不修改 ID，应按示例明确设置全部 ID。NVS 缺失、损坏或版本不支持时不初始化无线模块，禁止 TX。

可选命令 `config set hello-interval-ms <1000..3600000>`，默认事务结束后等待 5000 ms 再发新 HELLO；保存后重启生效。Network ID 可为 65535；节点和 Gateway 地址 0/65535 保留。

验收流程见 [测试操作手册](docs/zh-CN/BENCH_RUNBOOK.md)，实物记录模板见 `logs/test/RUN_RECORD_TEMPLATE.md`。本次仅枚举到蓝牙串口，未识别到 ESP32 串口。

开发前先阅读 `AGENTS.md`。
