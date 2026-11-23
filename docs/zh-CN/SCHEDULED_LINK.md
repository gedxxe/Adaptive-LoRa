# 协议与公式

[首页](../../README.zh-CN.md) · [烧录与现场测试](BENCH_RUNBOOK.md) · [结果](PROGRESS.md) · [English](../en/SCHEDULED_LINK.md)

## 帧格式

![单射频上的控制帧、DATA 与重传顺序](../research/figures/adaptive-frame.svg)

协议 5 使用小端编码，帧头 28 字节，整帧最多 96 字节。帧头包含网络、源地址、目的地址、
发送方 boot ID、帧号、序号及调度版本。Gateway 广播控制帧，Client 向 Gateway 发送 Request 和 DATA。

| 帧 | 发送方 | 帧头之后的字段 |
|---|---|---|
| Beacon | Gateway | 周期 µs (u32)、节点数 (u8)，每个节点的 ID (u16) 和字节上限 (u8) |
| Request | Client | Gateway boot ID (u32)、请求字节数 (u8)、本会话上次确认的帧号 (u32，无则填零) |
| Grant | Gateway | 节点数 (u8)，每项为授权字节数 (u8)、Client boot ID (u32) 和 DATA SF (u8) |
| DATA | Client | Gateway boot ID (u32)，随后是授权的应用字节 |
| ACK 位图 | Gateway | 节点数、接收和重传位图（各 u8），随后是各位置的 Client boot ID (u32)，未确认填零 |

Grant 空项长度为 `0xFF`，boot ID 和 SF 为零。解码器检查长度、版本、网络、目的地址和
载荷布局。运行时核对当前帧、会话 ID、Grant 和接收窗口。控制帧共用一个 SF，DATA 首传与
重传使用授权 SF。旧自适应解码器会丢弃包含 SF10 的整个 Grant，因此空口版本 5 要求
Gateway 和所有 Client 一起升级。

## 空口时间和时隙预算

