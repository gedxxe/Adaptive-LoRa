# Iteration 010 — 配置存储边界审查

完整英语审查为 docs/en/CONFIG_STORE_AUDIT.md。本摘要对应修改前审查和 ADR-013。

修改前 fetch 后，local/remote Iteration 009 HEAD 为 `3176e766d3876c86253e7694a7694560be72d1bf`，工作树干净；main 已通过外部合并变为 `2a7a68120552a8037d796cf61b0af11e91abd0f2`。本轮分支直接来自指定 Iteration 009 remote，不基于 main，target 原不存在；不合并、不创建 PR。

原 load：局部 Preferences 只读 begin，失败 nvs_missing_or_unreadable；长度非 24 则 end/nvs_missing_or_length；读后先 end，count 非 24 返回 nvs_read_failed，否则实际 decode。原 save：先 validate/encode，失败不访问 storage；可写 begin 失败 nvs_open_failed；putBytes 后 end，非 24 返回 nvs_write_failed 且不读回；之后实际 load 只读重开，原样传播其错误，再 encode/memcmp，有效不同则 nvs_verify_failed。失败 load 保留调用方输出；保存不改加载的 runtime config。

已检查 Arduino-ESP32 3.3.11 Preferences.h/.cpp 和 framework libs 5.5.5+sha.b774170ff46 的 ESP32-C3 nvs.h。begin 只在成功后设置 started；end 只关闭已打开 handle；析构也 end，但显式 end 后是空操作。getBytesLength 吞掉底层错误返回 0；getBytes 再查长度；putBytes 调用 nvs_set_blob/nvs_commit，任一失败返回 0。NVS API 要求显式 commit 和 close，close 不能替代 commit。预编译 SDK 未附 IDF 内部 cpp，本轮不推断其硬件行为。

namespace 数值初始化逐字节保留且不解码；key device，schema 1，blob 24 bytes，小端 magic 0x4344464D、原 offset/reserved/FNV-1a 不变。旧 13 个 config tests 未执行实际 store。新增五操作 ConfigStorage 引用和单一 PreferencesConfigStorage owner，store 保留全部策略；串行调用显式关闭后才重开 readback。两个 encode-false 防御分支在当前纯 codec/有效配置/固定 buffers 下不可达，不虚称覆盖。

准备 56 个新 store 用例：load/save 故障、错误优先级、caller 不变、begin/end/模式/顺序、精确 namespace/key/golden blob、readback 不同/损坏、更新及重复会话。fake 只存固定字节/响应计划/轨迹，不复制 store 或 codec。实际执行结果待命令完成后追加。

真实 Preferences/NVS/Flash commit、断电、重启/损坏恢复、wear/retention、RF 未验证；失败 save 不保证回滚，host 成功不证明实物持久性。固件 0.2.0-alpha.1/协议 1/RadioLib 7.8.1，既有 radio/app/PHY/console 和 163 regression 均保持。M0–M2 实物门槛仍 IN PROGRESS。

## 实际结果和保留性

- `pio run -e esp32c3_supermini`：退出 0、PASS，146.83 s；`logs/build/iteration-010-esp32.log`。
- `pio test -e native`：退出 0、219/219 PASS、0 失败/跳过，28.62 s；`logs/test/iteration-010-native.log`。新增 store 56/56，既有 163/163 的执行顺序/名称/结果与 iteration 009 完全一致（driver 71、app 38、codec 18、config 13、console 10、sequence/timing 7、logger 6）。
- 逐条结果 `logs/test/iteration-010-scenarios.md`；结构化结果 `logs/build/iteration-010-validation.json`。公开输出仅路径/EOL/显示尾空白规范化，私有原输出保留。无新增 warning，ERR-005 CDC 提示保留；首轮全部通过，未削弱断言。

56 场景分为 load 26、save/readback 24、consistency 6；lifecycle/compatibility 断言贯穿各组。覆盖每个短长度/读写计数 0..23、25/32/SIZE_MAX 超长查询、有界 buffer、无效 magic/hash/valid-hash schema/role/IDs/interval/reserved、失败输出保持、错误优先级；RO/W/RO 模式、begin/end 配对、失败 begin 不 end、失败写不 readback；精确 golden 24-byte blob 和数值 namespace/device key；不同有效 readback 为 nvs_verify_failed；保存失败不虚称回滚/成功，三轮更新共九次平衡会话。Fake 只有 32-byte 存储/读 payload、16 session plans、64 trace，无配置或 store 策略。

源码检查：39 个保护文件逐字节不变，包括 DeviceConfig、console、main.cpp 15 行、runtime.cpp、radio/app 源码和全部既有 tests、PHY/pins/版本。实际 store 仅改 Preferences 局部对象为引用转发，澄清一个 durability 注释；namespace 数值表达式、错误字符串/顺序、count 校验/end/codec/readback 全保留。Runtime 只改 adapter 先于 store 构造；native 只加实际 store.cpp。48 个编译输入 hash 与通过测试的快照相符。

RAM 24,532 bytes（与009相同），flash 363,854（+124）；不声称二进制/实测时序相同。无新增显式堆分配，库分配/commit 延迟未模拟；两个不可注入的防御性 encode-false 分支保留，未测量行/分支百分比或 sanitizer。

验证的是模拟响应下生产 store/config/codec 策略及 ESP32 编译。真实 Preferences/NVS/Flash commit、断电、重启/损坏恢复、磨损/保持时间，以及 Runtime/随机源/USB/SPI/IRQ/RF/天线仍未验证；本轮未执行实物测试。建议下一轮验证 runtime boot/provisioning/reboot dispatcher 的 fail-safe，M0–M2 整体仍 IN PROGRESS，不启动后续 MAC。
