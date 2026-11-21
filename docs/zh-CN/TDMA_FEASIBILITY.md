# TDMA 可行性与 M3 设计说明

当前 v3 的名额、载荷上限、超帧预算及 10、20、70 节点的外推见
[定时链路](SCHEDULED_LINK.md)。

本文保留 Iteration 015 的主机模型与候选调度表。Iteration 019 固件使用单 Client、
四轮、固定 SF7，其 v2 空口规范见 [定时链路](SCHEDULED_LINK.md)。下文网格仅供比较，
不能用于推断实测容量或 v2 时序。英文记录见 [TDMA_FEASIBILITY.md](../en/TDMA_FEASIBILITY.md)。

## 模型与计算假设

[tdma_feasibility.py](../../tools/tdma_feasibility.py) 仅使用 Python 标准库，无硬件访问。
输入包括各 Client 的应用 payload 字节列表、数据/控制 PHY、beacon airtime、ACK
策略、0–2 次重试机会、切换/处理/guard 时间及超帧长度。API 支持不同 payload，
CLI 为所有 Client 重复同一长度。模型允许 SF7–SF12、BW125/250/500、CR4/5–4/8，
不包含 SX126x SF5/6 特殊系数、长交织或其他调制。研究网格仅用 SF7/8/9 BW125，
不改变固件 SF7。

```text
Tsym_us = 1,000,000 * 2^SF / BW_Hz
DE = Tsym_us >= 16,000（或明确覆盖）
IH = 显式头 0，隐式头 1
Npayload = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*IH)
                       / (4*(SF - 2*DE))), 0) * CR_denominator
Tpacket_us = ceil((preamble + 4.25 + Npayload) * Tsym_us)
```

PL 包括 28 字节协议头，CRC 为 PHY 布尔选项，不再向应用长度加两字节。
Fraction/整数运算只对编码块和最终整微秒时长向上取整。依据
[RadioLib 7.8.1 airtime](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x.cpp)
与 [LDRO](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x_commands.cpp)
实现；前者注明 SX1268 datasheet 6.1.4。测试包含手算编码块向量，不代表无线实测。

68 字节应用数据加头共 96 字节：SF7/8/9 分别为 164,096 / 297,472 /
533,504 us。个别 ACK 沿用 28+9=37 字节。计划 bitmap 为
28+2+ceil(N/8)，1/10/70 个 Client 分别 31/32/39 字节，含 active_count、
bitmap_length，frame_id/schedule_version 在原 envelope。网格的 ACK/control 均用
SF9，这是未来假设；当前 M2 的 ACK 实际仍用 SF7。

每轮保留所有 Client 上行和 ACK；每次重试机会再保留一整轮，包含 ACK，
不是概率平均重试率。每超帧只计一个 beacon。Gateway 操作数为
1 + rounds*(N + ACKs_per_round)，分别对应 beacon TX、上行 RX window 和 ACK TX。
每操作计一次准备/切换与处理成本，每上行窗口计一次 guard。需要实测状态转换轨迹
校准，未模拟 Client 重叠准备、干扰、法规 airtime、同步获取、入网或 schedule 传输。

未知输入保留 JSON null。已知占用下界大于超帧时为 INFEASIBLE；其余只要有未知
成本便为 UNDETERMINED。成本明确时的 CONDITIONAL_FIT 仅指算术可容纳，不保证
物理可靠性。total_occupancy_us 和 remaining_us 在任一成本未知时均为 null，
不把缺测当实测零。负数、非整数和越界输入被拒绝。

```sh
python -m unittest discover -s tools -p test_tdma_feasibility.py -v
python tools/tdma_feasibility.py --scenarios --out logs/test/iteration-015-tdma-scenarios.json
```

216 场景覆盖 1/5/10/20/35/70 个 Client、SF7/8/9、应用 16/68 字节、重试 0/1/2、
individual/bitmap。统一假设十秒超帧、SF9 control、45 字节缓存 schedule beacon
（308,224 us）；切换/处理/guard 均未知。该 beacon 不带 entry，假设完整版本化
schedule 已安装，排除了 schedule 分发和 rejoin，不能证明 70 节点容量。
70 个最大帧单次上行本身即需约 11.487/20.823/37.345 秒，尚未计 ACK/beacon/guard。
低于十秒的情况仍不能断言可行。比较 bitmap 时必须计完整帧与实际 control PHY。

## 候选 M3 合同，尚未冻结或实现

候选保留 v1 28 字节 envelope。M2 仍拒绝 BEACON 0x10、非零 frame/schedule 和
广播地址。将来必须先审查 type-specific 验证、混合版本行为和版本策略；预留 type
不等于旧 Client 支持它。保留 HELLO/ACK golden bytes 与 schema 1，若语义不兼容，
按 [VERSIONING.md](../en/VERSIONING.md) 升级。本迭代不改版本。

