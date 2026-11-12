# Iteration012 — Boot-ID活性、依赖时延与内存预算

英文同名文件为完整规范/source行号记录，本文件同步技术决策、结果和操作边界。

## 基线、复现与修正

工作区起始干净，fetch核实011 `8fa0a473b6cc0905ddff53e4b9a5f22806e058e2`，012直接从指定SHA创建，不基于main `5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861`。修改生产前先记录英文审查/最小方案。固件0.2.0-alpha.1、协议1、RadioLib7.8.1保留，实际Arduino3.3.11/IDF5.5.5。

`python tools/reproduce_boot_id_liveness.py` 从Git取得原011 runtime函数，真实依赖编译，raw64 guard有限中断固定0/previous100/交替0和100；三种输入在guard前均不返回，编译/probe退出0，无无限测试。实际runtime初始10案例两次均9FAIL/1PASS、退出1，完整日志保留。首次wrap fixture还触发Client已到期TX，改Gateway隔离reset；生产app先命令顺序未改。修正后同10案例全过。

生产generator每次最多8次SDK调用，0为明确失败；保留uint32(previous)签名。Counter.renew拒绝0/上次ID，失败不覆盖旧boot或耗尽计数。两个app记录SESSION_ERROR/boot_id_unavailable，禁止普通TX（包括Gateway ACK），按尝试起点1s deadline重试，pending reboot不恢复；成功记录SESSION_NEW/identity_recovered并seq0。可继续监听/串口help/status/show/save/reboot。无时间/MAC/hash/常量fallback，无逐包持久写，旧ID恢复时仍会拒绝。

延迟read使过期ACK被接受、延迟finishTX令ACK窗口偏早、save/stop消耗100ms等待、drain等待使reset检查陈旧均已复现（ERR-020）。修正仅为poll后刷新app时间、stop完成后起100ms、drain后用当前micros判reset。保留app先串口、stop失败仍reset、不等日志排空、driver/app调用内观察时间/recovery策略。不能抢占阻塞操作或记录真实IRQ时刻。

## 熵与身份边界

已核对 [IDF5.5.5 ESP32-C3 RNG指南](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/system/random.html) 及已安装C3源码；用户stable链接当前解析6.1，不应当作安装版本。Bootloader暂时启用SAR噪声后在app前禁用；app启动/后续rollover未启用主熵源时只能按伪随机看待，secondary oscillator不提供主源保证。ESP RF指Wi-Fi/Bluetooth，不是外置LLCC68。bootloader_random_enable会改SAR ADC2/clock/reference，需独占且在ADC/RF前disable。本轮未自动启用任何额外源。

合法非零/不同previous、有限调用、熵质量、跨启动唯一、安全认证为不同属性。8次上限假设每个SDK调用返回，不是实测微秒WCET。32位会碰撞，只拒绝RAM中紧邻上次ID，无跨power-cycle保证，不提供认证。未来可另行设计版本化持久boot-counter，但须考虑断电原子性/损坏/磨损/wrap/重置前TX门槛；本轮NVS schema/key/namespace不改。

## 依赖时延清单（均未实测）

| 操作/调用点 | 已安装源码行为 | 对deadline影响 |
|---|---|---|
| Module构造/LLCC68 begin | 构造一次new ArduinoHal；begin才SPI/GPIO/reset/config | 无效config也构造，不能声称无分配；setup同步等待 |
| SPI命令/锁 | Module pre/post BUSY各检查1000ms；SPI参数锁启用时portMAX_DELAY，逐字节同步传输 | 多命令可累加，非全调用WCET；无BUSY引脚的50/1ms分支不是当前配置 |
| startTransmit | stageMode/launchMode准备IRQ/buffer/setTx同步，RF随后异步 | TX ramp另有while BUSY yield无显式timeout，firmware2s poll deadline无法抢占（ERR-021） |
| finishTransmit/poll | 同步读/清IRQ和standby | 在poll入口deadline之前观察的TX_DONE可在cleanup后返回；app现用post-poll时刻，不等于实物完成时间 |
| startReceive | 同步设置IRQ/参数/setRx，连续RF异步 | RX无软件时长上限，仍可能等待SPI/BUSY |
| RX完成/read/metrics/finish | 同步standby、length、重复status/IRQ/read/clear、RSSI/SNR、cleanup | 多次scratch分配/命令，post-poll新时间可拒绝晚读ACK |
| stop/standby | NOP唤醒/SET_STANDBY同步 | reset起点现为stop返回后，失败仍等100ms后reset |
| ISR | 仅volatile flag；poll再采GPIO/flag并清标志 | 实物edge/clear race/getter无错误返回仍未测，无ISR分配/SPI/log |
| NVS open/read/close | Preferences转NVS锁、handle及blob查询 | 同步锁/flash/cache，无firmware超时 |
| NVS write/commit/readback | set_blob执行writeItem/多页/可能erase；commit在已安装版本验证后直接OK；store再只读load比字节 | 主要flash工作在set_blob，不能用fake推断commit延迟/断电安全 |
| Serial available/write | HWCDC默认mutex100ms；connected ring发送每次等待默认100ms，连续失败20次退出，成功重置计数 | capacity不是预留，disconnect另有FIFO策略，不可把这些数当实测全调用上限 |
| logger/runtime | 16x512队列，每loop最多64输入/提交128输出字节 | 字节有界不等于时间有界；save/输出可延后ACK/恢复到下一loop；新时间样本不消除依赖阻塞 |
| RNG | 最多8次SDK调用；SDK按cycle计数等待读间隔 | 不证明熵/硬实时；失败后1s重试 |

