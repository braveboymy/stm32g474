# 平台架构（M1 基线 + 物联网演进设计）

电池供电的电机控制平台，演进目标：具备物联网能力的成熟嵌入式产品（可验证/可演进/可运维/可量产）。
领域词表见 `CONTEXT.md`，重决策见 `docs/adr/`。

## 1. 设计决策（已确认）

| 决策点 | 结论 | 出处 |
|---|---|---|
| 内核 | FreeRTOS（V11.1.0）；控制环等硬实时逻辑放中断裸跑 | M1 |
| 业务方向 | 电机控制（FOC 类），信号链能力优先；**电池供电** | M1 + Q3 |
| 能量预算 | 待机 ≤20µA（STOP2 + RTC），续航按月计，连接 ≤1 次/天；一切功能在此预算内立项 | Q8 |
| 设备行为 | 被动服务（被连接）+ **主动上报**（定时按上报调度，紧急事件可预算外立即上报） | Q18/Q26 |
| 连接通道 | 多通道可插拔；首条 **NB-IoT**，后续 CAT1 / BLE / 红外 | Q2/Q13/Q21 |
| OTA | 硬需求 + 回滚；**外挂 SPI Flash 暂存 + 单 bank 校验**（升级窗口停机） | Q4/Q7，ADR-0001 |
| 协议层 | **协议会换、组帧与交互不保证一致**；协议适配器可插拔，框架只做路由 | Q23/Q27 |
| 加密 | 小型手写实现：AES-128-ECB / SHA-256 / HMAC-SHA256（G474 无硬件加速）；RNG 用片内外设 | Q24，ADR-0002 |
| 安全边界 | 安全关键路径独立于 RTOS 与连接层；不做 SIL 流程，边界按可认证预留 | Q5/Q9 |
| 时间 | RTC 日历时间 + 协议注册帧对时（秒级，LSE 日漂移 ~1–2s） | Q11 |
| 数据三分离 | 运行日志（仅 DEBUG）· 事件账本（持久环形，撕裂容忍）· 历史记录（日期分片，掉电补全） | Q12/Q19/Q25 |
| 升级分工 | app 侧下载/校验/标记；bootloader 侧复制/回滚/启动校验 | Q16 |
| 分层机制 | core 硬件无关性经 **port 接口**（osal_/stor_/time_/rng_）保持；app 内部子层 business→proto→conn | 本轮 |
| 传输层 | bsp/uart 实例化字节管道 + at_ AT 引擎（共享）+ 适配器；换 MCU 只重写 bsp；at_ 只交原始响应行，解析归适配器（D3） | 本轮 |
| 通信管理 | 机制/策略分离：适配器执行机制（异常停 FAULT 等指令），business/comm 持有策略（上电/重试/休眠/档位）；细粒度状态+原因码对业务可见 | 本轮 |
| 定时器 | osal 抽象软件定时器（timer-to-queue）；日历唤醒归 power（RTC alarm）；中断级裸跑 | 本轮 |

## 2. 分层与模块图

依赖方向：`app → core → bsp → hal`，禁止反向（原有）。core 的硬件无关性通过 **port 接口**（2.1）保持。

