# 架构决策补充

英文 `docs/en/DECISIONS.md` 为完整规范记录。ADR-001–008 已确定：一份 NVS 选角色固件、LLCC68/RadioLib 7.8.1、先固定 PHY HELLO/ACK、后续 Gateway TDMA/bitmap/保守 ADR、M0–M2 不做云端/电表集成，以及薄 main.cpp 架构。

## ADR-009 — M2 有界存储、解析、去重和诊断

- 接受日期：2026-11-12；实现候选，实物验证待完成。
- NVS schema 1：显式 24 字节小端单 blob（magic/schema/role/IDs/interval/reserved/FNV-1a）。完整性校验检测存储损坏，不是无线校验或认证；保存验证完整配置并读回，失败加载保持 UNSET；重启后才激活暂存配置。
- Console：128 字节行、最多 6 token、每 loop 64 字节。新增 `config set hello-interval-ms <1000..3600000>`，默认事务结束后等待 5000 ms。角色编辑保留 ID；Network 65535 合法，广播保留值适用于节点地址。
- HELLO Payload 为 0–68 不解释字节，当前 Client 发空；ACK 恰好 9 字节、仅 status=0。M2 拒绝广播、未来类型及非零 frame/config/schedule 字段，v1 Header 不变。
- 去重：8 个 session，每个 64 sequence 窗口；更旧序号视为 stale。旧 session 淘汰或 Gateway 重启会丢历史，不提供持久化/认证重放保护，不实现 DATA。
- 固定 SF7、sync 0x12、XTAL、LDO，保留默认内部 DIO2 RF-switch 控制；TXEN/RXEN 不接 MCU，须核实实际模块。原生 USB CDC 和全部引脚仍为暂定。
- 日志：16 行有界队列，非阻塞排空；status 报告丢弃计数。丢日志时重做验收。库未公开匹配芯片标识字符串，不能伪造检测结果；未知指标使用 N/A。

## ADR-010 — 用最小 transport 引用验证生产 M2 状态机

- 接受日期 2026-11-12。先只读审查和提出方案，再实现 6 个现有 radio 方法的 RadioTransport；RadioDriver 保持唯一 LLCC68/SPI/IRQ owner，Runtime 保持具体 driver 的 begin/poll/stop。无 factory、堆所有权或额外调度框架。
- Native 编译实际 app/codec/tracker/config/logger；只替换 transport 完成事件、ESP 计时及 Serial 字节适配。它不是 LLCC68/RF/ISR/NVS 仿真。Runtime/NVS 本次为静态审查，不声称被 app integration tests 执行。
- 不添加 M2 自动重传。测试重放完全相同字节；超时后的新事务使用新序号。去重缓存和 Gateway 重启丢历史的现有边界保持。
- 更正 ADR-009 非阻塞表述：有界队列不证明 SDK USB 调用非阻塞；NVS/BUSY/USB 延迟、RadioLib 临时堆分配仍是未验证的边界。
- 实际库存两个模块/一个 MCU/无制作天线，未组装双设备。全部实物门槛保留，后续 MAC 不实施。

## ADR-011 — Adaptive-LoRa 独立身份和发布边界

- 2026-11-12 接受；固件 0.2.0-alpha.1、协议 1 不变。
- 项目为 Adaptive-LoRa；研究标题为 Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks。工业能耗是通用动机，不是协议依赖或部署关系。
- 按明确授权脱敏历史公开副本的身份/路径，保留顺序、时间戳、版本、结果、错误及解决状态。原始私有副本在发布树外，旧 hash 仍标识原工件；公开 digest/脱敏清单，不把改名解释为重新验证。
- 仅机械改 C++ namespace/native 宏；旧 NVS namespace 字节用数值兼容常量保留，schema/key/blob、空中字段、pins/PHY/状态逻辑不变，不做存储迁移或协议升级。
- 保持现有 RadioTransport/LLCC68 owner，明确 platform/application API 缺口，不声称已有通用 payload/event、跨 MCU 服务或 SX1262 实现；电表/回传/数据库/UI 必须在协议外。
- 只在项目根初始化 main，保留远端已有提交，审查全部脱敏暂存文件后发布到明确授权仓库。远端当前为空，不需要 unrelated-history merge；不强推、不改可见性、不自动打 tag。
- 排除 SDK/二进制/本地配置/私有原件，保留开发日志；项目许可证待选择，保留依赖许可，不上传无再分发授权的论文全文。

## ADR-012 — 以单一硬件引用验证生产 RadioDriver

