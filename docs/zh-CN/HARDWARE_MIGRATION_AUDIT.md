# Heltec V3.2 硬件迁移审查

Iteration 014，客户端日期2026-11-17。基线为 `075b8faf64ca8377cc2adc396429aa6943b47a27`（PR #3 已合并）。原附件要求只做审查，用户随后明确选择“Implement migration now; preserve engineering history”，因此本次已经实施迁移。用户直接要求 push 和合并，也覆盖附件中的不建PR条款。

## 当前实现与证据边界

唯一活动固件目标是 **Heltec WiFi LoRa 32 V3.2 HF**，ESP32-S3FN8、SX1262、8 MB Flash、512 KB SRAM，不假定有PSRAM。采购选型已确定，但实际到货数量、PCB标识、芯片stepping和射频性能尚未检查。使用 `heltec_wifi_lora_32_V3` 环境，一份镜像由NVS加载Gateway/Client角色。

协议1、NVS schema1、固件0.2.0-alpha.1、应用状态机及15行main.cpp保持。活动硬件适配器已经替换，不保留第二个生产目标。过去的BUILD_MEMORY、ERROR_LOG、ADR、日志和迭代分支保留真实硬件记录；它们不能被批量改写为本次测试结果。未实现TDMA、bitmap、选择性重传、ADR、OLED、电池检测或应用集成。

完整逐文件/逐行引用分类见 [依赖清单](../../logs/build/iteration-014-dependency-inventory.json)：A生产、B当前文档、C测试/工具、D历史证据、E无用材料。数字GPIO定义和SPI/串口入口也单独检查。最终差异与输入指纹另有review/validation记录。

## 官方资料与版本

完整URL、文件指纹及源码定位见 [英文主报告](../en/HARDWARE_MIGRATION_AUDIT.md) 和 [来源清单](../../logs/build/iteration-014-hardware-sources.json)。检查了Heltec产品页、V3/V3.1/V3.2原理图、V3.2数据手册、硬件更新记录、HeltecWiKi、厂商ESP32库、PlatformIO板定义、ESP32-S3资料以及RadioLib7.8.1源码。三份原理图都已渲染查看；V3图纸是扫描图。原始PDF/图片只放在忽略的研究缓存，不随仓库发布。

Heltec_ESP32固定提交为 `6ab5cf916e01b0930eaa2245323c728f849705c4`；WiKi为 `07d6ea4725d07fad98a9b556f91d34a653bd2cb4`。数据手册文件名与内页Rev/date标识不完全一致，因此以文件SHA和明确V3.2图纸为依据。当前stable IDF已是较新版本，构建判断依据仍为安装的Arduino3.3.11/IDF5.5.5。

| 项目 | V3 | V3.1 | V3.2（当前目标） |
|---|---|---|---|
| MCU/Radio | S3FN8/SX1262 | 同系列 | S3FN8/SX1262；实物stepping待确认 |
| 存储 | 8 MB Flash、512 KB SRAM；不假定PSRAM | 同系列 | 同系列 |
| USB | CP2102桥接 | 更新C-to-C支持 | CP2102/UART0默认；原生USB可选焊接路由 |
| 主供电 | 早期理想二极管路径 | 移除理想二极管部分 | 继续修改布局/供电 |
| Vext | 开关管方案 | 开关管方案 | U5 LDO，GPIO36低有效 |
| 充电 | TP4054方案 | TP4054方案 | LGS4056HDA |
| 电池检测 | 早期分压控制 | 早期分压控制 | Q3/Q1改动，GPIO37高电平/上拉使能 |
| Radio时钟/电源 | DIO3 TCXO及DC-DC网络 | 同类电路 | DIO3 TCXO1.8 V，DC-DC网络已在图上确认 |

V4的存储、电源和USB方案不同，Wireless Stick Lite、Tracker等产品引脚也不能替代本板。

## GPIO与外设

