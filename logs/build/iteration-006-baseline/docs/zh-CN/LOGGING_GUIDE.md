# 串口日志说明

英文规范见 `docs/en/LOGGING_GUIDE.md`。M2 每行一个事件，使用逗号分隔的 `key=value`，未知值为 N/A。帧日志包含 network/source/destination、boot_id、sequence 和版本字段，以及固定 SF/BW/频率；接收成功时带 RSSI/SNR。

`TX_START`/`ACK_TX` 表示开始，`TX_DONE` 才表示完成。`toa_us` 是 RadioLib 计算的空中时间，不是实测延迟。Client `LINK_COUNTS` 分别报告尝试、成功 ACK、ACK 超时、TX 失败和事务耗时。Header sequence 属于各发送方；Gateway ACK_TX 额外记录原 HELLO 身份。

16 行队列每 loop 最多排空 128 字节，不在 ISR 打印。`status` 中 `log_dropped` 不为零时应重做完整捕获。`CRC_ERROR` 仅代表已暴露的 CRC/Header IRQ，不能声称完整 CRC 失败计数。所有原始日志和测量条件应存入独立 run 目录。
