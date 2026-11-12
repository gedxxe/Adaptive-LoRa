# M0–M2 双设备测试操作手册

这是尚未执行的实物流程。英文规范见 `docs/en/BENCH_RUNBOOK.md`。编译和 host 测试通过不等于硬件或 RF 验收。

库存更正：两个无线模块、一个 MCU，第二个 MCU 尚未购买，Moxon 尚未制作，双设备未组装。本手册暂缓执行，当前只做 host 软件测试。

## 上电/烧录前

1. 拍摄两台 Super Mini、Ra-01SCH 标记/版本、焊盘标签和引脚方向。按建议图逐项确认 GPIO4/5/6/7（SPI）、GPIO1（RESET）、GPIO0（DIO1）、GPIO10（BUSY）的实际连通性，不能把建议图当成实测。
2. 确认 Flash 和 USB 路径。候选固件启用 GPIO18/19 原生 USB CDC；如为 USB-UART 板，应明确移除两个 USB build flag 并重新编译记录。GPIO20/21 为 UART0；避免使用绑定位 2/8/9 控制无线模块。
3. 检查独立稳压 3.3 V、共地、去耦和发射瞬态电压，确保模块端始终在 2.7–3.6 V 内。连接合适天线/负载和 50 欧姆馈线后才允许有效角色发射。除非具体转接板要求，否则 TXEN/RXEN 不接 MCU。
4. 记录测试频段/信道、功率、含天线增益/线损的 EIRP、接入规则及场地/设备许可；915.0 MHz 只是候选参数，不代表合规。
5. 候选参数为 XTAL（DIO3 电压 0）、LDO、RadioLib 默认 DIO2 内部 RF-switch 控制；需按实际模块确认。库的 LLCC68 驱动选择/初始化并非独立芯片鉴定，库可能接受 SX1261 标识字符串。

## 同一个镜像

```powershell
pio run -e esp32c3_supermini
pio test -e native
pio run -e esp32c3_supermini -t upload --upload-port COM_GATEWAY
pio run -e esp32c3_supermini -t upload --upload-port COM_CLIENT
```

必须替换端口占位符。本机可使用 `python -m platformio`。受限沙箱内包缓存写入和旧版 host 工具启动可能失败，需在经审核的沙箱外执行。本机的 `espressif32` 是已安装的 pioarduino 55.03.311，Arduino-ESP32 3.3.11；不能假设另一机器相同。重定向包列表遇到 cp1252 编码错误时，可为该进程设置 `PYTHONUTF8=1`。

## 115200 baud 配置

无有效 NVS 的设备必须显示 PROVISIONING、UNSET、配置无效且无线模块禁用。`scope=inactive_defaults` 是暂存建议，不能视为已加载的有效参数。

Gateway：

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 5000
config show
config save
status
reboot
status
```

Client 使用相同流程，但设为 `role client`、`node-id 2`。角色命令保留 ID。`config show` 是暂存值；`status` 是启动加载值。保存应返回 `verified_reboot_required`；当前应用直到受控重启前保持原配置。Gateway node 必须等于 gateway ID；Client 必须不同。Network 65535 合法；节点地址 0/65535 保留。

必须记录的反例：未设角色即保存、角色/ID 冲突、符号数字/溢出/尾部垃圾、节点 0/65535、超长命令以及随后正常 `status`。保存后断电重启确认持久化。事务期间修改角色并保存，确认旧应用仍运行；重启请求停止新 TX，当前 TX 完成或超时后再重启。

Host 测试覆盖配置表示的缺失/截断/损坏和 schema 校验。实物损坏/不兼容 NVS 需专用测试 fixture：截断 blob、校验损坏、校验正确但 schema/role 不支持。先备份配置，通过专用 NVS 测试 harness 注入，再用生产镜像重启检查无 TX，随后恢复有效配置。生产串口没有损坏 NVS 注入命令，本次未执行这类实物测试。不得把整片擦除作为未记录步骤；不得逐包写 NVS。

## 无线初始化与捕获

1. 先启动 Gateway，再启动 Client。两端验证 `RADIO_INIT code=0`、加载角色/ID、BUSY/DIO1/reset、无 brownout/watchdog 循环。初始化失败先查电源/接线再重启，不自动替换驱动。
2. 固定 SF7/BW125/CR4/5、显式 Header、CRC、前导码 8、sync 0x12、10 dBm。SF9 控制 PHY 在 M2 不使用；BW125 不允许 SF10–SF12。
3. 关闭串口监视器，再用 PlatformIO Python 内置的 pyserial 双口捕获。脚本只发 `status`，按有界 chunk 写入新目录；某些板开串口会因 DTR/RTS 复位，必须记录。默认 1200 秒只是为 200 次尝试预留时间，实际数量必须从日志确认。

```powershell
python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 1200 --out logs/test/RUN-YYYYMMDD-HHMM
```

替换两个端口占位符。本次只识别到蓝牙串口，不能把它们当成 ESP32 端口。

4. 将 `logs/test/RUN_RECORD_TEMPLATE.md` 复制为该目录的 `RUN_RECORD.md`，填写全部实测条件。原始字节保存为 `gateway.log` / `client.log`，时间/端口/捕获异常保存为 `capture.json`。结束捕获并关闭端口后，再打开 monitor 执行最终 `status`，记录 `log_dropped`。
5. 分别计数 Client `TX_START result=hello`、完成的 `TX_DONE result=hello_sent`、`ACK_RX result=accepted`、`ACK_TIMEOUT`、`HELLO_FAILED`。`LINK_COUNTS` 自开机累计。Gateway `HELLO_RX` 为新身份，`DUPLICATE` 为再 ACK；`ACK_TX` 仅表示开始，`TX_DONE result=ack_sent` 才表示完成。Header sequence 属于各发送方，ACK_TX 同时记录原 HELLO 身份。
6. 按 network/source/boot/sequence 匹配至少 200 次真实尝试，记录 Gateway 唯一接收、ACK 完成、Client 接受/超时/失败、两端 RSSI/SNR 分布和 `transaction_us`，明确分母。丢日志/不完整记录不能用于验收。短距离 ACK 成功率目标 >=99%，不代表现场容量。
7. 停止 Gateway 接收，观察 Client 超时；恢复后新 HELLO 应能再次成功。M2 不重传。错误 ACK 身份、畸形帧和重复身份已有 native 测试，实际 RF 注入仍需单独 fixture/记录，不能混称。

`CRC_ERROR` 来自库可见的 CRC/Header IRQ，不是完整校准计数。未知字段为 N/A。RX 启动最多连续恢复 3 次、间隔 1 秒；持续故障须查明后重启。TX 软件截止 2 秒，ACK 等待为 TX 完成后 1 秒；事务结束后按配置间隔发送新的 HELLO。Sequence 耗尽前更换非零 session。M2 不含 TDMA、重传调度或 ADR。

## 验收与后续

M0 关闭硬件、供电、天线、USB 和 RF 许可检查；M1 验证双设备初始化、持久化、无效配置无 TX、稳定串口；M2 需要真实 HELLO/ACK/超时/重复/错误身份及 >=200 次尝试记录。全部证据存在前维持 IN PROGRESS。M3–M8 计划见英文 `MILESTONE_TASKS.md` / `TEST_PLAN.md`；M3 需先测切换、IRQ、漂移再定保护时间，本次不含后续源码。
