# M0–M2实物准备

当前流程为BENCH_RUNBOOK.md，目标Heltec WiFi LoRa 32 V3.2 HF。源码迁移和host验证已完成，没有烧录或实物测试。到货/PCB/RF确认后先接RF终端，上电/首次app boot前完成同板ROM私有备份、安全/分区检查。单板仍有集成radio，不能使用以前拆除外置radio的单MCU流程。先测串口/供电/TCXO/BUSY/DIO1/reset/NVS，再做双板闭合cohort HELLO/ACK；ERR-021/023和真实重复/错身份/corrupt-NVS/恢复fixture继续开放。M0–M2实物IN PROGRESS，M3–M8未开始。采购/GPIO/API/风险见迁移审查，采集/CSV规则见LOGGING_GUIDE。