依据V3.2图纸、pinmap和已安装Arduino variant。下列“候选”仅表示板上header且当前固件未使用；外接负载和实际板仍须检查，不能因为SoC支持某功能就认定可用。

| GPIO | 板功能/当前分配 | 可用性 |
|---|---|---|
| 0 | PRG/启动strap | 保留 |
| 1 | 电池ADC_IN | 保留 |
| 2 | Header，当前未用 | 候选 |
| 3 | Header/strap | 避免改变启动电平 |
| 4、5、6、7 | Header，当前未用 | 各为候选 |
| 8 | Radio NSS | 固定CS |
| 9 | Radio SCK | 固定SPI |
| 10 | Radio MOSI | 固定SPI |
| 11 | Radio MISO | 固定SPI |
| 12 | Radio RESET | 保留 |
| 13 | Radio BUSY | 保留 |
| 14 | Radio DIO1 | IRQ |
| 15、16 | 32 kHz晶振网络 | 保留 |
| 17、18、21 | OLED SDA/SCL/RESET | 保留；本次不初始化 |
| 19、20 | 原生USB D-/D+；R3/R29默认NC | 需单独路由决策 |
| 22–25 | 该MCU不存在这些GPIO编号 | 不可用 |
| 26 | SPICS1/header | 与Flash总线一起保留，先核对封装使用 |
| 27–32 | SPI0/1/封装Flash | 保留 |
| 33、34 | Header/可选octal存储功能 | 确认FN8无附加存储后为候选 |
| 35 | LED | 保留 |
| 36 | Vext控制 | 保留，低=开 |
| 37 | ADC_Ctrl | 保留，高/上拉=测量使能 |
| 38 | Header | 候选 |
| 39、40 | Header/JTAG | 不用外部JTAG时可评估 |
| 41、42 | Header/JTAG、variant默认I2C | 不用JTAG/I2C时可评估；不同于OLED17/18 |
| 43、44 | CP2102 UART0 TX/RX | 串口专用 |
| 45 | Header/电压strap | 保留检查 |
| 46 | Header/启动strap，输入专用 | 保留 |
| 47、48 | Header，当前未用 | 各为候选 |

DIO2/DIO3是片内射频控制对应的板上网络，不新增MCU脚。仪器只应高阻探测BUSY/DIO1/SPI/RESET。未来RS485可另行审查4–7/38上的UART和DE，但本次没有实现。

GPIO36通过Q5控制U5使能，厂商例程也以低电平开Vext。Radio和RF-switch bias使用VDD_3V3，OLED用Vext，因此无线固件无需打开显示电源。以后增加OLED时先把输出锁存设为off，再设输出模式、拉低使能，按实测/器件要求等电源稳定，再释放OLED复位和初始化I2C；例程100 ms不能当作radio切换实测值。

GPIO37的V3.2测量路径需要高电平/上拉，不可照搬早期逻辑。本次不读电池、不操作充电IC。USB/5 V输入按手册至少500 mA，直接3.3 V输入至少150 mA；初期用稳定5 V并留余量。手册868 MHz下14/17/22 dBm的整板约200/210/230 mA是厂商测试条件，不是本项目10 dBm实测值。检查电池极性、TX电压跌落和外设负载，不并联任意电源或通过GPIO反向供电。

## SX1262驱动与RadioLib7.8.1

生产适配器只拥有一个Module和一个SX1262。SPI为CS8/SCK9/MOSI10/MISO11，RESET12/BUSY13/DIO1=14，1 MHz、MSB-first、mode0。初始化明确给出915 MHz、BW125、SF7、CR4/5、sync0x12、10 dBm、preamble8、TCXO1.8 V和 `useRegulatorLDO=false`。