- 2026-11-12 接受，iteration 009；固件 0.2.0-alpha.1 / 协议 1 不变。先完成只读审查/方案，见英语 RADIO_DRIVER_AUDIT.md。
- RadioDriver 保留状态、长度校验、deadline 和事件/错误优先级；Llcc68Hardware 唯一拥有 Module/LLCC68/SPI/GPIO/ISR。RadioHardware 是非拥有引用，Runtime 先构造唯一 adapter 再构造 driver；无堆所有权、factory/task 或第二种 radio。
- RadioTransport 应用接口不变。Native 编译实际 driver.cpp，有界 fake 仅注入返回码、IRQ 锁存/电平/flags、payload 和显式 uint32 时间，不复制 driver 状态机。生产编译用 static_assert 核对固定 status/raw IRQ 值与 RadioLib 7.8.1 一致。
- 固定 PHY/RX 参数和只置 flag 的 ISR 原样移动。保留 HEADER_VALID 条件、timeout 优先级、再次 begin 失败后既有 initialized 标志，以及 read 成功但 finishReceive 失败时保留的 metrics；不改变恢复策略。
- Host 不执行真实 adapter/SPI/BUSY/ISR 并发或 RadioLib 内部；同步等待/分配、NVS/RF 实物门槛仍未验证，不宣称完整通用 PHY/平台服务。

## ADR-013 — 通过五个存储操作验证生产配置策略

- 2026-11-12 接受，iteration 010；先审查/方案后改源码，见 CONFIG_STORE_AUDIT.md。从已核实的 Iteration 009 remote HEAD 创建分支，不从后来合并的 main 开始。
- ConfigStorage 只有 begin/getBytesLength/getBytes/putBytes/end；NvsConfigStore 保存非拥有引用，单一 PreferencesConfigStorage 拥有 Preferences 并原样转发 namespace/key/返回值，无配置策略。Runtime 先构造 adapter 后 store；只支持现有串行非重入调用，每次成功 begin 显式 end 后才重开 readback。
- 实际 store 保留验证、编解码、错误字符串/顺序和读回比较；native 执行生产 store/DeviceConfig。有界 fake 只有存储字节、返回计划和调用轨迹，不复制 schema/codec/load/save 模型；无堆所有权、factory/文件系统/通用框架。
- 数值 namespace 初始化表达式保留在 NvsConfigStore 且逐字节不变，不解码历史标签。device key、schema 1、24-byte LE/FNV 格式、验证规则不变；固件 0.2.0-alpha.1、协议 1、RadioLib 7.8.1、radio/app/PHY/console/runtime dispatcher 受保护。
- 保存失败不承诺回滚或持久性；readback 失败原样传播 load 错误，不一律改为 verify。有效但不同配置仍为 nvs_verify_failed。失败 load 不改调用方配置，运行配置仍由原重启流程生效。
- Native 仅验证模拟响应下的 store 策略，不验证真实 Preferences/NVS/Flash commit、断电、重启/损坏恢复、磨损/保持时间。防御性 encode-false 分支保留但不虚称注入覆盖；依赖等待/分配和硬件门槛保留。

## ADR-014 - 用 native SDK 适配执行唯一生产协调器

- 2026-11-12 接受，iteration011；先审查/方案后修改，见 RUNTIME_AUDIT.md。固件0.2.0-alpha.1/协议1不变。
- FirmwareRuntime 接受现有 ConfigStorage/RadioHardware 引用，仍拥有实际 store/driver/console/logger/可选 role apps；无新 lifecycle/platform 接口、factory/task 或第二协调器。Esp32Firmware 先构造具体 Preferences/LLCC68 owner，再构造唯一 runtime，生命周期覆盖引用；main 15行仅委托。
- runtime.cpp/freshBootId 策略原样保留，仅换 C++ cinttypes 兼容旧 host；扩展 native Arduino/esp_timer 适配，提供有界串口输入、micros、reset/reason/raw random。同一生产 app/store/driver/codec/logger 执行；这些 header 不是硬件模型或通用生产 MCU 端口。
- 固件使用标准 optional；已安装 GCC6.3 缺少该 header 时由项目 namespace alias 选择 experimental optional，仍原地 emplace/bool/dereference，无 namespace std 改写或堆所有权。复现 ERR-017 后在 native 全局启用 UNITY_SUPPORT_64，保留完整64位断言，不截断/删除。
- 保留既有策略：app update 先于串口解析，未 init 的 poll/stop 不访问硬件，save 仅暂存直到新启动，stop失败仍可 reset，100ms捕获时间 deadline 不等待日志排空，以及非零/previous-ID 拒绝循环；随机源失败活性/跨启动碰撞未解决。
- 低层响应+实际协调器仅证明软件顺序；对象重建/虚拟reset/raw random/串口容量不证明真实 NVS/重启/USB/熵/SPI/IRQ/RF，不增加后续 MAC 或外部集成。