精确包内source路径/行号见英文表，hash见 `logs/build/iteration-012-dependency-sources.json`。ERR-021只是源码风险，未注入真实BUSY故障；不修改第三方库/flags，不增加task/watchdog/scheduler。硬实时验收仍受这些风险阻挡，本轮完成的是审查与软件修正。

## 分配与内存预算

项目runtime、inline optional apps、store/driver/console/logger、frame/sequence均无新增显式堆分配。Logger持久8228（队列8192+metadata），event栈数组512+192+64+24=792，不含ABI/libc/nested开销；runtime局部320、Client finish256、TX bytes96及Frame等还需叠加调用链分析。Host测试string/output可分配，不是ESP堆证据。

RadioLib Module每个成功stream命令两次new[]，正常/error路径释放，IRQ/standby/TX/RX/metrics均会反复触发。write payload为len+2每份（HELLO30、ACK39、max98）；read为len+3（max99，两份198），不含allocator/HAL/SDK，不是整库peak。没有局部null检查/项目catch，OOM无可恢复RADIO_ERROR保证，精确失败/fragmentation未注入。STATIC_ONLY默认0；启用后两份自动268-byte数组占536栈，仍保留ArduinoHal/SDK分配，有容量/嵌套栈风险，因此不改。

SDK含SPI mutex/bus、USB默认RX queue/TX ring各256及锁/IRQ服务、NVS handle/namespace/page/blob节点、RTOS task栈。部分返回NO_MEM/0，部分仅记录/终止，无完整项目OOM策略。安装配置loop栈8192、main4096是配置分配，非实测headroom；任务栈/allocator开销不包含在compiler data+bss之内。

| Target sizeof | 011 | 012 |
|---|---:|---:|
| FirmwareRuntime |8984|9000|
| Esp32Firmware |9552|9568|
| GatewayApp /ClientApp |344 /184|352 /192|
| RadioDriver /Llcc68Hardware |20 /552|20 /552|
| NvsConfigStore /Preferences adapter |4 /12|4 /12|
| SerialLogger |8228|8228|
| Frame /RadioEvent |92 /120|92 /120|
| SequenceTracker /Counter |200 /8|200 /8|

对象包含关系重叠，不能直接相加。Compiler RAM24548→24564（+16），flash363842→364308（+466）。最终 .iram0.text58428、.flash.text226000、.dram0.data7980、.dram0.bss16584、.flash.rodata71900；.eh_frame25400另列，总image390256，与compiler flash摘要/实际padded bin有区别。不要简单加debug/dummy映射段。GDB仅查询ELF，无设备；初次printf Windows引号失败保留，p sizeof重试成功。完整size/nm日志、binary hash在iteration-012证据。

## 最终结果与实物计划

`pio test -e native`退出0，**290/290 PASS**，77.575s（runner80.660s），旧266 cpp字节和有序名称/结果不变，新增24。`pio run -e esp32c3_supermini`退出0，PASS31.748s（runner32.399s），增量编译；仅已知ERR-005 CDC warning一次，native无warning。53输入hash匹配冻结树，39文件逐字节保护。逐条结果 `logs/test/iteration-012-scenarios.md`，无行/分支覆盖百分比。

新24覆盖初始坏源/最大值/零前缀、第8/9边界、重试频率/重启抑制/wrap、counter65536耗尽保留、Gateway三种rollover失败及Client失败、恢复拒绝100后用200/seq0，及delayed read/TX finish/save/stop/serial/ACK/reset/recovery。测试使用真实生产模块，输入/clock/backend/reset为模拟。版本0.2.0-alpha.1/协议1/RadioLib7.8.1、wire/identity/NVS24-byte schema及数值namespace/device key、pins/PHY/单镜像/命令不变，main不变15行。文件职责清单见iteration-012-review.json。

没有烧录、实物NVS/熵/RF/重启验证。库存2个RA-01SCH/LLCC68、1个ESP32-C3 Super Mini；第二MCU待购、915MHz Moxon待制作、双设备未组装。PDR/RSSI/SNR/供电/时延/堆peak/栈高水位均NOT MEASURED。M0–M2实物IN PROGRESS，M3–M8 NOT STARTED。

组装后用GPIO/逻辑分析记录SPI/BUSY/IRQ、TX/RX start/finish、poll/app/serial/save/stop/reset，含安全fault注入及USB host阻塞/断开；在页压力与受控断电/重启下测NVS，保留raw日志。重复包/配置/冷暖启动/错误恢复前后测free/min/largest heap和task高水位；另审查OOM及主熵源冲突。硬时限宣称前先处置上游BUSY与锁/返回状态限制。本轮不声明实物可靠性。

RAM口径补充：linker报告DRAM82,992/321,296，包含instruction58,428 +data7,980 +bss16,584；PlatformIO24,564摘要只计data+bss。两种剩余数均不是实测可分配/minimum heap，须考虑IRAM/DRAM别名、SDK保留、启动分配和task栈，不能再次加dummy映射段。
