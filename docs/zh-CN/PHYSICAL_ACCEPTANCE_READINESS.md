# M0-M2实物验收准备 - Iteration013

基线012 `9341e7c0016c66fd9316d489ccfca00a646ae2cb` 已fetch核实，干净工作区直接建013，不基于main。固件0.2.0-alpha.1/协议1/RadioLib7.8.1、所有firmware/native输入不变；本轮native290/290、ESP32 PASS，RAM24,564/flash364,308；capture13个fake-port检查通过，不能当实物数据。

库存2个RA-01SCH/LLCC68、1个ESP32-C3 Super Mini；第二MCU待购、915MHz Moxon待制作，无组装pair。013只读枚举无port，未开口/烧录/擦除/复位/RF。所有物理门槛NOT RUN/IN PROGRESS。

## 操作入口与阻挡

- [HARDWARE](HARDWARE.md)区分官方文档pin功能、项目GPIO提案、实际未验证continuity；当前官方V1.1.0 PDF表/图已核对，旧链接迁移，URL/hash见iteration-013-readiness-sources.json。不能由SoC规格推断克隆Super Mini修订/USB/LED/排针。full PDF只留忽略的本地cache，不发布。
- [BENCH_RUNBOOK](BENCH_RUNBOOK.md)单MCU分支覆盖身份/USB、ROM中两份私有全flash备份/hash、实际partition核对/不覆盖NVS的上传、missing/invalid暂存、Gateway save/readback/reboot/power-cycle/恢复。全过程radio物理断开/断电，不选CLIENT。有效角色仍先init radio，卡住记BLOCKED并ROM恢复，不假称NVS PASS；SDK在Runtime前可能格式化坏/旧版NVS，备份须先于首次app boot。secure/encrypted布局另行审查，不force/改eFuse。
- 双设备门槛：M0实际照片修订/断电continuity/3.3V瞬态/USB/feed与天线/负载/合法RF；M1实际LLCC68状态+标记/SPI/BUSY/DIO1/reset/稳定日志/no unintendedTX/故障恢复；M2 >=200完整HELLO cohort，RX/ACKstart/done/accepted/timeout/error/RSSI/SNR/latency分开。duplicate/错ACK/损坏NVS实物fixture未提供/执行，不能用native替代，保持门槛开放。
- ERR-021 OPEN，详见[依赖审查](DEPENDENCY_BUDGET_AUDIT.md)：setTx成功后另有BUSY无限yield，当前loop未订阅TWDT，默认配置不保证检测/恢复。局部上游有界timeout、adapter方案、progress watchdog需各自审查/测试；013不补丁/改watchdog/加推测恢复。无人值守/卡BUSY验收held，有人正常观察不关闭此风险。
- 仪器待备：数据USB、板控件、断电DMM、限流稳压3.3V/去耦、适合1MHz SPI/BUSY/DIO1的scope/logic analyzer与sample rate/calibration记录，合适功率50欧feed/天线/负载、匹配证据及随手安全断电。实际可用性TBD；无radio开路TX，未证明915MHz合法。

## 采集与证据

PlatformIO Python运行capture，先备份/RF gates，再开新阶段目录，按[RUN_RECORD_TEMPLATE](../../logs/test/RUN_RECORD_TEMPLATE.md)记录实际固件commit/binSHA/board/IDs/PHY/仪器/授权/备份私有引用。--board单口，或--gateway和--client不同口；默认仅open/end各status并尾读1s，--no-status纯被动。DTR/RTS=false仍可能开关口reset，不共享monitor。脚本hash不是固件hash，private备份/USB身份不发布。

原始.log+chunk offset/UTC/monotonic journal+capture schema2保留。退出0/COMPLETE只表示采集完成，不是验收；Ctrl-C130/INTERRUPTED，IO1，CLI2，初始RUNNING表示可能未干净结束。检查hash/长度/offset/time/碎行/空流/reset和同连接初末drop0；unjoined writer数据未finalize。尾status可能来不及，缺失就阻挡证据，不能猜0。秒数不证明200包。

[LOGGING_GUIDE](LOGGING_GUIDE.md)明确events/transactions CSV、HELLO K与Gateway ACK header区别及cohort分母：PDR=|R交T|/T，事务ACK成功=C/A（A含启动失败），边界/歧义排除并记录；host read时间不是RF/IRQ/同步时钟。CSV只是准备规范，不是自动评估或实测输出。完整未来命令在英文/中文runbook，不在013执行。

仍需第二MCU、天线/负载/Moxon计划、实际布线/修订/供电/USB、仪器/当地授权、ERR-021处置及replay/corrupt-NVS fixture。下一步识别连接板并按备份MCU流程执行；M0-M2保持实物IN PROGRESS、M3-M8 NOT STARTED。无物理距离/LOS/供电/PDR/RSSI/SNR/latency测量。