| 边界 | 源码结论和实施 |
|---|---|
| TCXO | DIO3连接X2供电，厂商代码用1.8 V；不用RadioLib默认1.6 V。库默认启动等待5000 us不是项目实测 |
| DIO2 | common begin检查setDio2AsRfSwitch(true)，不重复发送或增加外部TXEN/RXEN |
| 稳压 | L8/C54支持DC-DC；false选择setRegulatorDCDC |
| PA/电流 | begin默认60 mA OCP，SX1262完成PA-clamp和输出设置；保留10 dBm，不自动使用最大功率 |
| Header/CRC | 库begin已设explicit/CRC2；Driver的再次设置虽重复，但保留已有错误边界和测试，不在迁移中删除 |
| RX/IRQ | infinite timeout0xFFFFFF，全generic mask0x272映射DIO1；编译断言核对真实库的raw bits/status |
| TX/完成 | standby、start、finish、read结果按原接口上报；Driver 2 s deadline和状态优先级不变 |
| 长度/指标 | 转发length、RSSI/SNR、time-on-air；无status的getter仍不能报告所有SPI失败 |
| ISR | 只置volatile latch，主循环处理；level fallback保留，不声称native证明真实中断竞争 |

配置结构体overload不能直接替代本次位置参数overload，因为后者先设置oscillator/regulator字段。适配器测试检查实际参数转发，真实库内部初始化仍靠固定源码检查和目标编译。

### ERR-021继续存在

实际路径是RadioDriver.transmit -> Sx1262Hardware.startTransmit -> PhysicalLayer.startTransmit -> SX126x.stageMode/launchMode(TX) -> setTx(0)，随后SX126x.cpp1121–1123的BUSY/yield循环无显式deadline。它发生在setTx成功后，Driver超时无法抢占。换为SX1262没有修复该问题。

保留OPEN：后续可单独审查上游修复、有明确deadline的项目HAL/命令路径，或明确配置并故障测试的watchdog/复位方案。本次未改第三方库或watchdog；须在安全RF终端和受控BUSY注入下验证，无人值守恢复仍未通过。

## PlatformIO、USB和内存

已安装espressif32 55.03.311支持官方板定义，无需升级整个工具链或引入Heltec库。环境改为heltec_wifi_lora_32_V3，esptool上传、115200，RadioLib7.8.1，C++17。删除项目复制的USB定义，明确CDC_ON_BOOT=0；官方board manifest自身保留USB_MODE=1和core affinity，不等于启用原生CDC。Serial选UART0/CP2102。GPIO19/20可选USB路由不能同时接两个USB transceiver。

HardwareSerial.availableForWrite进入UART后端，write通过带锁uartWriteBuf/uart_write_bytes，仍可能同步等待；日志128字节drain不是硬实时保证。以前HWCDC特定分析只属历史，不能当成本后端实测。

默认8 MB分区：NVS0x9000/0x5000、otadata0xe000/0x2000、app0x10000/0x330000、app1 0x340000/0x330000、spiffs0x670000/0x180000、coredump0x7f0000/0x10000。布局包含预留区不代表实现OTA。S3的512 KB物理SRAM与PlatformIO manifest320 KB报告口径不同；静态剩余空间不能代替heap/stack高水位。

原基线RAM24564、flash364308；本次S3为RAM32096、flash342560（应用分区3342336）。架构/SDK后端一起改变，不能把差值当成单个驱动的成本。无PSRAM假设，运行时heap/stack/碎片和双核IRQ时序仍需实测。

## 测试迁移

| Suite | 原用例 | 处理 |
|---|---:|---|
| App integration | 38 | 生产app原样，模拟transport/time |
| Store | 56 | 生产store和golden bytes保留；不是Flash持久性 |
| Runtime | 47 | 生产协调器，模拟SDK/reset/RNG |
| Codec | 18 | 字节和异常边界保留 |
| Console | 10 | parser/预算保留 |
| Radio driver | 71 | 全部保留，仅一处driver名称期望更新 |
| Role/config | 13 | schema/ID规则保留 |
| Reliability | 24 | boot-ID和依赖延迟保留 |
| Sequence | 7 | 去重/session/wrap保留 |
| Logger | 6 | formatter/queue保留；真实UART时延另测 |
| SX1262 adapter | 新12 | 编译实际adapter，以有界SPI/RadioLib调用记录检查板参数、TCXO/DC-DC、错误传播、IRQ、RX mask、字节/指标转发和初始化顺序 |
| Capture | 另13 | fake-port原测试保留，不开真实串口 |