## ADR-015 — 有界session更新与runtime边界时间刷新

ACCEPTED2026-11-12，Iteration012。修改前审查/有限RED复现见DEPENDENCY_BUDGET_AUDIT.md与ERR-019/020。生产generator每次最多8次raw调用，失败返回0；保留uint32(previous)签名，明确0为失败，不增加平台接口层。SequenceCounter.renew拒绝0/previous，失败保持旧boot与已耗尽计数。两个app记录SESSION_ERROR/boot_id_unavailable、禁止普通TX，每1s按尝试开始时间重试；恢复记录identity_recovered。pending reboot禁恢复。无可预测fallback、逐包NVS写、schema/boot-counter变更或自动熵源启用。有限调用数假设SDK调用返回，不等于实测WCET/唯一性/认证。

实际延迟注入复现过期ACK被接受、ACK窗口起点偏早、reset等待缩短及检查陈旧。poll后刷新app时间，stop完成后起100ms计时，drain后用当前时间判reset；保留app先串口、stop失败仍reset、不等日志排空及driver/app调用内观察/恢复语义，依赖仍不可抢占。旧266源码/结果不变且通过，新增24通过，ESP32 PASS。静态RAM+16、flash+466，无新增显式分配/task/framework，main不变15行。

wire/NVS/版本/PHY/单镜像兼容。IDF5.5.5不支持无条件熵保证，32位ID跨启动可碰撞；持久counter需另行版本化断电/磨损设计。STATIC_ONLY保持0，避免未经验证的栈/容量变更，HAL/SDK分配仍存在。上游BUSY-ramp及SDK锁/flash/USB时延未解决，未建立硬实时或实物验收。

## ADR-016 - 备份门槛与采集证据契约

ACCEPTED2026-11-13，Iteration013。firmware/native及53输入与012逐字节一致，无架构/协议/PHY/watchdog/库改动。流程/软件准备不等于实物验收；硬件/布线/授权、ERR-021及controlled fixture保持阻挡。无radio的合法角色仍先init LLCC68；单MCU持久性用Gateway暂存、radio物理缺席、ROM私有同板备份/恢复。无备份不擦/覆盖配置，secure/encrypted/不兼容layout另审查。ERR-023 SDK可在Runtime前格式化NVS，仅记录并设backup gate，不改SDK。

Capture schema2保留pair/status-only，增加单板、DTR/RTS开口意图（不保证无reset）、time/offset journal、hash/碎行、有界metadata、初末状态/中断错误结果。13 fake-port测试针对复现的false-success和证据缺口（ERR-022），不是新增固件覆盖。末status/tail尽力，缺snapshot/歧义就阻挡，file hash不能消除host读取前丢失，私有备份/身份须审核后方能发布。

CSV使用关闭HELLO cohort，区分A/T/R/ACKstart/done/C；A含启动失败，PDR以R交完成T，分母0/关联未知保留N/A。host观察时间/firmware transaction_us不是实测RF。正常>=200完整HELLO、干预和注入另跑，不虚构/覆盖历史实物记录。

## ADR-017 - Iteration013 合并后的 canonical main 基线

2026-11-13 按用户明确要求接受。通过 PR #3 将已核实 e431e3f8ffd6e2d95dc126bb68e8231723a13c4d 及011/012历史用普通 merge commit 合入 main。初始 main5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861 独有两个合并记录，但其 tree 与共同010祖先相同；无冲突候选 tree 等于013。保留双亲历史、全部历史分支和既有记录；本次仅新增整合文档与实际验证日志，不改生产/测试/工具输入。

成功合并后 main 为规范开发基线，后续分支从最新 main 开始。合并前 ESP32、290 native 和13 fake-serial capture 全部重跑通过，最终核对 merge tree/输入。固件0.2.0-alpha.1、协议1、NVS schema1/namespace/key/24-byte blob、RadioLib7.8.1、固定PHY及暂定pins相对013不变。保留既有011/012修正，不新增功能；不声称实物验证，不 squash/rebase/强推/删除历史分支。

## ADR-018 - 单一Heltec V3.2 HF生产目标

2026-11-17客户端日期接受，Iteration014。用户在原audit-only附件后明确授权立即实施、保留工程历史，并要求push/普通merge。只保留Heltec WiFi LoRa 32 V3.2 HF活动目标，替换adapter/environment，不保留双生产目标；历史记录/分支不删改。

