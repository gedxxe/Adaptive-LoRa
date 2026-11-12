# Adaptive-LoRa 通信协议

## 1. 协议范围

协议第一版先实现固定 PHY 的 HELLO/ACK。后续再逐步加入 Beacon、TDMA、ACK
Bitmap、选择性重传和 ADR。

本项目是自定义 LoRa P2P 协议，不是 LoRaWAN。

## 2. 帧限制

- 完整应用帧最大 96 字节。
- 固定 Header 长度 28 字节。
- Payload 最大 68 字节。
- 多字节整数使用 little-endian。
- PHY CRC 必须开启。
- 禁止直接发送 C++ struct。
- 解码器必须检查所有长度和字段。

## 3. Protocol v1 Header

| Offset | 字段             | 长度 | 编码                  |
|-------:|------------------|-----:|-----------------------|
|      0 | magic            |    2 | u16 LE，常量 `0x4D4C` |
|      2 | protocol_version |    1 | u8，值为 1            |
|      3 | message_type     |    1 | u8                    |
|      4 | flags            |    1 | v1 保留，发送时为 0   |
|      5 | header_length    |    1 | 必须为 28             |
|      6 | network_id       |    2 | u16 LE                |
|      8 | source_id        |    2 | u16 LE                |
|     10 | destination_id   |    2 | u16 LE                |
|     12 | boot_id          |    4 | u32 LE                |
|     16 | frame_id         |    4 | u32 LE                |
|     20 | sequence         |    2 | u16 LE                |
|     22 | config_version   |    2 | u16 LE                |
|     24 | schedule_version |    2 | u16 LE                |
|     26 | payload_length   |    1 | u8                    |
|     27 | reserved         |    1 | v1 必须为 0           |
|     28 | payload          | 0–68 | 消息内容              |

总长度必须等于 `28 + payload_length`，且不能超过 96 字节。

## 4. 消息类型

|     值 | 类型           |
|-------:|----------------|
| `0x01` | HELLO          |
| `0x02` | ACK            |
| `0x10` | BEACON         |
| `0x11` | DATA           |
| `0x12` | ACK_BITMAP     |
| `0x20` | CONFIG_PROPOSE |
| `0x21` | CONFIG_ACCEPT  |
| `0x22` | CONFIG_COMMIT  |
| `0x30` | REJOIN_REQUEST |
| `0x31` | REJOIN_GRANT   |
| `0x7F` | DIAGNOSTIC     |

## 5. HELLO / ACK

HELLO 第一版可以使用空 Payload。ACK Payload 必须回显原始 HELLO
的身份信息：

| 字段            | 长度 |
|-----------------|-----:|
| acked_source_id |    2 |
| acked_boot_id   |    4 |
| acked_sequence  |    2 |
| status          |    1 |

Client 只有在 source ID、boot ID 和 sequence 全部匹配时才能认为 ACK
成功。

M2 编解码允许 HELLO 使用 0–68 字节不解释的 Payload，当前 Client 发送空 Payload。ACK 仅实现 status=0，Payload 必须恰好为 9 字节；回显的 source 必须等于 ACK destination，boot_id 非零。M2 只允许单播，源/目的地址为 1–65534 且不同；Network ID 为 1–65535。未来保留消息类型及非零 frame/config/schedule 字段暂不接受。v1 Header 布局不变。

## 6. 去重

逻辑数据包身份：

`network_id + source_id + boot_id + sequence`

重传必须复用相同身份和 Payload。Gateway 不得将同一个逻辑 DATA
包重复交付给上层应用。16 位 Sequence
即将溢出时，停止发送新包，结束或超时处理未完成事务，生成新的非零
boot_id，重置 Sequence，并重新建立会话。

M2 去重保留 8 个 session key，每个有 64 个 sequence 的窗口；重复 HELLO 再次 ACK，不重复接受。窗口以前的 sequence 视为 stale。旧 session 淘汰和 Gateway 重启后历史丢失；此缓存不提供持久化重放保护或身份认证，M4 须重新评估容量和生命周期。

## 7. ACK Bitmap

ACK Bitmap 使用当前 `schedule_version` 的客户端顺序进行映射，bit
顺序为每字节最低有效位优先。

- 1 个客户端：1 字节。
- 10 个客户端：2 字节。
- 70 个客户端：9 字节。

Payload 包含 `active_client_count`、`bitmap_length` 和
bitmap。最后一个字节未使用的高位必须为 0。

客户端状态必须区分： - `ACK_RECEIVED` - `MISSING` - `ACK_UNKNOWN`

重传使用确定性时隙，且复用原始 packet
identity。初始最多重传两次。Gateway 必须去重。

## 8. 保守 ADR

ADR 由 Gateway 控制，初期只允许 BW125 下的 SF7、SF8、SF9。

使用 RSSI、SNR、PDR/PER、ACK
结果和重传次数的滑动窗口。必须有滞回机制，不得根据单个数据包切换 PHY。

通过 CONFIG_PROPOSE、CONFIG_ACCEPT、CONFIG_COMMIT
完成配置握手，并在约定的 frame boundary 生效。若 COMMIT 未被确认，Client
保留旧配置。
