# M0–M2 软件审查与验证

发布说明：iteration 008 仅中和本历史报告的身份/路径标签，不改变原测试结果、顺序、版本或实物限制；当前迁移验证另见 BUILD_MEMORY 与 IDENTITY_AUDIT。

英文完整报告为 `docs/en/SOFTWARE_AUDIT.md`。源码修改前已完成只读审查：GatewayApp、ClientApp、FirmwareRuntime、RadioDriver、FrameCodec、DeviceConfig、NVS，以及 sequence/console/logger 和本机依赖源码。修改前快照为 `logs/build/iteration-006-baseline/`，hash 清单为 `logs/build/iteration-006-baseline-manifest.json`。当前目录无 Git 仓库，将使用 Git no-index 比较，不初始化 Git、不提交。

实际库存：2 个 RA-01SCH/LLCC68、1 个 ESP32-C3 Super Mini；第二个 MCU 尚未购买，Moxon 尚未制作，未组装双设备。此前库存描述为计划配置，现已更正。当前仅做 M0–M2 软件验证。

## 主要结论与边界

- FrameCodec 先检查长度再复制，ACK 长度短路检查先于字段读取；配置有完整校验和有界 blob，暂未在审查中发现越界缺陷。这不是形式化内存安全证明。
- 原 native 只测 helper，不运行两个生产状态机。已先提出最小 seam，再实现 6 方法 RadioTransport 引用；RadioDriver 仍唯一拥有 LLCC68/SPI/IRQ，Runtime 保留具体 begin/poll/stop。无框架、factory 或堆所有权。
- 测试编译实际 app/codec/tracker/config/logger .cpp；只有时钟、Serial 和 transport 是测试适配。Fake transport 只产生完成的包/错误事件，不模拟 RF、SPI、IRQ、硬件 CRC 或 NVS。
- M2 不新增自动重传；测试重放捕获的完全相同字节，核对 network/source/boot/sequence/payload。超时后新事务仍使用新序号。
- Gateway 去重先于 ACK，ACK 丢失后允许重复 HELLO 再 ACK；缓存仍为 8 个 session、每个 64 序号窗口。淘汰和 Gateway 重启会丢历史，不提供认证或持久化重放保护。
- Client 使用 64 位报告间隔、32 位 wrap-safe ACK/恢复截止时间；将通过虚拟时间测试边界、迟到/旧 ACK、重启、重复事务和完整序号耗尽。
- Runtime 和实际 Preferences/NVS 本次只做静态审查，不声称被 app integration tests 执行。NVS commit 是同步调用，BUSY 等待也有依赖内超时；控制返回前不能执行应用 deadline 检查（ERR-008）。
- 本机 HWCDC availableForWrite/write 可等待 mutex，默认超时 100 ms，write 可多次等待 ring buffer。有界日志队列不能证明端到端非阻塞（ERR-010）。Native 只测队列回压和短写，不测 USB 延迟。
- RadioLib 默认 SPI 临时缓冲区使用 new[]；应用固定数组不代表整个依赖热路径无分配。本次不暗改内存模式/栈预算（ERR-009）。
- ISR 只置 volatile flag；清除标志的窗口依赖下一次对锁存高电平 DIO1 的采样，不能用 host 测试证明硬件竞态安全。随机 boot ID 生成也假设硬件随机源可前进；故障常量源可能阻塞拒绝循环。均保留为审查边界。
- 第一轮 native 编译暴露旧 MinGW 隐藏 PRI* 宏的问题（ERR-011），尚未执行新测试；计划使用 C++ `<cinttypes>` 修正兼容性，保持格式及状态行为不变。

最终测试数、结果、完整 diff 和仍未验证的边界将在英文报告及 PROGRESS/BUILD_MEMORY 中记录。无物理 NVS 或 RF 验证，M0–M2 不能标记完整 PASS；不启动 TDMA/bitmap/retry scheduler/ADR 或外部集成。

## iteration-007 实际结果