安装平台55.03.311已有官方板定义，无需升级。S3FN8/SX1262、Radio GPIO8–14、DIO3 TCXO1.8 V、DC-DC；RadioLib7.8.1 begin已设DIO2切换。保留Driver header/CRC重设、全部状态/错误策略。CDC_ON_BOOT=0选择CP2102/UART0，board自带USB_MODE不等于启用CDC。OLED/Vext/电池功能不做；36低有效、37高使能仅保留资源。

固件0.2.0-alpha.1、协议1、NVS namespace/key/schema/blob、app/codec/session/driver策略和15行main不变，driver标签改SX1262。保留915 MHz/10 dBm/固定PHY，Moxon为设计基线，未捏造匹配/gain/RF。ERR-021继续开放，不改第三方库/watchdog，不做TDMA/ADR/应用集成。

实际adapter通过有界测试专用SPI/RadioLib调用记录器编译，原290保留、新增12；只有旧driver标签断言更新。首次fixture重复声明失败保留，修正后302和capture另13全部通过。测试不证明真实chip/IRQ/TCXO/DC-DC/RF。单板radio也集成，操作前接RF终端，先同板ROM私有备份，不跨MCU恢复完整镜像。


## ADR-019 — 当前合同与仅主机调度研究

2026-11-18，Iteration 015 接受。AGENTS 精简并链接规范 PROGRESS、模块归属、验收与
历史记录；CODEX_START_PROMPT 明确归档，当前进度与历史快照分开。小型 checker 从
生产/配置读取版本、板环境，检查选定当前文档链接/状态并排除历史，不永久固定测试数/SHA。

新增 Python 标准库 airtime/预留预算工具和一份双语 TDMA_FEASIBILITY。未知时间保持
显式，ACK 包含完整头/metadata，retry 每轮包含 ACK；结果仅分析假设。不激活预留
消息、scheduler、bitmap/retry/ADR，不改版本/生产策略。表征八 session 缓存，不扩容；
按 admission/schedule 维护状态留给 M4。ERR-021 无已证实缓解而继续开放，历史记录保留。
仅发布指定分支/PR 供审查，不自动合并规范 main。


## ADR-020 — 可选板载 OLED 诊断

2026-11-19。保留原 V3 环境作为关闭 OLED 的参考，候选环境沿用 board、版本、RadioLib、
PHY 和 NVS 布局。使用框架自带 Wire 和项目内 SSD1306 命令/字形，无需引入 Heltec 库或
全像素 framebuffer。控制器/地址和 variant alias 有 V3 参考，V3.4 电路及 RF SKU 待测。

Esp32Firmware 持有显示。Runtime 启动 Vext/复位，在 radio、应用和串口服务后传入只读
快照，每次处理一段变化页，TX、Client 等 ACK 及重启期间暂停。I²C 失败停止更新并记录
一次。Wire 使用 SDK 锁并分配缓冲区，请求时限与实际延时分别记录。遥测分别统计 ACK
开始/完成，接收指标保留有效位，计数按 2^32 回绕、重启归零，单调时钟判断新鲜度。
保留协议字节和 schema 1，审批前不更改研究频点。M2 仍需实物证据。

## ADR-021 — 独立 v2 研究镜像并恢复固定 PHY

2026-11-20。默认和 OLED 模式继续使用 v1，单独的 PlatformIO 研究镜像使用 v2。
两种镜像继续使用原 NVS 角色和 schema 1，分别校验各自的线格式，均使用固定 SF7 与
同一无线驱动。v2 以实际处理 Beacon 完成事件的时间为锚，按授权载荷计算一个 Client
的时隙。收到零位或等不到有效反馈时，可在下个分配时隙重发同一 DATA 字节。正反馈须
核对 Client boot ID 和序号。按用户确认的预算，初次发送加最多三次重传。

恢复用户确认的 10000 ms 上报默认值和 M2 ACK 超时，已有 NVS 周期继续生效。
v2 的反馈窗口在该时限内。恢复请求 22 dBm，RadioLib `begin()` 保留 60 mA 后明确设置
140 mA 高功率 OCP。PA 限流不能用于推断板卡输入电流。V3.4 继续以 V3.2 为电气参考。
实际功率和试验保护时间需要实测，ADR 仍待实施。

## ADR-022 — 按实际长度授权及共享重传

