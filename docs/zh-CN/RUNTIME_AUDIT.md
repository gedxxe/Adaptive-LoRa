# Iteration 011 — 固件 Runtime 生命周期审查

英文 RUNTIME_AUDIT.md 为完整规范记录；本文件同步决策、结果及边界。

## 修改前与最小 seam

基线为已 fetch 核实的 iteration-010-config-store-verification `3902de90058ec775587105c5c3edb4914853a9d8`，工作区干净。远端 main 为外部合并 `5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861`；011 直接基于指定010，不基于main。修改源代码前已记录英文 audit/proposal。

原顺序：Serial.begin115200，实际 store.load，logger/console 与 boot/config 日志；无效配置先于radio/app/RNG返回。有效配置先radio.begin，成功才emplace唯一对应app。loop捕获micros，poll、app.update、最多64输入字节、待重启stop、最多128日志字节、deadline reset。show为暂存，status为已加载；save后角色/ID仅在新实例启动生效。

FirmwareRuntime只接收已有ConfigStorage/RadioHardware引用，内部仍是实际store/driver/console/logger/apps。Esp32Firmware依次拥有PreferencesConfigStorage、Llcc68Hardware、唯一runtime，逆序析构保证引用寿命；main仍15行。无新接口体系、factory、task、第二radio或runtime模型。Firmware用标准optional，旧GCC6.3用experimental optional，namespace std不改；runtime.cpp仅inttypes.h换cinttypes。Native SDK header适配有界串口输入、共享时钟、reset/reason/raw random，执行实际freshBootId，不复制状态策略。

## 执行结果

- 最终 `pio run -e esp32c3_supermini`：退出0，PASS，143.01s（runner143.563s）。
- 修正后 `pio test -e native`：退出0，**266/266 PASS**，36.68s（runner37.234s）；旧219案例源码及有序名称/结果不变。
- 新增实际runtime47：boot11、provision8、serial7、radio4、reboot10、session3、diagnostics4。覆盖无效配置零硬件调用、load/radio/app顺序和角色互斥、暂存/保存/新启动、存储错误、输入预算/分片/CRLF/畸形恢复、实际radio事件/恢复、TX done/timeout后stop/reset、失败stop、重复reboot与wrap、backpressure及session。
- Gateway序号耗尽用65,537次实际HELLO/ACK，验证零值/previous ID拒绝，boot100变200且sequence归零；不额外init或load。
- 首次native：219通过，runtime因Unity未启用64位assert编译失败（ERR-017）。native统一开启UNITY_SUPPORT_64后266通过，未删除/截断断言；保留失败与重试日志。ERR-016为受限runner停滞，host-shell重试通过，根因未确定。

逐条266结果：`logs/test/iteration-011-scenarios.md`；最终build/native日志分别为 `logs/build/iteration-011-esp32-final.log`、`logs/test/iteration-011-native-retry.log`。不虚构行/分支覆盖率。

## 保留策略与资源

app先处理，再解析同loop reboot，因此已到期事务可能先启动；pending之后禁止新事务。仅在非TX时stop一次，失败仍记录后reset。100ms deadline用捕获的uint32时间且wrap-safe；reset不等待日志排空。host已复现这些既有行为，未改策略。setup只支持每实例一次，fake reset仅计数；对象重建不代表实际MCU/adapter复位。

RAM24,548/327,680（较010 +16），flash363,842/1,310,720（-12）；这些是compiler报告，不推断单一布局成因。无新显式生产堆分配；既有inline optional、固定帧/队列和320-byte局部数组不变。栈高水位、峰值heap及最坏时延NOT MEASURED。RadioLib既有Module构造分配ArduinoHal，构造存字段但SPI/GPIO初始化在begin/init；不可声称全固件无分配。Host输入4096、随机脚本16、低层trace有界；测试输出/string可分配，不是固件内存保证。

版本0.2.0-alpha.1/协议1/RadioLib7.8.1、数值NVS namespace/device key/schema/24-byte、wire/identity、pins/PHY、错误分类与日志字段不变。52个输入hash及保护证据见 iteration-011-source-manifest.sha256/source-review.json/validation.json。Native通过后仅两个不参与native的concrete header owner注释变更；最终ESP重编译覆盖最终树。无新增warning，只保留ERR-005 USB CDC两次，native无warning。

工具链：Core6.1.19、platform55.03.311（显示55.3.311）、Arduino3.3.11、IDF libs5.5.5+sha.b774170ff46、RISC-V14.2.0+20260121、RadioLib7.8.1；native1.2.1/Unity2.6.1/MinGW6.3.0，无升级。

## 证据界限

软件验证为实际协调器顺序、guard、状态/命令/事件分派、字节兼容和已测虚拟deadline。模拟包括低层返回/IRQ、raw RNG、clock、serial容量、实例重建/reset请求。真实NVS/Flash持久性、断电、MCU重启、USB锁/阻塞、SPI/BUSY、GPIO/IRQ竞态、RF/天线及熵质量均NOT VERIFIED。

实际库存2个RA-01SCH/LLCC68、1个ESP32-C3 Super Mini，第二MCU待采购、915MHz Moxon待制作，未组装双设备，无烧录或物理发射。ERR-008/010同步等待、ERR-009库分配、捕获时间陈旧、永久坏随机源下freshBootId无界循环仍未解决；跨真实启动previous=0不保证持久唯一性。未复现需改策略的runtime内存/状态缺陷，不能据此证明无race。

M0–M2实物门槛IN PROGRESS，M3–M8 NOT STARTED。本轮不增加TDMA/bitmap/选择性调度/ADR或外部集成。下一软件任务建议依赖时延/分配预算与boot-ID故障活性分析，具体缺陷复现后再作最小策略修正。

## Iteration012修正说明

上文无界generator/陈旧时间属于011。012复现后修正为8次上限、保持耗尽身份/1s恢复，以及poll/stop/drain后刷新边界时间（ADR-015、ERR-019/020）。旧266不变通过，新增24，共290；ESP32 PASS。stop失败仍reset、app先命令、未排空日志可reset均保留。依赖等待/熵/实物限制仍未解决，见DEPENDENCY_BUDGET_AUDIT.md；不改写历史证据。