$\lambda$ 是送入 LoRa 调制器的字节数，不含调制器添加的物理层头和 CRC。
$\sigma$ 为扩频因子，$\beta$ 为以 Hz 表示的带宽。
[Semtech LoRa 计算器](https://www.semtech.com/design-support/lora-calculator)

```math
\begin{aligned}
\tau_{\mathrm{sym}}(\sigma)&=\frac{2^\sigma}{\beta},\\
\nu_{\mathrm{coded}}(\lambda,\sigma)
  &=\max\!\left(0,\left\lceil
     \frac{\kappa\lambda-\alpha\sigma+\zeta_0+
        \zeta_{\mathrm{CRC}}\iota_{\mathrm{CRC}}-
        \zeta_{\mathrm{IH}}\iota_{\mathrm{IH}}}
      {\alpha(\sigma-\mu\iota_{\mathrm{DE}})}
     \right\rceil\right).
\end{aligned}
```

```math
\begin{aligned}
\nu_{\mathrm{sym}}(\lambda,\sigma)
  &= \pi+\psi+\xi+\gamma\nu_{\mathrm{coded}}(\lambda,\sigma),\\
\tau_{\mathrm{air}}(\lambda,\sigma)
  &= \nu_{\mathrm{sym}}(\lambda,\sigma)\,\tau_{\mathrm{sym}}(\sigma).
\end{aligned}
```

| 符号 | 含义 | 当前设置或公式系数 |
|---|---|---|
| $\kappa$、$\alpha$、$\mu$ | 每字节比特数、SF 系数、低速率优化分母修正量 | 8、4、2 |
| $\zeta_0$、$\zeta_{\mathrm{CRC}}$、$\zeta_{\mathrm{IH}}$ | 分子中的基准、CRC 及隐式头系数 | 28、16、20 |
| $\iota_{\mathrm{CRC}}$、$\iota_{\mathrm{IH}}$、$\iota_{\mathrm{DE}}$ | CRC、隐式头、低速率优化标志 | 当前 SF 范围采用 1、0、0 |
| $\pi$、$\psi$、$\xi$ | 配置的前导符号、固定同步后缀、基础载荷符号 | 8、4.25、8 |
| $\gamma$ | 每编码组符号数，即编码率的分母 | CR 4/5 时为 5 |
| $\beta$、$\sigma_{\mathrm{ctrl}}$、$\sigma_\iota$ | 带宽、共用控制 SF、各 Client 的 DATA SF | 125 kHz、SF9、SF7–SF10 |

$\Omega$ 为公告名单，$\nu_\Omega=|\Omega|$ 包含有请求窗口的空闲节点。预留时取
$\Theta=\Omega$，$\lambda_\iota$ 为载荷上限；实际调度时 $\Theta$ 只包含获授权节点，
$\lambda_\iota$ 为授权长度。$\Theta^\star$ 由 $\Theta$ 内最多 $\varkappa$ 个最长时隙组成。

```math
\begin{aligned}
\eta &= \eta_{\mathrm{header}}+\eta_{\mathrm{session}},\\
\tau_{\mathrm{slot},\iota}
   &=\tau_{\mathrm{air}}(\lambda_\iota+\eta,\sigma_\iota)+\delta,\\
\tau_{\mathrm{data}}&=\sum_{\iota\in\Theta}\tau_{\mathrm{slot},\iota},\\
\tau_{\mathrm{retry}}&=\sum_{\iota\in\Theta^\star}\tau_{\mathrm{slot},\iota}.
\end{aligned}
```

```math
\begin{aligned}
\tau_{\mathrm{requests}} &= \delta+\nu_\Omega(\tau_{\mathrm{R}}+\delta)+\delta,\\
\tau_{\mathrm{bitmaps}} &= (\varrho+1)\tau_{\mathrm{A}} + \delta_{\mathrm{first}}
    +\varrho(\delta_{\mathrm{before}}+\delta_{\mathrm{after}})\\
    &\quad+\delta_{\mathrm{last}}+\delta_{\mathrm{close}},\\
\tau_{\mathrm{ctrl}} &= \tau_{\mathrm{B}}+\tau_{\mathrm{requests}}
    +\tau_{\mathrm{G}}+\tau_{\mathrm{bitmaps}}.
\end{aligned}
```

```math
\tau_{\mathrm{ctrl}}+\tau_{\mathrm{data}}+
\varrho\,\tau_{\mathrm{retry}}+\tau_{\mathrm{tail}}
\le \tau_{\mathrm{frame}}.
```

$\tau_{\mathrm{B}}$、$\tau_{\mathrm{R}}$、$\tau_{\mathrm{G}}$、$\tau_{\mathrm{A}}$
分别为控制 SF 下的 Beacon、Request、Grant 和位图空口时间，字节数为帧头加上表中字段。
$\tau_{\mathrm{requests}}$ 包含请求窗口及它们与 Grant 前的间隔。
$\tau_{\mathrm{bitmaps}}$ 包含所有反馈帧、首次 DATA 前的间隔、重传轮次前后的间隔、最后
一帧反馈前的间隔及尾部保护时间。各间隔均取 $\delta$。
Gateway 先核对不等式再预留更高 SF，试用较低 SF 时保留此前的额度。

| 符号 | 含义 | 当前自适应镜像 |
|---|---|---|
| $\eta_{\mathrm{header}}$、$\eta_{\mathrm{session}}$ | DATA 帧头、Gateway boot ID 字节数 | 28 B、4 B |
| $\nu_\Omega$、$\lambda_\iota$ | 测试名单数、单节点载荷上限 | 8 个、32 B。空口格式上限为 64 B。 |
| $\varrho$、$\varkappa$ | 重传轮数、每轮共享重传槽 | 3 轮、最多 4 槽 |
| $\delta$、$\tau_{\mathrm{tail}}$、$\tau_{\mathrm{frame}}$ | 时隙保护、帧尾预留、超帧周期 | 80 ms、300 ms、15 s |

切换提前量和发射窗口均为保护间隔的四分之一。接收窗口在预期完成时刻的前后各半个保护
间隔。调度以无线完成时间戳为基准，发射时隙过晚则跳过。实际切换时间和时钟漂移仍待测量。
[无线设置](../../include/radio_config.h) · [空口时间与长度](../../src/common/protocol/multiclient_protocol.h) ·
[时隙安排](../../src/common/protocol/multiclient_protocol.cpp)

### 15 秒容量

设各 Client 的载荷上限均为 $\lambda_\mathrm{cap}$，其中 $\nu'$ 个使用 SF9，其余使用 SF7。
可预留的 SF9 节点数为

```math
\nu_9^{\max}=\max\!\left\{\nu'\in\{0,\ldots,\nu_\Omega\}\;\middle|\;
\tau_{\mathrm{used}}(\Omega,\lambda_{\mathrm{cap}},\boldsymbol{\sigma}(\nu'))
\le\tau_{\mathrm{frame}}\right\}.
```

| Client 数 | 16 B | 24 B | 32 B | 64 B |
|---:|---:|---:|---:|---:|
| 4 | 4 | 4 | 4 | 4 |
| 6 | 6 | 6 | 6 | 3 |
| 8 | 8 | 8 | 5 | 2 |

自适应镜像的样本为 32 字节 ASCII（256 位），加 28 字节帧头与 4 字节 Gateway boot ID 后，
DATA 在计入 LoRa 物理层开销之前为 64 字节。五个节点用 SF9、三个用 SF7 时预留 14.905952 秒。
八个都用 SF9 则需要 15.722336 秒。八个 Client 全部申请时，Beacon、Grant 和四帧 ACK
占用 Gateway 约 2.361344 秒的发射时间。
包含 Request 与控制间隔的控制段为 6.019456 秒。以上均为计算值。
[测试上限](../../include/research_roster.h) · [容量模型](../../tools/model_multiclient.py)

名单固定时，增加单个时隙长度不会缩短 DATA 总时长，也不会缩短最长若干重传时隙的总和。
Grant 的载荷和 SF 均受各节点预留值限制，因此模型时长受名单预留预算约束。
真实收发切换和时钟误差仍需落在保护间隔内。

### SF10 容量及更高 SF 选项

按八个 Client、每节点 32 字节建模，保留 SF9 控制交互、四次累计 ACK 和全部重传预留。
余量列假设只有一个节点使用该 SF，其余七个均用 SF7；最右列给出其他节点维持 SF7 时可同时使用该 SF 的节点上限。

| DATA SF | 单个 64 字节 DATA 的空口时间（秒） | 单节点使用时的帧余量（秒） | 节点上限 |
|---|---:|---:|---:|
| SF7 | 0.118 | 4.720 | 8 |
| SF8 | 0.216 | 4.330 | 8 |
| SF9 | 0.390 | 3.632 | 5 |
| SF10 | 0.698 | 2.399 | 2 |
| SF11 | 1.561 | -1.050 | 0 |
| SF12 | 2.793 | -5.982 | 0 |

其余六个节点用 SF7 时，两个 SF10 节点在帧尾预留之外还剩 77.408 毫秒，第三个会让帧超出
2.244 秒。
Gateway 根据现有的 SF 预留核算预算，拒绝超额升档。

Beacon、Request、Grant 和 ACK 位图均使用 SF9。到 DATA 时隙时双方切换至授权 SF。
Client 切回 SF9 等反馈，Gateway 逐个切换到下一个 Client 的 SF，再回到 SF9。
控制链路中断后，Client 等待新的有效 Beacon 和 Grant。提高 DATA SF 只能改善 DATA 链路。

BW125 下 SF10 符号时长为 8.192 毫秒，无需 LDRO。96 字节包在 SF10 的空口时间为
0.985088 秒，低于驱动的 2 秒发送期限。SF11 和 SF12 只用于建模，需要 LDRO。
96 字节的 SF11 包及 64 字节的 SF12 包均会超过现有发送期限。
[RadioLib SF 范围](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x_config.cpp) ·
[RadioLib LDRO 选择](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x_commands.cpp)

## 自适应与恢复

<img src="../research/figures/adaptive-algorithm-adr.png" alt="Gateway SF 策略，包括会话恢复、近期丢包窗口、降档失败回退和确认。" width="480">

README 算法中的 $\phi$ 为帧 ID，$\phi_{\mathrm{ack}}$ 是同一 Gateway 会话中
最后收到 ACK 的帧，$\Gamma$ 为有效 Grant。第 $\jmath=0$ 轮为首传。
$\Theta_\jmath$ 是本轮有权发送的 Client 集合，$\Xi$ 累计记录已接受的 Client，
$\Pi_\jmath$ 从已获授权但 DATA 仍未到达的 Client 中选出最多 $\varkappa$ 个。
$\epsilon_\jmath$ 表示本地 Client 有权发送。位图缺失或无效时，两集合为空。
Gateway 接收 DATA 时检查帧、会话、载荷长度和时序。

设当前时刻为 $\tau$，计划发射时刻为 $\tau_{\iota,\jmath}$，预计完成时刻为
$\widehat{\tau}$，实际接收完成时刻为 $\tau_{\mathrm{rx}}$。发送和接收窗口以及
SNR 更新公式如下。

```math
\begin{aligned}
0 &\le \tau-\tau_{\iota,\jmath}<\delta_{\mathrm{tx}},\\
-\delta_{\mathrm{rx}} &\le \tau_{\mathrm{rx}}-\widehat{\tau}<\delta_{\mathrm{rx}},\\
\overline{\upsilon}_\iota^{\mathrm{next}}
   &=(1-\omega)\overline{\upsilon}_\iota+\omega\upsilon_\iota.
\end{aligned}
```

当前配置使用 $\delta_{\mathrm{tx}}=\delta/4$、$\delta_{\mathrm{rx}}=\delta/2$
和 $\omega=1/4$。当前 SF 首次有效的首传 SNR 样本 $\upsilon_\iota$ 用于初始化滤波。
尝试降低 SF 时，可以保留较高 SF 所需的时隙，以便回退。

下表列出 SF 策略的计数参数。帧编号和 ACK 回执都归属于当前 Gateway 会话。

| 符号 | 含义 | 当前值 |
|---|---|---:|
| $\nu_{\mathrm{obs}}$、$\nu_{\mathrm{up}}$ | 有效记录窗口长度和申请升档所需的首传失败次数 | 3、2 |
| $\nu_{\mathrm{idle}}$ | 释放较高 SF 额度的帧编号差 | 4 |
| $\nu_{\mathrm{lock}}$、$\nu_{\mathrm{probe}}$ | 锁定或尝试降档所需的连续首传成功次数 | 3、6 |
| $\nu_{\mathrm{ack}}$、$\nu_{\mathrm{hold}}$ | 降档试用等待回执的上限及回退等待期间的首传成功次数 | 6、8 |

$\epsilon_\iota^{\mathrm{fail}}$ 表示首传失败，即使重传后来成功也计入。
$\alpha_\iota$ 表示最新 Request 回报了已记录交付的帧，并确认对应 SF。
$\nu_\iota^{\mathrm{first}}$ 为连续首传成功次数，包含当前有效记录。
$\phi_\iota^{\mathrm{req}}$ 和 $\phi_\iota^{\mathrm{del}}$ 是上次 Request 和交付帧，
$\sigma_\iota^{\mathrm{del}}$ 是该次交付所用 SF。
$\sigma_\iota^{\mathrm{want}}$、$\sigma_\iota^{\mathrm{res}}$、
$\sigma_\iota^{\mathrm{mem}}$ 和 $\sigma_\iota^{\mathrm{back}}$ 依次表示目标、预留、记忆及回退 SF。
$h_\iota$ 是回退后剩余等待次数，$\theta_{\sigma_\iota-1}$ 是试用低一级 SF 的 SNR 门限。

```math
\boldsymbol{\xi}^{\mathrm{loss}}_\iota\gets
  \operatorname{Tail}_{\nu_{\mathrm{obs}}}
  \bigl(\boldsymbol{\xi}^{\mathrm{loss}}_\iota\mathbin{\|}\epsilon_\iota^{\mathrm{fail}}\bigr),
\qquad
\sum_{j}\xi^{\mathrm{loss}}_{\iota,j}\ge\nu_{\mathrm{up}}.
```

首传失败后，Gateway 检查当前 SF 的丢包窗口，时序异常和应用拒收不计入。累计两次失败后
清空窗口，并在下一次 Grant 前核算较高 SF 的额度。

连续三次首传成功且有同一 SF 的 ACK 回执后锁定，预算受限而分配较低 SF 时也适用。
锁定后记录已确认的 SF，并释放为回退保留的较高额度。锁定且等待期结束后，若连续六次首传成功且平滑 SNR
达到要求，可试用低一级（SF7 至少 −3.5 dB，SF8 至少 −6 dB，SF9 至少 −8.5 dB）。
试用期间首传失败即回退，连续六次有效首传仍无该 SF 的匹配 ACK 回执也回退。回退后需八次
首传成功才再次考虑降档。

Client 只有在收到有效位图授权时才重传。Gateway 只有在下一轮开始前及时处理位图发送完成事件，
才打开重传时隙。保留四帧累计 ACK，允许补确认。时序异常记入帧样本，并排除在 SF 评估之外。
对每个获分配的时隙，Gateway 会检查是否在切换提前量内已按指定 SF 就绪，或是否收到了有效
DATA；两项均无记录时，跳过该 Client 本帧的 SF 评估。
Beacon 过期后 Client 停止按时隙发射，并返回控制 SF 搜索。准备 Grant 时距某个 Client
上次 Request 达四个帧编号，Gateway 释放其较高 SF 的预留。相同 boot ID 的 Client 返回时，
Gateway 按当前预算重新申请由 ACK 回执或后续锁定记住的 SF。预算不足时可能分配较低 SF。
降档试用锁定前，保留此前的恢复 SF。Client boot ID 改变或 Gateway 启动新会话时清除该记录。
同步无线调用仍可能
超出软件时序窗口。[LaTeX 源文件](../research/figures/adaptive-algorithm.tex) · [运行时代码](../../src/app/multiclient_link.cpp) · [已知时序风险](../en/ERROR_LOG.md)

## 板卡参考

Heltec V3.4 使用 ESP32-S3、SX1262 和 CP2102 UART0，以 V3.2 原理图为电气核对参考。
GPIO 8–14 依次为 NSS、SCK、MOSI、MISO、复位、BUSY 和 DIO1。DIO2 控制射频开关，
DIO3 给 1.8 V TCXO 供电。显示连接 SDA 17、SCL 18、复位 21 和低电平启用的 Vext 36。
研究频率暂定 915.0 MHz，固件请求 22 dBm 及 140 mA PA 过流保护，走测前核实实板性能和射频版本。
[引脚定义](../../include/board_pins.h) ·
[Heltec 原理图](https://s.heltec.cn/download/WiFi_LoRa_32_V3/WiFi_LoRa_32_V3.2_Schematic_Diagram.pdf)