```
app/        （业务层；内部子层：business → proto → conn）
├─ business/  业务任务 + app_fsm（运行/待机/充电/OTA/故障 编排）
│   └─ comm 通信管理（策略所有者：上电/复位/重试/休眠/档位决策，见 2.4）
├─ proto/     协议层（硬件无关，可单测）
│   ├─ proto_fw   协议框架（薄层）：协议模块注册、字节路由（通道-协议绑定表）、事件分发
│   ├─ proto_nbiot 协议适配器：组帧+CRC + 会话(注册/上报/结束帧) + DID 映射（参考 docs/protocol/）
│   └─ proto_ir / proto_ble 协议适配器（组帧/交互独立定义，可插拔）
├─ conn/      连接抽象层（共享积木 + 薄适配器，见 2.2）
│   ├─ at_  AT 会话引擎（共享，纯逻辑可单测）：命令行/结果码/URC/超时
│   ├─ conn_nbiot · conn_cat1 · conn_ble（薄：AT 命令集 + 通道语义 → 设备模型）
│   └─ conn_ir（薄：裸字节管道 + 红外线编码）
├─ dev/       设备模型：状态/配置/历史/事件的稳定访问接口（驱动无关，业务与协议共享）
└─ update/    OTA 管理：下载到 SPI Flash → 校验 → 标记 → 复位（M6）

core/       （平台核心：硬件无关，PC 可单测；只依赖 port 接口，不碰 bsp/HAL）
├─ osal/   任务/延时/互斥/临界区/tick/堆水位/定时器（A 类软件定时器，见 3.8）
├─ sec/    AES-128-ECB / SHA-256 / HMAC-SHA256 算法 + 会话密钥派生（ADR-0002）
├─ log（运行日志，DEBUG-only）· fault（崩溃报告，M2）· util/rb
├─ event 事件账本：SPI Flash 持久环形（魔数+CRC+序列号，撕裂容忍，启动扫描恢复）
├─ hist 历史记录：SPI Flash 日期分片，主站按范围查询，掉电按上电后数据补全
├─ config 参数/NVM：参数区 EEPROM 仿真（双备份+撕裂容忍）+ 升级状态 + 通道-协议绑定表 + 上报调度 + 密钥存储
├─ time 时间服务：UTC 换算/对时逻辑/时间戳（经 time_ port）
└─ port/   接口声明（纯接口，实现放 bsp）：osal_ · stor_ · time_ · rng_

bsp/        （板级：port 实现 + 外设驱动）
├─ port 实现：stor_spi_flash（SPI NOR）· stor_params（片内参数区）· rtc（LSE）· rng（片内 RNG）
├─ power 电源管理：STOP2/RTC 唤醒调度、能量预算执行、掉电处理序列、充电管理、电量估计
├─ protection 保护：电机/欠压/过温/堵转 + 硬件刹车（安全边界内，不依赖 RTOS 任务）
├─ uart（实例化字节管道：多实例、DMA、空闲超时切帧）· spi_flash · iwdg · led · clock
└─ 芯片级：startup / system / linker

bootloader/ （M6：SPI Flash 复制/回滚/启动校验；分区见 docs/flash-partition.md）
```

### 2.1 port 接口（core 硬件无关性的实现机制）

core 模块只依赖以下接口声明；实现全部在 bsp，换平台只重写实现（2.3），PC 单测用 stub。

| port | 接口（示意） | 实现（bsp） | 消费方 |
|---|---|---|---|
| osal_ | 任务/延时/互斥/临界区/tick/堆水位/定时器 | FreeRTOS（osal_freertos.c） | 全部 |
| stor_ | open/read/write/erase | stor_spi_flash · stor_params | event / hist / config / update |
| time_ | rtc_read / rtc_set / alarm | rtc（LSE 日历 + alarm） | core/time · bsp/power |
| rng_ | rand_bytes | rng（片内 RNG） | core/sec |

### 2.2 conn_ 三层结构（共享积木 + 适配器）

- **字节管道**：`bsp/uart` 实例化（多实例，每通道一个），DMA + 空闲超时切帧——NB-IoT / CAT1 / BLE-AT / 红外共用
- **AT 引擎**：`at_` 命令行 / 结果码（OK/ERROR/CME）/ URC 分发 / 超时 / echo，纯状态机，零 HAL 依赖，可 PC 单测——三个 AT 型通道共用；内置看门狗（定时 AT ping + 适配器注册的 reset 钩子，连续失败计数，D1）
- **适配器**（每通道一个，机制实现者）：命令集表 + 模组驱动流程（boot/attach/socket/休眠状态机）+ 细粒度状态与原因码上报 + 控制接口实现 + `link_cost()`（唤醒→就绪时间×电流，供 power 调度，D2）；红外不用 at_，裸管道 + 线编码
- 纪律：**conn_ 只依赖 bsp/uart API，任何适配器不碰 HAL**；**适配器不持有策略**（见 2.4）

**at_ 引擎边界（D3）**：只懂「发一行、收多行、判最终码、分 URC」，**只交原始响应行，解析归适配器**（`+CSQ: 15,99` 是 NB 的事，`+LINK: 1` 是 BLE 的事）；支持透明模式 **pause/resume**（BLE 连接后进透传、AT 挂起，`+++` 退出）；线参数可配（终结符/大小写/流控/波特率/命令间隙）；不强制 URC——适配器可轮询（便宜 BLE 模组无 URC）。

