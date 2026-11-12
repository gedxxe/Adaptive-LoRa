# 串口日志说明

英文规范见 `docs/en/LOGGING_GUIDE.md`。M2 每行一个事件，使用逗号分隔的 `key=value`，未知值为 N/A。帧日志包含 network/source/destination、boot_id、sequence 和版本字段，以及固定 SF/BW/频率；接收成功时带 RSSI/SNR。

`TX_START`/`ACK_TX` 表示开始，`TX_DONE` 才表示完成。`toa_us` 是 RadioLib 计算的空中时间，不是实测延迟。Client `LINK_COUNTS` 分别报告尝试、成功 ACK、ACK 超时、TX 失败和事务耗时。Header sequence 属于各发送方；Gateway ACK_TX 额外记录原 HELLO 身份。

16 行队列每 loop 最多排空 128 字节，不在 ISR 打印。`status` 中 `log_dropped` 不为零时应重做完整捕获。`CRC_ERROR` 仅代表已暴露的 CRC/Header IRQ，不能声称完整 CRC 失败计数。所有原始日志和测量条件应存入独立 run 目录。

审查更正（ADR-010/ERR-010）：队列处理和单次请求字节数有界，但 SDK availableForWrite/write 可能等待 mutex/ring buffer，不能声称端到端已证明非阻塞。Native 回压/短写测试只验证队列，不验证 USB/UART 延迟。

## Iteration013 采集与CSV规范

Host capture schema2不是协议版本。支持--board单MCU或不同--gateway/--client；默认仅开口和正常结束时各发status，结束再读1s，--no-status纯被动。开口前DTR/RTS=false仍不能保证不reset。4096-byte原始读取直接写.log；chunks.jsonl记录offset/length/UTC/monotonic_ns，capture.json记录状态/byte/hash/换行/tail/error，可带<=64KiB operator JSON。scriptSHA不是目标固件SHA，实际烧录commit/bin/config须操作者记录。

RUNNING在开口前写；COMPLETE/exit0仅表示采集完成，不是RF验收。Ctrl-C为INTERRUPTED/130；IO/短写/未退出worker/磁盘错误ERROR/1；CLI错误2。读0.2s/写1s、每worker join3s，阻塞driver仍可能需进程/设备处置；unjoined hash/长度不可用。检查raw SHA/长度与连续chunk offset/time；它不能检测读取前USB/UART丢失。首尾碎行/空流/畸形行须标记；仅可排除已关闭分析窗口之外的尾片段，不可把半事务算RF丢包。错误/中断采集须重跑主验收。

英文段定义完整events.csv列；每完整raw行保存byte范围，对应结束byte的chunk时间。host时刻为观察/合并包时间，不是RF/IRQ/同步MCU时钟。缺值/N/A不变0。BOOT/uptime回退/操作reset分session，歧义不猜关联，BOOT计数不是独立实测reset数。

HELLO逻辑K=(network,Client source,Client boot,HELLO seq)。A包括TX_START hello或HELLO_FAILED tx_start_failed（后者没有TX_START）；T为Client TX_DONE hello_sent；R为cohort内Gateway HELLO_RX/DUPLICATE的唯一K，分别报new/duplicate事件。ACK TX_START使用Gateway自身header身份，邻近ACK_TX回显K；TX_DONE按Gateway header匹配后关联K，不能用ACK_TX代替完成。Client ACK_RX accepted按唯一outstanding K和后续LINK_COUNTS增量关联，日志不独立显示整个ACK payload。

Timeout/TX失败按K/result/code区分；RADIO_ERROR和HELLO_FAILED可属于同一次故障，事件数不是独立丢包数。RSSI/SNR按Gateway new/duplicate及Client acceptedACK方向分别统计，不重复算RX_DONE。LINK_COUNTS transaction_us从startTransmit返回后sentAtUs到finish，不含start调用时延，不是一程/turnaround；成功/timeout/failure分开。两侧稳定boot的初/末STATUS log_dropped须0且无reset歧义，新reset后的0不能代替末值。

transactions.csv每K一行，明确api/start/done/R/dup/ACKstart/done/accepted/terminal/latency/error及unknown；关闭边界对齐LINK_COUNTS attempts/success/timeouts/txfail增量。正常物理run需>=200个完整T，另报A/启动失败/排除边界。PDR=|R交T|/|T|（另报未配对R异常）；事务ACK成功=|C|/|A|，C/T另列。ACK条件率写明分母，duplicate事件率用有效接收事件数。分母0为N/A，不截断分子、不混boot、不从IRQ猜总CRC失败、不挑有利窗口。既有短距>=99%目标按C/A，是未验证工程目标；注入/断链另跑，不把host/native结果算物理样本。完整CSV列/关联表以英文为准。

上文future BEACON/slot/bitmap/ADR事件属于后续milestone，M0-M2无这些事件不算采集失败。
