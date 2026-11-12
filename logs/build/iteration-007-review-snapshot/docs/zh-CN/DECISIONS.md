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