```c
at_handle_t at_open(const at_cfg_t *cfg);   // 终结符/大小写/流控/波特率/命令间隙
int  at_cmd(at_handle_t h, const char *line, at_resp_t *resp, uint32_t timeout_ms);
     // resp = 原始响应行列表 + 最终结果码（OK/ERROR/CME: n）
void at_register_urc(at_handle_t h, const char *prefix, at_urc_cb_t cb);
void at_pause(at_handle_t h);   // 透明模式：挂起队列，字节直通
void at_resume(at_handle_t h);
int  at_ping(at_handle_t h);    // 看门狗原语
```

### 2.3 换平台矩阵（两条正交轴）

| 变更 | 重写范围 | 零改动 |
|---|---|---|
| 换 MCU（G474 → 其他芯片） | bsp/：uart、stor_/time_/rng_ 实现、时钟/启动 | conn_ / at_ / proto_ / dev / core |
| 换模组（BC26 → M5311；AT 型 BLE → HCI 型） | 对应薄适配器的命令集/线协议 | 框架、其他通道、业务 |

前提三条纪律：① conn_ 只依赖 bsp/uart API；② at_ 零 HAL 依赖；③ proto_ 不碰传输细节（只见帧）。

### 2.4 通信管理：机制与策略分离（状态对业务可见）

业务**必须**感知链路状态并**控制**模组流程——模块不是自己玩自己的。拆两层：

- **适配器 = 机制**：boot 序列、命令收发、URC → 状态翻译、机械步骤超时。异常时**停在 FAULT 状态等指令，不自主重试**（重试消耗能量，是策略不是机制）
- **business/comm 通信管理 = 策略**：订阅细粒度状态与原因码；决定上电/断电时机（上报调度/紧急事件/OTA）、失败后的重试/复位/放弃（受能量预算约束）、休眠与档位切换；通信事件写事件账本

**状态两级**：
- 细粒度 + 原因码（适配器 → comm）：OFF / POWERING_ON / INIT / ATTACHING / ONLINE / SLEEP / FAULT(原因=REG_TIMEOUT|SIM_ERROR|NO_SERVICE|SIGNAL_WEAK…)——卡在哪一步，业务看得见
- 粗粒度投影（派生，被动消费者）：可达性 + 数据通道就绪——proto_fw 据此决定能否发帧，time 据此知道对时窗口

**控制接口**（comm → 适配器）：power_on/off · reset · attach/detach · enter_sleep/wake · set_profile（能量档位）· link_cost

**失败处理闭环**（例：附网失败）：适配器上报 FAULT(REG_TIMEOUT) → comm 记事件账本 → 按策略：余量足 → 断电复位重试（限次+退避）；电量低 → 放弃本次随下次调度再试；连续失败 → 降档 + 事件升级。

## 3. 核心机制

### 3.1 能量模型
- 待机：STOP2 + RTC 唤醒，外设按档位供电（SPI Flash/UART/LED 断电），仅留 RTC/IWDG/唤醒 GPIO
- BLE 待机：低频广播（10s 间隔，~5–10µA），广播间隔为能量档位参数，低电量自动降频/全关（Q20）
- 唤醒调度表数据源 = 上报调度（协议 2006H，主站可配，落 config）
- 连接抽象层每个通道携带能量档位（唤醒→工作→休眠代价模型）

### 3.2 掉电处理序列
掉电检测 → 事件账本落盘 → 紧急事件上报 → 关机。备电电容窗口 = 落盘 + 上报共用（Q3/Q12/Q26 交汇）。

### 3.3 上报模型
- 定时上报：按上报调度（默认 1 次/天）
- 事件触发上报分级：**紧急**（掉电/欠压/过温/防拆等安全相关）预算外立即上报；**普通**入事件记录随下次定时上报
- 事件记录格式按协议（9B：事件码+BCD 时间+详情），事件账本为其供数

### 3.4 协议装配
全部协议适配器编译时编入（flash 充足），运行时按 **通道-协议绑定表**（config）路由；换协议现场改配置，不重刷固件。新协议 = 新增适配器文件组，框架/设备模型/传输层零改动（Q28）。

### 3.5 数据三分离
| | 运行日志 | 事件账本 | 历史记录 |
|---|---|---|---|
| 生命周期 | 易失，仅 DEBUG 构建 | 持久环形覆盖 | 持久日期分片 |
| 内容 | 开发调试流 | 离散事件/故障 | 周期采样工况 |
| 掉电语义 | 无所谓 | 撕裂容忍+扫描恢复 | 上电后补全 |