版本保持 0.2.0-alpha.1 / 协议 1。ESP32 编译退出码 0（15.89 秒，`logs/build/iteration-007-esp32.log`）；native 92/92 PASS、0 失败/跳过、退出码 0（24.35 秒，`logs/test/iteration-007-native.log`）：生产状态机 38、codec 18、config 13、console 10、sequence/timing 7、logger 6。全部 case 名称见 `logs/test/iteration-007-scenarios.md`，未测量行/分支覆盖百分比。

38 个 integration 场景通过：200 次重复事务及独立 65,537 次完整序号耗尽（两端均换 session 后才复用序号）；丢包/ACK 丢失；ACK deadline 前 1 us、恰好和之后；旧/重复/错误身份 ACK；44 种畸形 HELLO、45 种畸形 ACK；相同字节最大 Payload 重放；TX/RX 故障和三次恢复上限；接收错误不延长 deadline；对象重建；旧 session/乱序/stale；32 位 wrap 与 64 位间隔；停止新事务门控。

本轮没有复现应用包处理/状态缺陷；唯一复现的生产源码可移植问题为 `<inttypes.h>`，已在 client/logger 改为 `<cinttypes>`，格式和状态语句不变。新 transport header 30 行，其余生产修改仅签名/依赖/override/include。静态比较确认 main.cpp（15 行）、Runtime/NVS/driver/codec/config/tracker 实现、PHY/pins/版本均与修改前相同。接口不引入运行时堆分配。

真实 app/codec/tracker/config/console/logger 的列举行为以及 ESP32 编译属于软件证据；包投递/丢失/错误、对象重建、虚拟时钟、Serial 字节容量属于测试刺激仿真。Fake airtime 固定 1000 us，不是实测；没有 RF/IRQ/SPI/NVS 模型或多节点容量结论。

实际 Runtime 重启、随机源、NVS/掉电、driver poll/IRQ/SPI/BUSY/CRC、Flash/USB 延迟以及全部电气/RF/合规事项均未验证；ERR-005/008/009/010 保留。两个模块/一个 MCU/无制作天线，未组装双设备，M0–M2 仍 IN PROGRESS。

当前无 Git 仓库（rev-parse 128）；没有初始化或提交。完整 no-index diff 为 `logs/build/iteration-007-complete.diff`，清单为 `logs/build/iteration-007-changed-files.txt`，修改前/后快照保留在 logs/build。最终 source hash 清单为 `logs/build/iteration-007-source-manifest.sha256`，二进制 SHA-256 为 `1eb8c7fafeb36e87035095a9c357a9f024e8164c88e43a88274831c9618a34da`。没有增加自动重传或后续 MAC。

## Iteration 009 补充 — 已执行生产 driver 策略

此前 native 不覆盖 driver poll/IRQ 分类的描述对应 iteration 006–008。本轮增加最小 RadioHardware 引用和 Llcc68Hardware owner，实际生产 RadioDriver 的 71 个确定性故障用例全部通过；既有 92 个 regression 通过，总计 163/163，native/ESP32 命令退出均为 0。先审查后改源码，状态/错误优先级/逐条结果和保留性证据见 RADIO_DRIVER_AUDIT.md。

新验证仅针对 driver 策略，host 绕过真实 adapter、RadioLib 内部/SPI/BUSY/实物 IRQ 竞态和指标；Runtime/NVS/随机源/USB/RF 仍未验证。未复现需修正既有策略的缺陷，不增加后续 MAC 或 platform/application API。

## Iteration 010 补充 — 实际配置存储策略已执行

通过五操作 ConfigStorage 引用执行实际 NvsConfigStore，单一 PreferencesConfigStorage 唯一拥有并原样转发 Preferences；验证/codec/错误顺序/清理/readback 仍在实际 store。新增 56 与既有 163 全部通过（219 总计），ESP32 编译通过，命令均退出 0。修改前审查、A–E 映射、不可注入防御性 encode 分支见 CONFIG_STORE_AUDIT.md。

数值 namespace/key/schema/24-byte 格式和 config/radio/app/console/runtime 实现保持；失败输出和 staged-until-reboot 不变。Native 不验证真实 Preferences/NVS、Flash commit/断电持久性、重启/损坏恢复、wear/retention/回滚。Runtime/随机源/串口时序及物理 RF 仍未验证；建议后续验证 runtime boot/provisioning/reboot 调度。