2026-11-20。保留 v1、v2 线格式，新增独立 v3 环境。Gateway 的 Beacon、授权及反馈
允许组播，请求和 DATA 由 Client 单播。名单最多八个 Client，按实际请求长度授权，
名单顺序决定时隙和位图索引。应用提交复制到有界缓冲，Gateway 回调同意后才设正反馈。
每轮最多四个共享重传时隙，供欠缺位置使用。丢反馈时等待新授权，累计位图可恢复确认。

默认上报 3000 ms、M2 ACK 超时 1000 ms，已有 NVS 值继续生效。v3 以 1000 ms 为目标，
按名单最坏载荷计算周期，上限 10000 ms，50 ms 保护值仍需实测。v2 保留独立的 10 秒反馈
预算。两种研究镜像均固定 SF7，PHY 协调切换须另有版本、生效边界及回退协议。

## ADR-023 — v3 无线活动结束后刷新 OLED

2026-11-20。保留 v3 带屏和无屏镜像。OLED 使用经过协议校验的只读遥测。运行时先
处理无线和串口，只在帧尾空闲、无待处理 IRQ 时刷新。Client 自身完成后仍保护其他节点
的 DATA、反馈和重传窗口。没有可用 Beacon 锚点时继续监听。配置模式可独立显示。

每步预留 10 ms，另在下一 Beacon 前保留半个无线保护间隔。若实际步骤超出 10 ms，停止
显示服务直至重启。保持原 v3 时序和帧格式。Wire 超时及主机测试不能确定实板上已开始调用的时延上限。

## ADR-024 — v3 保留一个固件构建目标

2026-11-20。按用户要求移除独立的无屏 v3 ESP32 环境。带屏环境直接继承 Heltec 基础板卡，
保留 v3 与 OLED 构建选项。两套 v3 主机测试继续分别验证带屏和无屏运行路径。以往构建
记录保留当时存在的目标与结果。

## ADR-025 — Client 先显示开机状态再监听 Beacon

2026-11-20。无线初始化成功后，v3 OLED Client 在无线待机期间最多用 400 ms 绘制
第一幅 WAIT 画面。主循环仍处理串口。屏幕就绪、报错或超时后立即开始监听。
启动后继续使用原有帧尾刷新规则，Gateway 的 Beacon 调度保持原样。

## ADR-026 — v3 OLED 完成整页后再开始计时

2026-11-20。整页画完后至少停留五秒。先画新页正文，再更新标题，分段刷新从上一次
停下的位置继续。自动计算 OLED Gateway 周期时，若未超上限，在无线预留后增加
150 ms。默认名单周期由 6516 ms 变为 6666 ms。相对无线时隙、保护时间、帧格式和
持久配置保持原样。每步仍检查待处理 IRQ、I²C 预算与下一次 Beacon 的时间。

## ADR-027 — Gateway 确认位图发送完成后才开放重传

2026-11-20。在上一轮反馈收到 TX_DONE 前，下一轮接收位图保持清零。完成事件须与帧
及重传轮次匹配，并在首个重传时隙开始前或起点处理。后续位图仍可累计确认，沿用现有
无线事件和 v3 线格式。

## ADR-028 — v3 搜索 Beacon 时保持诊断显示

2026-11-20。保护活动时隙和下次预计 Beacon 接收完成后 25 ms。过期时隙不可授权
Client 发射。搜索时保持接收，每个 OLED 步骤之间检查待处理 IRQ。收到新的有效 Beacon
后恢复时隙保护。Gateway 在 Beacon 或授权发射失败后保留可能发生对端活动的时段，
再使用剩余帧尾更新显示。旧会话标识继续用于重放检查。

自动计算周期且不超过 10 秒上限时，保留 250 ms 显示余量，默认周期为 6766 ms。
显示故障后等待至少两秒，在允许的窗口内复位重绘。Wire 已初始化时只复位屏幕，
保持 Vext 供电且不重建总线。v3 线格式仍最多支持八个 Client，70 节点需要调整容量与控制帧格式。

## ADR-030 — 区分无线完成时间与发射启动时间

2026-11-21。DIO1 首个边沿由 ESP 计时器记录，主循环在短临界区内取出时间戳。
只有引脚电平可用时采用轮询入口时间。v3 以完成时刻校验接收并同步 Beacon 和授权，
发射仍按当前时间判断，避免过期时隙发送。开发名单为 Gateway 1 和 Client 2–9。

## ADR-031 — 默认多 Client 镜像与离线 ADR

2026-11-21。默认 v3 镜像包含板载屏幕，同时保留 v1 和 v2 对照构建。最后一轮位图成功
后记录各授权节点的上行结果。离线 SF 建议使用实验给出的阈值和滞回参数。空口保持 SF7，
等待控制协商、恢复方案和切换耗时实测。
