# 协议与公式

[首页](../../README.zh-CN.md) · [烧录与现场测试](BENCH_RUNBOOK.md) · [结果](PROGRESS.md) · [English](../en/SCHEDULED_LINK.md)

## 帧格式

![单射频上的控制帧、DATA 与重传顺序](../research/figures/adaptive-frame.svg)

下表说明默认自适应空口 v5。轮换配置沿用这些阶段，分别使用
[空口 v6 分组选择器](#rotating-schedule)和[空口 v7 安全封装](#secure-wire-v7)。
网关和所有客户端须使用同一空口版本。

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

<a name="airtime-and-slot-budget"></a>

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
\eta &= \eta_{\mathrm{header}}+\eta_{\mathrm{session}}+\eta_{\mathrm{tag}},\\
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
分别为控制 SF 下的 Beacon、Request、Grant 和位图空口时间。
按所选配置的完整帧长度计算，包括公共帧头和 v7 的安全字段。
$\tau_{\mathrm{requests}}$ 包含请求窗口及它们与 Grant 前的间隔。
$\tau_{\mathrm{bitmaps}}$ 包含所有反馈帧、首次 DATA 前的间隔、重传轮次前后的间隔、最后
一帧反馈前的间隔及尾部保护时间。各间隔均取 $\delta$。
Gateway 先核对不等式再预留更高 SF，试用较低 SF 时保留此前的额度。

| 符号 | 含义 | 默认自适应镜像（空口 v5） |
|---|---|---|
| $\eta_{\mathrm{header}}$、$\eta_{\mathrm{session}}$ | DATA 帧头、Gateway boot ID 字节数 | 28 B、4 B |
| $\eta_{\mathrm{tag}}$ | DATA 认证标签 | v5 为 0 B，v7 为 16 B |
| $\nu_\Omega$、$\lambda_\iota$ | 测试名单数、单节点载荷上限 | 8 个、32 B。空口格式上限为 64 B。 |
| $\varrho$、$\varkappa$ | 重传轮数、每轮共享重传槽 | 3 轮、最多 4 槽 |
| $\delta$、$\tau_{\mathrm{tail}}$、$\tau_{\mathrm{frame}}$ | 时隙保护、帧尾预留、超帧周期 | 80 ms、300 ms、15 s |

v5 的切换提前量和发射窗口均为保护间隔的四分之一。[v7 预算](#secure-schedule-capacity)
另计处理耗时。接收窗口在预期完成时刻的前后各半个保护
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

八节点配置继续使用 SF9 控制。如果只将控制改为 SF10，八个 Request 与四次 ACK 位图
在内的控制时段将使模型帧增加 3.721 秒。32 B、15 秒的配置下，其余节点均用 SF7 时，
最多只容纳两个 DATA SF8 节点。仅一个 DATA SF9 节点就会超出 89.504 毫秒。
Gateway 每帧控制发射时间将从 2.361 秒增至 4.272 秒。

Beacon、Request、Grant 和 ACK 位图均使用 SF9。到 DATA 时隙时双方切换至授权 SF。
Client 切回 SF9 等反馈，Gateway 逐个切换到下一个 Client 的 SF，再回到 SF9。
控制链路中断后，Client 等待新的有效 Beacon 和 Grant。提高 DATA SF 只能改善 DATA 链路。

BW125 下 SF10 符号时长为 8.192 毫秒，无需 LDRO。96 字节包在 SF10 的空口时间为
0.985088 秒，低于驱动的 2 秒发送期限。SF11 和 SF12 只用于建模，需要 LDRO。
96 字节的 SF11 包及 64 字节的 SF12 包均会超过现有发送期限。
[RadioLib SF 范围](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x_config.cpp) ·
[RadioLib LDRO 选择](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x_commands.cpp)

## 自适应与恢复

下图说明三种构建时可选的规则及其共用的 SF 调整流程。

<img src="../research/figures/adaptive-algorithm-adr.png" alt="Gateway SF 策略，包括会话恢复、近期丢包窗口、降档失败回退和确认。" width="480">

[算法 1（网关调度）](../../README.zh-CN.md#algorithm-1-gateway-scheduling)
说明网关如何选择并服务分组。[算法 2（客户端交付）](../../README.zh-CN.md#algorithm-2-client-delivery)
展示客户端如何按匹配的 Grant 和反馈发送数据。两份算法中，$\phi$ 为帧 ID，
$\phi_{\mathrm{ack}}$ 为同一网关会话中最后收到 ACK 的帧，$\Gamma$ 为有效
Grant。$\Omega$ 为当前名单，$\nu_\iota$ 为客户端 $\iota$ 已获调度机会的计数
（固定名单时为帧 ID），$\iota_0$ 为名单中的首个位置。$\kappa_\iota$ 为 v7
Request 挑战值。第 $\jmath=0$ 轮为首传。
$\Theta_\jmath$ 是本轮有权发送的 Client 集合，$\Xi$ 累计记录已接受的 Client，
$\Pi_\jmath$ 从已获授权但 DATA 仍未到达的 Client 中选出最多 $\varkappa$ 个。
$\epsilon_\jmath$ 表示本地 Client 有权发送。位图缺失或无效时，两集合为空。
只有已获授权但 DATA 未到达时才安排重试。应用拒收的报文不进入重试集合。
网关接收 DATA 时检查帧、会话、载荷长度和时序，v7 还验证 CCM 标签。

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
| $\nu_{\mathrm{ack}}$、$\nu_{\mathrm{hold}}$ | 试用期限和恢复失败后的冷却帧数，以及降档等待期要求的首传成功次数 | 6、8 |

$\epsilon_\iota^{\mathrm{fail}}$ 表示首传失败，即使重传后来成功也计入。
$\alpha_\iota$ 表示最新 Request 回报了已记录交付的帧，并确认对应 SF。
$\nu_\iota^{\mathrm{first}}$ 为连续首传成功次数，包含当前有效记录。
$\phi_\iota^{\mathrm{req}}$ 和 $\phi_\iota^{\mathrm{del}}$ 是上次 Request 和交付帧，
$\sigma_\iota^{\mathrm{del}}$ 是该次交付所用 SF。
$\sigma_\iota^{\mathrm{want}}$、$\sigma_\iota^{\mathrm{res}}$、
$\sigma_\iota^{\mathrm{mem}}$ 和 $\sigma_\iota^{\mathrm{back}}$ 依次表示目标、预留、记忆及回退 SF。
$h_\iota$ 是回退后剩余等待次数，$\theta_{\sigma_\iota-1}$ 是交付窗口规则试用低一级 SF 的 SNR 门限。

设 $r_\sigma=x_{\mathrm{req}}+b_\sigma-\vartheta_\sigma$ 为本次 Request
换算到已分配 DATA SF $\sigma$ 的 SNR 余量。同 SF 重传成功时，或完全未收到
DATA 且有效的 Request 余量满足 $r_\sigma<M_{\mathrm{up}}$ 时，丢包指示 $e$ 为 1，
仅有首传窗观测时 $e$ 为 0。三种规则均在已分配 SF 上维护独立的窗口
$\boldsymbol{\chi}_\iota$。伪代码中 $q_\iota$ 表示首传接收窗有效，
$m_\iota=1$ 表示此时也没有收到 DATA。符合条件的 DATA 交付记零。缺席、应用拒收、首传窗未观测或存在本地故障的记录
不进入该窗口。

```math
\begin{aligned}
\boldsymbol{\xi}^{\mathrm{loss}}_\iota
  &\gets \mathrm{Tail}_{\nu_{\mathrm{obs}}}
  \bigl(\boldsymbol{\xi}^{\mathrm{loss}}_\iota\mathbin{\|}e\bigr),
  & e=1 &\;\land\; \sum_j\xi^{\mathrm{loss}}_{\iota,j}\ge\nu_{\mathrm{up}},\\
\boldsymbol{\chi}_\iota
  &\gets \mathrm{Tail}_{\nu_{\mathrm{obs}}}
  \bigl(\boldsymbol{\chi}_\iota\mathbin{\|}m_\iota\bigr),
  & m_\iota=1 &\;\land\; \sum_j\chi_{\iota,j}\ge\nu_{\mathrm{up}}.
\end{aligned}
```

第一行给出常规升档规则。常规规则保持原 SF 时，第二行也可申请升档。两种触发条件都进入
同一个限时试用流程。下一次 Grant 核对完整名单的空口预算。已获授权的候选 SF 若首传丢失
且最终仍未收到 DATA，则再升一级。有效 Request 的回执若匹配试用开始后在候选 SF 接收的 DATA，
处理 Request 时即确认试用，后续帧时序故障不影响该确认。预算不足、最高 SF 仍全丢或回执超时均会回退并释放额外预留。
当 $\phi-\phi_{\mathrm{failed}}\ge\nu_{\mathrm{ack}}$ 时可再次尝试。周期为 15 秒时，连续
两次有效丢失对应约 30 秒后首次获得高一级 SF 的机会。窗口中间若有一次成功则再等一帧。
即使后来没有有效记录，回执期限仍有效。

连续三次首传成功且有同一 SF 的 ACK 回执后锁定，预算受限而分配较低 SF 时也适用。
锁定后记录已确认的 SF，并释放为回退保留的较高额度。锁定且等待期结束后，若连续六次首传成功且平滑 SNR
达到要求，可试用低一级（SF7 至少 −3.5 dB，SF8 至少 −6 dB，SF9 至少 −8.5 dB）。
试用期间首传失败即回退。若试用始于帧 $\phi_{\mathrm{probe}}$，在
$\phi-\phi_{\mathrm{probe}}\geq\nu_{\mathrm{ack}}$ 的有效记录处理中仍无匹配回执，也会回退。
即使期间记录缺失或被排除，未确认的试用也会在下一帧开始时结束。周期为 15 秒时，
六个帧编号对应 90 秒。回退后需八次首传成功才再次考虑降档。

Client 只有在收到有效位图授权时才重传。Gateway 只有在下一轮开始前及时处理位图发送完成事件，
才打开重传时隙。保留四帧累计 ACK，允许补确认。Gateway 分别核对每个 Client 的首传窗口
是否已按授权 SF 开启 RX，以及窗口内是否发生本地无线故障。ACK 或重传时序异常，或其他
节点的接收故障，仍保留该节点已观测的首传丢失，用于限时搜索。时序异常仍不参与常规余量和稳定性判断。
Beacon 过期后 Client 停止按时隙发射，并返回控制 SF 搜索。准备 Grant 时距某个 Client
上次 Request 达四个帧编号，Gateway 释放其较高 SF 的预留。相同 boot ID 的 Client 返回时，
Gateway 按当前预算重新申请由 ACK 回执或后续锁定记住的 SF。如果最近收到的 DATA 使用了更高
SF，但仍没有 ACK 回执，也可按相同期限和预算试用该 SF。预算不足时可能分配较低 SF。
降档试用锁定前，保留此前的恢复 SF。Client boot ID 改变或 Gateway 启动新会话时清除该记录。
同步无线调用仍可能
超出软件时序窗口。[LaTeX 源文件](../research/figures/adaptive-algorithm.tex) · [运行时代码](../../src/app/multiclient_link.cpp) · [已知时序风险](../en/ERROR_LOG.md)

<a name="loss-cause-aware-adr"></a>

### 按丢包情况调整

#### 卡尔曼估计器

[代码](../../src/app/adr_policy.cpp)跟踪 SNR 电平 $s$（dB）及其变化趋势 $\dot{s}$（dB／有效报告）。观测 $y_k$ 优先使用 SF9 Request 的 SNR。只有已接收 DATA 的 SNR 时，减去各 SF 暂定偏移 $b_\sigma$，统一到 Request 的观测尺度。

```math
\begin{aligned}
\boldsymbol{x}_k &= [s_k,\dot{s}_k]^{\mathsf T},
& F&=\begin{bmatrix}1&1\\0&1\end{bmatrix},
& H&=\begin{bmatrix}1&0\end{bmatrix},\\
Q&=\begin{bmatrix}q_s&0\\0&q_{\dot{s}}\end{bmatrix},
& R&=r,\\
\boldsymbol{x}_k^- &= F\boldsymbol{x}_{k-1},
& P_k^-&=FP_{k-1}F^{\mathsf T}+Q,\\
e_k&=y_k-H\boldsymbol{x}_k^-,
& \omega_k&=HP_k^-H^{\mathsf T}+R,\\
K_k&=P_k^-H^{\mathsf T}/\omega_k,
& \boldsymbol{x}_k&=\boldsymbol{x}_k^-+K_k e_k,\\
P_k&=(I-K_kH)P_k^-,
& \widehat{x}&=HF\boldsymbol{x}_k,\\
\sigma_x^2&=H(FP_kF^{\mathsf T}+Q)H^{\mathsf T},
& m_\sigma&=\widehat{x}+b_\sigma-\vartheta_\sigma-z\sigma_x .
\end{aligned}
```

$P$ 为状态协方差，$Q$ 为过程噪声，$R$ 为测量方差，$K_k$ 对新息 $e_k$ 加权。首个有限观测初始化电平、零趋势和对角协方差 $(r,p_{\dot s,0})$。某次有效报告没有有限的 SNR 时，采用先验 $\boldsymbol{x}_k^-,P_k^-$，跳过测量校正。缺席和排除的报告不推进滤波。实现将趋势方差与开方前的预测方差下限设为零。目前没有专门的异常值剔除。

轮换配置也按有效报告预测下一步。活动组以外的节点不消耗滤波步数。重启或空闲重置会丢弃旧估计。参数尚未按实际秒数校准。若改用实际时间，需把 $F_{12}=\Delta t$ 改为秒，配套推导 $Q(\Delta t)$ 并用采集数据重新校准。裕量只用于可选的 ADR 策略，切换仍需通过时隙预算、试探和回执检查。默认继续使用交付窗口策略。

`MARGIN_EWMA` 跟踪指数加权均值与方差。有效报告缺少有限 SNR 时保留估计值。

RSSI 描述接收包的强度。RSSI 低但交付正常时可继续使用当前 SF；RSSI 高也可能同时出现
低 SNR 和丢包。固件记录 RSSI、SNR 与交付结果供诊断，单个 RSSI 读数无法触发试用。

余量估计优先使用已验证的 SF9 Request SNR，缺失时使用已接受的 DATA SNR。
[AdrParams](../../src/app/adr_policy.h) 中每个 SF 的偏移量定义为 DATA SNR 减去 Request SNR，
初值为零。估计器使用统一的 Request SNR 尺度，切换 SF 时保留状态，更换会话或离线状态到期后重新估计。
同时保存两种 SNR 以便后续标定偏移。

| 参数 | 初始实验设置 |
|---|---|
| SF7/8/9/10 的暂定 SNR 门限 $\vartheta_\sigma$ | −7.5 / −10 / −12.5 / −15 dB。后续用各 SF 实测的 50% 交付门限替换。 |
| 余量 $m_\sigma=\widehat{x}+b_\sigma-\vartheta_\sigma-z\sigma_x$ | $\widehat{x}$ 为预测 SNR，$b_\sigma$ 为偏移，$\sigma_x$ 为预测标准差，$z=1$ |
| 丢包分数 $S\gets\rho S+w$ | $\rho=0.75$。符合条件的全丢加 1.0，同 SF 重传成功加 0.4，$e=0$ 时加 0。 |
| 根据余量提高 DATA SF | 要求 $e=1$、$S\geq S_{\mathrm{up}}=1.5$，且 $m_\sigma<M_{\mathrm{up}}=0$ dB。限时搜索使用独立的丢失窗口。 |
| 试用较低 DATA SF | 延续锁定和回执规则，连续六次首传成功且 $m_{\sigma-1}\geq4$ dB。保留回退、等待和回执超时处理。 |
| 估计器 | EWMA 权重为 0.25。Kalman 的水平和趋势过程方差为 0.16 和 0.01，测量方差为 1.0，初始趋势方差为 0.25。每条有效记录为一个时间步。 |

`V3_LINK_SAMPLE` 保留旧采集字段，并在自适应 Gateway 上增加 `node_id`、`class`、`snr_req`、
`snr_data`、`level`、`slope`、`sigma`、`margin`、`score`、`decision`、`rule` 和
`params_version`。余量对应已分配的 SF，升档分数在清空前记录，缺失值为 `N/A`。
`LOST_ALL` 表示已分配机会均未成功交付。`params_version=5` 下，`PRIMARY_MISS`
表示首传窗已观测且未收到 DATA，但整体时序无法支持全丢分类。`adr=loss_unconfirmed`
表示没有收到 DATA，且 Request 余量未满足条件。`step_up` 和 `recovery_probe` 开始试用，
`recovery_next` 请求下一候选 SF。`recovery_wait_ack` 表示已收到 DATA 并等待匹配回执，
`recovery_confirmed` 表示回执验证成功。`recovery_budget` 与 `recovery_failed` 表示回退，
`recovery_cooldown` 表示失败后的等待期。`ADR_PROBE` 的 `result=expired`
表示试用在两次评估之间达到帧编号期限。`ABSENT` 表示在已监听的请求阶段未收到有效 Request。
`EXCLUDED` 对应本地时序问题或应用拒收。这两类均不更新估计器或分数。
另外两类为 `OK_FIRST` 和 `RESCUED_SAME_SF`。离线解析器兼容旧日志，并跳过时序无效、应用拒收和缺席记录。

所有升档仍需通过完整名单的空口预算检查。即使 Gateway 收到了 Request，Grant 丢失也会造成
所有 DATA 缺席。低余量可以支持升档判断，但不足以确定丢失原因。归因前应对照 Client 的发射日志。

从零开始，连续 $n$ 次相同权重的分数为 $S_n=w(1-\rho^n)/(1-\rho)$。按现有设置，
在余量条件也满足时，两次符合条件的全丢或连续十次重传成功即可达到 $S_{\mathrm{up}}$。
编译时检查 $S_{\mathrm{up}}(1-\rho)<w_{\mathrm{loss}}$，确保分数可以达到门限。
余量与权重仍须现场标定。[定种子模拟器](../../tools/adr_sim.py) 的 `--policy-snr-shift-db`
可单独调整策略门限，同时保持假定信道不变。

<a name="rotating-schedule"></a>

## 轮换调度

空口 v6 是可选的明文研究配置。它保留 28 字节帧头、96 字节帧长上限、SF9 控制与逐客户端的 DATA SF7–SF10。配置 `ADAPTIVE_LORA_REGISTERED_CLIENTS` 后，登记表依次分配可用 ID，跳过网关 ID。网关为 1 且登记上限为 128 时，客户端 ID 为 2–129。每帧只广播当前名单，为选中的每个客户端分别安排 Request 和 DATA。网络 ID、网关启动 ID 和帧 ID 联合标识当前组。接收新的信标后，旧授权失效。

[选择器](../../src/common/protocol/multiclient_protocol.cpp)取能满足时限的最长连续名单，按载荷上限和期望、预留两者中更高的 DATA SF 计算。预算包含信标、Request、Grant、首轮 DATA、最长的合格重试时隙、反馈和保护时间。放不下的节点排在下一组首位。每轮到达表尾后从头开始。组外客户端保留待发送数据。ADR 状态按客户端 ID 索引，空闲与试探期限按该客户端自己的服务机会计算。网关重启会清空这些易失状态。

参数固定且每组容量为 $\nu_g$ 时，$\tau_{\mathrm{cycle}}=\lceil\nu/\nu_g\rceil\tau_{\mathrm{frame}}$。下表按 32 字节上限与 15 秒帧计算一轮的服务机会，复用固件的空口计算。SF 变化可能改变节点在下轮的位置。漏收信标、丢包和停机会拉长成功交付间隔。

| 登记节点数 | 全部 SF7 | 全部 SF8 | 全部 SF9 | 全部 SF10 |
|---:|---:|---:|---:|---:|
| 70 | 135 秒 | 135 秒 | 150 秒 | 360 秒 |
| 84 | 165 秒 | 165 秒 | 180 秒 | 420 秒 |
| 128 | 240 秒 | 240 秒 | 285 秒 | 645 秒 |

### 早期认证开销估算

安全版本需要在使用 Beacon、Grant、ACK、Request 和 DATA 前先认证。共享组密钥会让被攻破的客户端冒充网关。对比两种方案：每条控制帧附 64 字节网关签名，或为名单中每个节点附 16 字节独立认证标签。签名也可由组外客户端验证。分组少于四个节点时，独立标签更短，但验证方需要知道对应的标签位置。

主机模型给每个 Request 和 DATA 增加 16 字节，再给信标、授权与各轮预留位图增加签名或标签开销。保护时间、重试和帧尾保持一致。32 字节、DATA SF7/8/9/10 时，签名方案每帧可容纳 8/7/4/2 个客户端，独立标签为 8/6/4/2。128 节点时，两者在全 SF7 下均计算为 240 秒，全 SF10 为 960 秒。这些是空口时间推算。额外会话流量、CPU 时间、密钥管理、实际调度间隔和射频丢包仍需测试。

早期估算没有计入新鲜度确认和额外处理保护时间。[v7 实现](#secure-wire-v7)采用签名和挑战回显。v6 对照仍为明文。

<a name="secure-wire-v7"></a>

## 安全空口 v7

两种角色都使用 `heltec_wifi_lora_32_V3_secure`，仅接收 v7。保留 v1、v2、v3、v5 和 v6 作为对照。安全配置的射频缓冲区上限为 237 B，内层仍使用 28 B 帧头和 96 B 总长限制。内层长度字段只计入安全尾部之前的报文体。

| 报文 | 完整空口字节数 | 保护方式 |
|---|---:|---|
| Beacon | $113+3n$ | P-256 签名和 128 位网关启动纪元 |
| Request | 69 | 128 位客户端挑战值和截断为 128 位的 HMAC-SHA-256 |
| Grant | $93+18n$ | P-256 签名，每个名单位置包含 96 位挑战回显 |
| DATA | $48+\lambda$ | AES-128-CCM 和 128 位标签 |
| ACK 位图 | $95+4n$ | P-256 签名 |

$n$ 为名单节点数，$\lambda$ 为应用载荷字节数。$n=8$ 时 Grant 的编码上限为 237 B。32 B 配置按空口预算选取更小的分组。ACK 表示接收回调接受报文，数据库持久化需要独立确认。

网关在记录 Request 前验证逐客户端 MAC。客户端用预置网关公钥验证控制帧。签名覆盖可见路由帧头、类型、调度、帧编号、发送方启动 ID 与载荷，并绑定凭据代次和完整网关纪元。被攻破的客户端可以冒充自身，但无法签署网关报文或冒充其他客户端。

客户端验签后将新纪元的 Beacon 作为待确认调度。每次发送 Request 前生成 128 位挑战值。只有及时收到匹配帧、客户端启动 ID、请求载荷大小并回显 96 位挑战的有效 Grant 才确认新纪元并发送 DATA，此前会保留已确认的交付状态。失去同步后，录制的旧 Beacon 可能引发 Request，录制的 Grant 缺少本次挑战回显。客户端开始发送 DATA 后才检查当前会话、帧和轮次的反馈。同一事务中的重复认证 DATA 最多调用一次应用回调。

启用无线前，用 ESP32-S3 启动熵源为 Mbed TLS CTR-DRBG 播种。网关每次启动或帧计数回绕时生成新的 128 位纪元。HKDF-SHA-256 用凭据代次派生请求 MAC 密钥，用凭据代次、网关纪元和完整挑战值派生 DATA 密钥，两种用途的标签不同。DATA 的内层帧头作为认证附加数据。13 字节 nonce 使用帧头第 12–23 字节及 v7 域字节。新帧改变 nonce，新握手在熵源和 HKDF 正常的条件下产生新密钥。重传复用缓存的密文和标签，报文收发无需写入闪存更新计数器。

管理员离线导入凭据。运行时不写入或修复持久凭据。安全角色和密钥使用独立于明文角色的命名空间。记录缺失、读取故障、配置不一致、熵源失败或 NVS 被格式化时，射频保持关闭。恢复有效旧凭据后通过新握手更新会话密钥，但完整闪存回退可能恢复已撤销的信任材料，因此恢复后应给所有活动板重新分配凭据及代次。Secure Boot、闪存／NVS 加密及回退控制仍需单独验收。射频干扰、流量元数据和网关被攻破属于剩余风险。

<a name="secure-schedule-capacity"></a>

### 安全调度容量

调度计入完整认证帧、四个反馈时段和 300 毫秒帧尾预留。时隙保护为 160 毫秒，在原 80 毫秒上暂加两侧各 40 毫秒的密码处理预算。编码后超过时隙起点 60 毫秒便取消发送。安全适配器直接使用带硬件超时的 SX1262 SetTx，跳过 RadioLib 独立的无界 PA 等待。SPI 锁和命令耗时仍需实测。

| 登记节点数 | 全部 SF7 | 全部 SF8 | 全部 SF9 | 全部 SF10 | 依次混合 SF7–SF10 |
|---:|---:|---:|---:|---:|---:|
| 1 | 15 秒 | 15 秒 | 15 秒 | 15 秒 | 15 秒 |
| 4 | 15 秒 | 15 秒 | 30 秒 | 30 秒 | 30 秒 |
| 8 | 30 秒 | 30 秒 | 45 秒 | 60 秒 | 45 秒 |
| 16 | 60 秒 | 60 秒 | 90 秒 | 120 秒 | 90 秒 |
| 32 | 105 秒 | 120 秒 | 165 秒 | 240 秒 | 180 秒 |
| 63 | 195 秒 | 240 秒 | 315 秒 | 480 秒 | 360 秒 |
| 64 | 195 秒 | 240 秒 | 330 秒 | 480 秒 | 360 秒 |
| 70 | 210 秒 | 270 秒 | 360 秒 | 525 秒 | 390 秒 |
| 84 | 255 秒 | 315 秒 | 420 秒 | 630 秒 | 480 秒 |
| 96 | 300 秒 | 360 秒 | 480 秒 | 720 秒 | 540 秒 |
| 128 | 390 秒 | 480 秒 | 645 秒 | 960 秒 | 720 秒 |

计算采用 32 B 载荷与连续调度。成功交付还需双向 SF9 控制以及成功的 DATA 时隙。SF10 DATA 无法扩大 SF9 控制链路的覆盖。[模型结果](PROGRESS.md#secure-rotation-experiment)给出共享干扰下的交付间隔，现场测试要求见[安全配置测试](BENCH_RUNBOOK.md#test-the-secure-candidate)。

<a name="integration-prompt"></a>

## 集成提示词

先核对本地代码和目标系统的许可，再使用以下提示词：

> 阅读本仓库的工程契约、协议和设置指南。审查目标固件，找出样本来源与接收数据的消费点。列出客户端数量、载荷上限、上报间隔、可接受交付间隔和获准使用的射频配置，对照调度与认证状态验证。提出带版本号的二进制载荷，明确长度、字节序、单位、时间戳和稳定样本标识。
>
> 应用依赖留在协议之外。用 `FirmwareRuntime::submitPayload` 提交数据并处理 `QUEUED`、`BUSY`、`TOO_LARGE` 和 `INACTIVE`。单份待发样本跨组保留。同步 `PayloadPort::Receiver` 返回前复制所需字节，并返回应用是否接收。ACK 表示回调已接收报文。下游持久化若需要保证，另行定义幂等确认。同时考虑重启、交付未知、队列溢出和版本不匹配。添加字节级样例及重试、丢失的主机测试，保留对照固件，避免在日志或 Git 写入密钥与私有数据，分别报告编译、仿真和实测结果。

**采样与持久化。** 按计划时间采集，在提交 LoRa 前分配稳定样本 ID。应用重试或重启后继续使用原始时间戳和 ID。传输层仅持有一份待发载荷副本。要求在长时间中断中保留测量时，由消费应用实现有界持久队列。采样任务通过同步队列把不可变数据交给协作式运行循环。收到 `BUSY` 时保留样本，并报告队列溢出、最老样本等待时间和丢失数量。在 `MISSING`、`UNKNOWN` 或重启后，用同一应用样本 ID 重试。网关消费端按已认证的客户端 ID 和样本 ID 跨会话去重。接收回调必须满足射频时序。下游持久化使用独立的应用确认。

## 板卡参考

Heltec V3.4 使用 ESP32-S3、SX1262 和 CP2102 UART0，以 V3.2 原理图为电气核对参考。
GPIO 8–14 依次为 NSS、SCK、MOSI、MISO、复位、BUSY 和 DIO1。DIO2 控制射频开关，
DIO3 给 1.8 V TCXO 供电。显示连接 SDA 17、SCL 18、复位 21 和低电平启用的 Vext 36。
研究频率暂定 915.0 MHz，固件请求 22 dBm 及 140 mA PA 过流保护，走测前核实实板性能和射频版本。
[引脚定义](../../include/board_pins.h) ·
[Heltec 原理图](https://s.heltec.cn/download/WiFi_LoRa_32_V3/WiFi_LoRa_32_V3.2_Schematic_Diagram.pdf)
