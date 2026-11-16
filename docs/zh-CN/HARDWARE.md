# 硬件与射频基线

当前唯一目标为Heltec WiFi LoRa 32 V3.2 HF，ESP32-S3FN8/SX1262、8 MB Flash、512 KB SRAM、CP2102，不假定PSRAM。采购选型已确定，但到货数量、实际板标记和silicon stepping未检查。首个验收配置是一台Gateway和一台Client，使用相同硬件。

要求HF版本，厂家覆盖863–928 MHz，采购选项902–928 MHz；不接受LF470–510 MHz或其他Heltec产品替代。需要卖家确认、V3.2 HF正反面照片、数据USB-C线、稳定5 V、IPEX1.0匹配pigtail、合适天线/50欧姆负载。完整来源、revision/GPIO矩阵见HARDWARE_MIGRATION_AUDIT。

| 信号 | GPIO |
|---|---:|
| NSS | 8 |
| SCK | 9 |
| MOSI | 10 |
| MISO | 11 |
| RESET | 12 |
| BUSY | 13 |
| DIO1 | 14 |

映射有V3.2图纸和Arduino variant依据，实板仍须核对。DIO2控制板上RF switch，DIO3为32 MHz TCXO供电1.8 V，radio使用已装DC-DC网络；不额外分配TXEN/RXEN。唯一Sx1262Hardware拥有RadioLib/SPI/GPIO/IRQ操作。

保留OLED17/18/21、LED35、Vext36、ADC_Ctrl37、UART43/44、PRG0和其他strap/Flash/晶振/JTAG资源。19/20原生USB路由的R3/R29在参考图上为NC，USB-C默认接CP2102，不可把两个USB transceiver并接。

V3.2 GPIO36低有效控制Vext LDO；Radio和RF bias用VDD_3V3，OLED用Vext，本次不初始化OLED或外设供电。以后加显示功能时先设安全锁存，开启电源并按要求稳定后再释放reset。GPIO37高/上拉使能新分压检测，不复制早期逻辑；充电IC为LGS4056HDA，本次不做电池功能。

USB/5 V供电按手册至少500 mA能力，直接3.3 V至少150 mA；用稳定5 V并留余量。测TX电压/电流、reset和温升，检查电池极性，不并联任意电源或从GPIO反向供电。射频在板上不能拆除，未知factory固件或已保存角色可能TX，所以第一次上电和单板操作前也要接好IPEX1.0对应50欧姆负载/天线，禁止开路发射，初始10 dBm。

固定PHY为915.0 MHz、BW125、dataSF7、保留controlSF9、CR4/5、preamble8、explicit、PHY CRC、sync0x12、10 dBm、SPI1 MHz/mode0/MSB。SX1262支持SF5–SF12，但研究仍限制SF7–SF9，不新增动态PHY/功率/调度。

按用户要求保留915 MHz研究设置，不据此声称辐射许可；部署前仍须核对适用频率/EIRP/access/设备要求。EIRP=输出+天线gain-feed loss，未知参数不能当零。915 MHz Moxon作为设计基线接受，但制作、SWR、gain和方向图未测，不声称920–923 MHz匹配。后续对比须保持相同PHY、功率、feed、距离、高度、极化和时长。

实物验收按BENCH_RUNBOOK：确认板/RF版本、Flash/security/USB、供电、RF终端、TCXO/BUSY/DIO1/reset、串口和实际HELLO/ACK。编译/native不证明这些。ERR-021仍阻挡卡BUSY无人值守恢复，未改库或watchdog；ERR-023是SDK runtime前可能格式化NVS，须在app boot/配置改动前完成同板私有备份和恢复计划。