### 3.6 故障分级与看门狗
- IWDG 独立保护（protection 路径）
- 业务任务：RTOS 任务监视喂狗
- 协议/连接栈故障：不复位，记事件账本 + 故障标记（故障域隔离，Q9）

### 3.7 安全
- 软件 AES-128-ECB（PKCS7）/ HMAC-SHA256（详见 ADR-0002）；RNG 片内硬件
- 密钥体系按参考协议：主密钥（默认/客户双态）→ 会话密钥派生；密钥存 config（加密区），2009H 可下发更新

### 3.8 定时器三分（为什么 osal 需要定时器抽象）

| 类别 | 载体 | 颗粒度 | 归属 |
|---|---|---|---|
| A. 软件定时器（周期/单次） | osal 定时器（timer daemon） | ms 级 | core/osal |
| B. 日历定时唤醒 | RTC alarm + STOP2 | 秒级/日历对齐 | bsp/power（上报调度数据源） |
| C. 中断级定时 | 硬件定时器 | µs 级 | 中断裸跑，不进 RTOS |

- osal 定时器 API：周期 = 单次 + auto_reload；**timer-to-queue 模式**（回调只投递事件，工作由业务任务做，避免 daemon 栈溢出）
- 判据：软件定时器在 STOP2 下不跑（CPU 停、tick 不跳），准点唤醒必须走 B 类
- 单次定时器是主力（协议超时/防重窗口/会话空闲），周期定时器用于落盘与上报节拍

## 4. 中断优先级约定（沿用 M1）

| 优先级 | 用途 | 是否可调 RTOS API |
|---|---|---|
| 0~4 | 电机控制环、保护（欠压/过温/堵转）、硬件刹车 | 否 |
| 5 | UART 等普通外设中断 | 可 |
| 6~14 | 预留 | 可 |
| 15 | SysTick/PendSV/TIM6 | 内核管理 |

## 5. 内存与存储布局

- SRAM1 96KB（.data/.bss/堆 48KB/主栈 8KB）；SRAM2 32KB 预留 `.sram2`（电机控制 DMA 缓冲）
- 片内 Flash：boot 32K / app 448K / params 32K（不变，见 flash-partition.md）
- 外挂 SPI NOR（≥1MB）布局：暂存区 448K / 上一版本镜像 448K / 事件账本 / 历史记录 / 元信息

## 6. 被否决方案（防止被改回去）

| 方案 | 否决原因 |
|---|---|
| 片内双 bank A/B OTA | 分区推倒重来、bootloader 重构；收益低（ADR-0001，可复议） |
| 统一帧协议核心 | 组帧/交互不保证一致（Q23 初稿被否决） |
| 协议注册表（通用解析器） | 被真实协议样本推翻：协议族自洽，无需通用抽象 |
| mbedTLS | 空间/演进权衡后选手写（ADR-0002） |
| 全被动设备 | 主动上报是刚需（Q18） |
| BLE 中高频广播 / 常开 UART 日志 | 违反能量预算（Q20/Q12） |
| 事件账本两级保留 | 环形覆盖够用，压缩合并复杂度不值（Q19） |
| 每通道独立串口实现 | 共享字节管道（bsp/uart 实例）+ AT 引擎后，适配器只是薄层；换 MCU 只重写 bsp（本轮） |

## 7. 已知取舍与风险

- **手写加密**：安全敏感，缓解 = 只用协议所需原语（不自创构造）+ NIST 测试向量 + 代码审查 + 模块隔离（可换 mbedTLS，ADR-0002）
- **升级窗口停机**：bootloader 复制期间设备重启（无人值守可接受）
- **协议层「暂时这样定」**：适配器模式已吸收协议不确定性；新协议先写适配器再评估框架是否需要演进
- **停机策略优先于恢复策略**（M1 遗留）：业务故障分级后再调
- UART 逐字节中断发送（115200 ~87µs/字节）：接 AT 通道前升级 DMA（M1 遗留）

## 8. 实施阶段

M 系列排期未重排；新模块与既有里程碑（M2 fault / M3 NVM / M6 OTA）的映射在排期时确认。
