# Adaptive-LoRa 身份与发布审查

研究标题：Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks.

源码修改前已检查 253 个候选文本文件（包括历史日志/快照）。英文完整逐文件清单见 [IDENTITY_AUDIT.md](../en/IDENTITY_AUDIT.md)：组织/旧项目标签 261 处、站点标签 4 处、工作站路径问题 110 处、设备枚举标识 6 处；高置信凭据候选 0。未发现需公开的论文全文、压缩包、私钥或环境文件。该检查不是对不存在敏感信息的形式化保证。

原始未脱敏副本保存在发布树外，不提交其路径或旧标签。公开记录保留迭代顺序、已有时间戳、版本、测试数/结果、错误描述及解决状态；只中和身份/路径/设备标签并规范行尾空白。旧 hash 仍标识原工件，不声称脱敏副本逐字节相同；转换清单为 `logs/build/iteration-008-sanitization-manifest.json`。

当前源码只机械重命名命名空间/native 宏。NVS namespace 的原字节以数值兼容常量保留，否则会导致旧配置无法寻址；它不是部署依赖，旧标签不出现在公开文本。协议、schema/key/blob、版本 0.2.0-alpha.1 / 1 和固定 PHY 不变。

完整原型仍为 ESP32-C3 Super Mini + AI-Thinker RA-01SCH/LLCC68 + RadioLib 7.8.1，计划 915 MHz Moxon。实际为两个模块、一个 MCU；第二个 MCU/天线未就绪。92/92 native（38 integration）与 ESP32 编译是软件证据，不是实物 NVS/重启/SPI/IRQ/RF/天线验证。

目标架构有协议核、radio、platform、application 四个边界；当前实现/缺口见 ARCHITECTURE。未添加 TDMA、bitmap、选择性重传、ADR 或外部集成。

本地原无 Git 仓库，现以 main 初始化并使用公共账户的 noreply 身份；全局配置未修改。目标仓库 API 和 Git fetch 均显示远端为空，没有初始 README commit 需要合并。最终编译、测试、暂存审查、commit 与 push 结果将记录在 BUILD_MEMORY/PROGRESS 和发布验证记录中。不改仓库可见性，不强制推送，不自动打 tag。
