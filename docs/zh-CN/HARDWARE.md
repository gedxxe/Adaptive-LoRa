# 硬件与射频基线

用户报告已收到两块标记 V3.4 的 Heltec WiFi LoRa 32 及配套单极天线。厂商 V3
资料列出 ESP32-S3FN8、SX1262、8 MB flash、CP2102 和 128 × 64 OLED。公开图纸
与改版记录到 V3.2。V3.4 的 SMT/晶振变化及连接沿用情况还需图纸或实物核对。
两块板的 RF SKU、天线频段、前后布局、芯片与电气参数尚无独立确认。

| 项目 | V3.2 图纸及 Arduino V3 参考 | 实物待查 |
|---|---|---|
| LoRa | NSS/SCK/MOSI/MISO/RESET/BUSY/DIO1 为 8/9/10/11/12/13/14 | 比对网络，观察 RESET、BUSY 和 DIO1 |
| 射频供电 | DIO2 控制 RF switch，DIO3 供 1.8 V TCXO，DC-DC | 核实器件标记、开关和供电拓扑 |
| OLED | SSD1306 128 × 64、7-bit 地址 0x3C，SDA/SCL/RESET 为 17/18/21 | 核对线路、地址 ACK 和实际屏幕响应 |
| Vext | GPIO36 低有效，OLED 用 Vext，radio 用 VDD_3V3 | 确认极性及稳定时间 |
| USB 和 flash | CP2102 UART0 43/44，8 MB flash | 逐块枚举，读 ROM 标识和分区/安全状态 |
| RF 接头/天线 | 参考 IPEX 1.0，V3 有多个频段 SKU | 识别两板 SKU 和原装天线频段及匹配 |

OLED 候选构建明确指定 Arduino variant 的 OLED 引脚，而非 Wire 默认 41/42。
主构建保持显示关闭，既有 LoRa 引脚常量保留。外设资源与来源见
[英文参考矩阵](../en/HARDWARE.md) 和历史迁移记录。确认实物后再启用 OLED。

USB/5 V供电按手册至少500 mA能力，直接3.3 V至少150 mA；用稳定5 V并留余量。测TX电压/电流、reset和温升，检查电池极性，不并联任意电源或从GPIO反向供电。射频在板上不能拆除，未知factory固件或已保存角色可能TX，所以第一次上电和单板操作前也要接好IPEX1.0对应50欧姆负载/天线，禁止开路发射，初始10 dBm。

固定PHY为915.0 MHz、BW125、dataSF7、保留controlSF9、CR4/5、preamble8、explicit、PHY CRC、sync0x12、10 dBm、SPI1 MHz/mode0/MSB。SX1262支持SF5–SF12，但研究仍限制SF7–SF9，不新增动态PHY/功率/调度。

按用户要求保留915 MHz研究设置，不据此声称辐射许可；部署前仍须核对适用频率/EIRP/access/设备要求。EIRP=输出+天线gain-feed loss，未知参数不能当零。915 MHz Moxon作为设计基线接受，但制作、SWR、gain和方向图未测，不声称920–923 MHz匹配。后续对比须保持相同PHY、功率、feed、距离、高度、极化和时长。

实物验收按BENCH_RUNBOOK：确认板/RF版本、Flash/security/USB、供电、RF终端、TCXO/BUSY/DIO1/reset、串口和实际HELLO/ACK。编译/native不证明这些。ERR-021仍阻挡卡BUSY无人值守恢复，未改库或watchdog；ERR-023是SDK runtime前可能格式化NVS，须在app boot/配置改动前完成同板私有备份和恢复计划。