原290全部通过，新总数302；capture另13，不混为RF数。第一次native编译因fixture重复常量失败，原日志保留，删除重复后全套通过（ERR-025）。实际S3编译使用真正RadioLib常量和Arduino variant；native headers只记调用/返回，不复制芯片、Driver或MAC模型。

## 频率、天线和规模

按用户要求保留915.0 MHz初期研究设置，未实现地区channel plan，也未把频率选择当成辐射操作许可。用户要求暂缓法规审查，既有部署前要求仍适用。SX1262支持SF5–SF12和更多BW，研究仍只选BW125/SF7–SF9，不自动ADR。厂商其他SF下的灵敏度或21±1 dBm最大输出不代表当前10 dBm链路性能。

915 MHz Moxon按要求作为设计基线接受，但制作、SWR、feed loss、gain、极化均未测量，不能声称匹配920–923 MHz。任何TX前均须接合适50欧姆pigtail/天线或负载。EIRP=conducted power+gain-feed loss；当前两角色同为10 dBm，不能凭未知gain保证任何角色EIRP要求。以后角色限功率需明确配置设计和测试，不在本次擅自改NVS。

用相同LoRa参数的标准公式计算，28-byte HELLO约66.816 ms、37-byte ACK约82.176 ms，尚未包含处理/切换/guard；5次交换至少0.745 s空中时间，70次约10.429 s。这是分析示例，既非仿真结果也非实测容量。后续beacon、retry、不同SF和网关下行占空预算会增加负载。当前8-session去重缓存不能证明70节点保留能力。单Radio每次只收一个PHY，时钟漂移、BUSY、beacon/ACK时序、未来Wi-Fi共存须在M3–M8前测量。

## 风险和实物步骤

| 风险 | 后续证据 |
|---|---|
| 错板/LF版本 | 卖家确认V3.2 HF，正反面照片和芯片标记；不再让用户另选型号 |
| TCXO/RF switch/DC-DC | 参数有源码依据，实际电压、BUSY/DIO1/RF仍须测 |
| CP2102与复位 | 识别port/USB桥接、实际upload、115200、capture开口复位和最终计数 |
| Vext/电池版本 | 保留36/37，未新增外设逻辑 |
| ERR-021 | 受控故障注入和有界恢复决策，保持OPEN |
| NVS自动格式化/跨芯片恢复 | 同一实板ROM备份、security/layout检查；不把C3完整镜像恢复到S3 |
| 供电/开路RF | 稳定5 V、测TX电流/压降、始终连接RF终端 |
| 天线/EIRP | 测量/可靠规格和操作授权分开记录 |
| 内存/熵/双核时序 | ELF预算后做heap/stack/IRQ/reset测量 |
| 测试误用 | 分开编译证据、模拟输入和真实物理结果 |

采购要求：V3.2 HF、902–928 MHz订单选项（厂家HF覆盖863–928）、S3FN8/SX1262、8 MB、IPEX1.0、频率匹配天线、数据USB-C线、稳定5 V、正确pigtail、50欧姆负载和VNA/等效仪器。到货数量和实板标识NOT AVAILABLE。

首先RF终端接好，再识别/ROM私有备份、核对S3安全和分区后上传。集成radio始终在板上，不能套用“无外置radio就不会TX”的单MCU流程。显式配置Gateway1/1/1、Client1/2/1，检查保存后只在重启激活、无效配置禁TX、供电/RESET/BUSY/DIO1，最后做至少200次闭合cohort HELLO/ACK和受控重复/错身份/timeout测试。M0–M2实物门槛保持开放；M3–M8未开始。
