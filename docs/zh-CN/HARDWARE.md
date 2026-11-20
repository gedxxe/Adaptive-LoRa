# 硬件与射频基线

本研究使用两块 **Heltec WiFi LoRa 32 V3.4**，配有 ESP32-S3、SX1262 和原装单极天线。
用户确认采购选项为 902–928 MHz，V3.4 沿用 V3.2 的电气线路和引脚分配，改进主要涉及
SMT 工艺和晶振。V3.2 原理图继续作为本研究的电气参考。

V3 系列厂商资料列出 ESP32-S3FN8、SX1262、8 MB flash、CP2102 和 128 × 64 OLED。
两块板的实物 SKU、天线标记/频段/匹配、线路导通、晶振、USB/flash、供电及 OLED
行为仍需直接检查。当前没有实物测量或采集记录。

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

USB/5 V供电按手册至少500 mA能力，直接3.3 V至少150 mA；用稳定5 V并留余量。测TX电压/电流、reset和温升，检查电池极性，不并联任意电源或从GPIO反向供电。射频在板上不能拆除，未知出厂固件或已保存角色可能发射，第一次上电和单板操作前也须接好 IPEX1.0 对应的 50 欧姆负载或天线，禁止开路发射。

固定 PHY 为 915.0 MHz、BW125、SF7、CR4/5、前导 8、显式头、PHY CRC、sync 0x12、
请求功率 22 dBm、SPI 1 MHz/mode 0/MSB。两种镜像均使用 SF7，SF9 保留。RadioLib
7.8.1 在设定 PA 后恢复其 60 mA 默认过流保护，适配器随后设置 SX1262 高功率
140 mA OCP 并检查返回值，见 [Semtech 数据手册](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)
表 5-2。[Heltec V3 规格](https://heltec.org/project/wifi-lora-32-v3/)为 21 ± 1 dBm，
实板传导功率、电流、电压跌落和温升待测。辐射须满足当地获准的频率、EIRP（传导功率加
天线增益减馈线损耗）、空口占用及场地条件。

915.0 MHz 是固定的暂定研究频点。辐射测试须使用获准的射频配置。首轮链路使用原装单极天线，
先核对天线标记和适用频段。Moxon 不属于本轮测试。

实物验收按BENCH_RUNBOOK：确认板/RF版本、Flash/security/USB、供电、RF终端、TCXO/BUSY/DIO1/reset、串口和实际HELLO/ACK。编译/native不证明这些。ERR-021仍阻挡卡BUSY无人值守恢复，未改库或watchdog；ERR-023是SDK runtime前可能格式化NVS，须在app boot/配置改动前完成同板私有备份和恢复计划。
