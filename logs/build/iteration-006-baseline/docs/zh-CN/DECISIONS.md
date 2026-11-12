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