候选 little-endian payload：format_version u8、active_client_count u8、entry_count
u8、superframe_duration_us u32、lease_frames u16、uplink_start_after_beacon_us
u32、slot_duration_us u32，共 17 字节。每个 entry 为 node_id u16、profile_index
u8、reserved-zero u8、slot_index u16，共 6 字节。frame_id、gateway session 和
schedule_version 用 envelope。一个 entry 的整帧为 51 字节；最多八 entry 是 93
字节，十或七十 entry 放不下。将来缓存/分片与原子生效需单独设计，不得用部分
schedule 授权 TX。模型的 45 字节 beacon 假设 schedule 已完整缓存；M3 实际先传一项，
严格检查 count、slot 唯一性、source、profile、duration、非零 lease 和 Client 身份。

Gateway 发布 frame ID、已提交 schedule version 和相对时序。Client 只接收预期
Gateway/network/session、完整已知映射和递增 frame；Gateway 重启使旧同步/lease
失效。schedule 在约定的未来 frame 原子切换；旧/未知/不完整版本禁止正常 TX。
frame/version 回绕需有界 lease 与序号比较，不能直接使用普通 unsigned 大小关系。

Gateway TX completion 与 Client RX completion 为各自本地时间锚点。先测 DIO1/
ISR 到服务延迟、准备/切换、airtime 锚点误差、处理及最长 beacon 间隔内相对漂移，
再确定 RX 提前量、slot 和 guard。保守 M3 候选在漏收 beacon 的当帧停止 DATA。
lease 只约束缓存映射有效性，不授权失同步 TX；重复漏收/lease 过期进入
UNSYNCHRONIZED，之后由 M6 rejoin 处理，不临时发送无调度 DATA/rejoin。

绝对 frame 锚点与长超帧用扩展 64 位单调微秒，相对偏移做范围检查。uint32 微秒约
4,294.967296 秒回绕，有符号模差比较仅适用半范围 2,147.483648 秒内。
M2 当前期限满足该条件；未来长 lease 不得截断成 uint32。Python 无限整数不证明
MCU clock 扩展或 ISR 原子性。保留单事件循环射频 owner，不新增 task/timer 框架。

## M4/M8 前必须处理的去重边界

实际 SequenceTracker 的 Native 测试覆盖 1/5/8/9/10/70 session、淘汰前后重放、
boot churn、多 Client rollover 和对象重建。八个保留 session 各有 64 sequence
窗口；九项及以上按插入顺序重放会持续淘汰下一项，全部被判断为 NEW。旧 boot
竞争相同八个名额，Gateway 重启会丢失全部易失历史。这是 M2 HELLO 缓存的已知
限制，不是新修复的缺陷，也不是 DATA exactly-once 保证。M2 无自动重传，重放
fixture 注入相同捕获字节，timeout 后下一新事务使用新 identity。

建议 M4 按活动 schedule entry 限定 admission/session/window，未知 session 在
HELLO/rejoin 前拒绝，并明确旧 session 保留、受控淘汰、schedule 替换与 Gateway
重启 epoch/rejoin 规则。不要简单把八项数组扩至七十。自然对齐 session 粗估
24 字节，70 项约 1,680 字节；若每项另留八字节 slot/profile/admission metadata，
约 2,240 字节，不含指标窗口、队列和 retired session。这是设计估算，非新分配。
当前 ELF sizeof 另见 Iteration 015 resource probe。线性扫描 O(N)，验证映射后可
按 schedule index O(1) 查找；内存能放下或逻辑测试通过不证明无线容量。

## 后续 M4–M6

Bitmap 必须绑定 gateway/network/session、frame ID、schedule version、精确
count/length 和尾字节未用高位零；按 schedule 顺序 LSB-first 映射。
有效 bit 0 为 MISSING，缺失/无效 bitmap 为 ACK_UNKNOWN，二者只能在预定重试位
发送。重试保留 source/boot/sequence 和原 payload，ACK 丢失不得导致重复应用交付，
先限制最多两次并保留耗尽诊断。

ADR 由 Gateway 控制，SF7–SF9/BW125、固定 TX power、公共 SF9 control。
滚动 RSSI/SNR、PDR/PER、retry history、最小观察窗口和 hysteresis 支持决策，
证据模糊时保持配置，阈值/窗口仍 experimental/TBD。PROPOSE/ACCEPT/COMMIT 绑定
config_version/effective_frame_id，仅边界切换；丢 commit 的 Client 保留旧 profile。
Gateway 必须保留公共控制恢复路径，单方 commit 不代表对端已收到。激活前需丢包、
旧版本和重启测试。

下一门槛是完成 M0–M2 实物验收并测量时序/内存，再审查单 Client M3 codec/scheduler。
见 [TEST_PLAN.md](TEST_PLAN.md) 和 [MILESTONE_TASKS.md](../en/MILESTONE_TASKS.md)。

本次网格结果：103 INFEASIBLE，113 UNDETERMINED，不是实物容量结论。
