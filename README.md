# STM32-BMS-48Pro

基于 **STM32F103C8T6 + BQ76940** 的 9S 锂电池管理系统（BMS）重构项目。

本项目以真实硬件平台为基础，围绕 BMS 核心功能逐步实现了电池采样、软件告警、硬件保护、自动均衡、执行控制、异常安全处理、FreeRTOS 多任务调度以及 CAN 通讯闭环。

项目当前 **V1 版本已完成**，已实现核心功能联调与 PCAN-View 实物验证。当前阶段主要进入 README、流程图、测试截图、项目复盘与求职展示材料整理。

---

## 1. 项目简介

`STM32-BMS-48Pro` 是一个面向嵌入式学习与求职展示的 BMS 项目。

项目以 **BQ76940 电池监控芯片** 为核心，完成 9 串锂电池的电压、电流、温度采集，并在此基础上实现：

* 软件告警：UV / OV / DIFF / OT / UT
* 硬件保护：OCD / SCD / DSG 阻断
* 自动均衡：多电芯 Balance Mask、非相邻选择、奇偶窗口、读回校验
* 执行控制：BQ76940 FET 控制、BQ76200 执行层状态机、Safe-Off
* 异常处理：BringUp 初始化异常、Runtime 采样异常、HwFault 硬件故障异常
* FreeRTOS 多任务调度：Task / Mutex / Semaphore / Queue
* CAN 通讯：周期上报、RX 命令接收、ACK 响应、PCAN-View 验证

本项目重点不只是“功能能跑”，而是尽量按照实际嵌入式工程思路进行模块划分和架构重构，包括：

* 驱动层与应用层分离
* 采样、保护、均衡、控制、通信任务拆分
* I2C 总线互斥访问
* 全局 BMS 状态互斥保护
* CAN 接收中断与任务解析解耦
* 故障状态下的安全关断与写保护
* 关键状态通过 CAN 和串口进行可视化验证

---

## 2. 硬件平台

| 模块      | 型号 / 说明                     |
| ------- | --------------------------- |
| MCU     | STM32F103C8T6               |
| 电池监控芯片  | BQ76940                     |
| 电池串数    | 当前实物调试为 9 串                 |
| 高边驱动器   | BQ76200，执行层接口与状态机已实现        |
| 电量计     | BQ34Z100-G1，接口预留 / 后续扩展     |
| CAN 收发器 | TJA1050                     |
| 通讯接口    | CAN 500 kbps                |
| 调试接口    | UART 串口、USB-CAN / PCAN-View |
| I2C     | 软件 I2C，PB8 / PB9            |
| LED     | PA15                        |
| BQ 唤醒控制 | PA8                         |

---

## 3. 软件平台

| 类型    | 说明                        |
| ----- | ------------------------- |
| 开发语言  | C                         |
| RTOS  | FreeRTOS                  |
| IDE   | Keil MDK                  |
| MCU 库 | STM32 HAL                 |
| 通讯    | Soft I2C、CAN、UART         |
| 代码管理  | Git / GitHub              |
| 文档记录  | Obsidian、draw.io、Markdown |

---

## 4. 系统功能概览

当前 V1 已完成以下核心模块：

| 功能模块                      | 状态          |
| ------------------------- | ----------- |
| BQ76940 Bring-up          | 已完成         |
| 9 节单体电压采样                 | 已完成         |
| Pack 总压计算                 | 已完成         |
| CC 电流采样                   | 已完成         |
| TS1 温度采样                  | 已完成         |
| UV / OV / DIFF 软件告警       | 已完成         |
| OT / UT 温度保护              | 已完成         |
| OCD / SCD 硬件保护接入          | 已完成         |
| CHG / DSG FET 控制          | 已完成         |
| BQ76200 执行层状态机            | 已完成 V1      |
| 自动均衡 V2                   | 已完成         |
| BringUp 初始化异常处理           | 已完成         |
| RuntimeFault Safe-Off     | 已完成 V1      |
| HwFault 硬件故障处理            | 已完成 V1      |
| FreeRTOS 多任务架构            | 已完成         |
| Mutex / Semaphore / Queue | 已完成         |
| CAN 周期上报                  | 已完成         |
| CAN RX 命令接收               | 已完成         |
| CAN ACK 响应                | 已完成         |
| PCAN-View 实物验证            | 已完成         |
| BQ34Z100 电量计              | 接口预留 / 后续扩展 |
| Qt 上位机                    | 后续扩展        |

---

## 5. BQ76940 Bring-up

已实现 BQ76940 基础初始化流程：

* BQ76940 唤醒
* 基础寄存器读取
* CRC 写寄存器
* ADCGAIN / ADCOFFSET 校准参数读取
* CC_CFG 配置
* SYS_CTRL1 / SYS_CTRL2 配置
* PROTECT1 / PROTECT2 / PROTECT3 配置
* OCD / SCD 硬件保护参数配置
* 初始化阶段自检
* 初始化失败后的 Fail-Safe 处理

Bring-up 阶段不仅完成芯片配置，还会对关键寄存器与采样链路进行检查，避免 BQ76940 初始化失败后系统继续进入正常任务调度。

---

## 6. 电池采样模块

### 6.1 电压采样

当前实物调试为 9 串电池，BQ76940 通道映射如下：

```text
VC1, VC2, VC5, VC6, VC7, VC10, VC11, VC12, VC15
```

已实现：

* 单体电压原始 ADC 读取
* 单体电压 mV 换算
* Pack 总压计算
* 最大单体 / 最小单体统计
* 最大压差计算
* 最大 / 最小单体标签记录
* 采样数据提交到全局 BMS 状态

---

### 6.2 电流采样

已实现 BQ76940 Coulomb Counter 电流采样：

* CC_HI / CC_LO 原始值读取
* 电流 mA 换算
* 充电 / 放电 / 近零方向判断
* Pack 电流状态保存
* CAN 状态帧上报

---

### 6.3 温度采样

已实现 TS1 温度采样：

* TS1 ADC 读取
* 热敏电阻温度换算
* 温度单位采用 0.1°C，即 dC
* 过温 / 低温告警判断
* 温度状态通过串口和 CAN 上报

---

## 7. 软件告警模块

当前实现的软件告警包括：

* UV：单体欠压告警
* OV：单体过压告警
* DIFF：单体压差告警
* OT：过温告警
* UT：低温告警

软件告警支持：

* 进入阈值
* 恢复阈值
* 迟滞判断
* 计数滤波
* 最大 / 最小异常单体记录
* 告警标志位统一打包
* CAN `0x304` 状态帧上报

软件告警与执行控制解耦，告警模块只负责判断状态，实际 FET / 执行层动作由保护和控制任务统一处理。

---

## 8. 硬件保护模块

已接入 BQ76940 硬件保护相关配置与状态读取：

* OV 硬件保护配置
* UV 硬件保护配置
* OCD 过流保护配置
* SCD 短路保护配置
* SYS_STAT 硬件故障位读取
* OCD / SCD 故障识别
* OCD / SCD 故障锁存
* DSG 阻断
* 硬件故障诊断帧上报

当前 HwFault 处理逻辑采用独立任务处理：

```text
BQ76940 ALERT
    ↓
HwFaultTask
    ↓
读取 SYS_STAT
    ↓
判断 OCD / SCD
    ↓
锁存硬件故障
    ↓
阻断 DSG
    ↓
通知 ControlTask 更新执行层状态
```

硬件故障与软件告警分离，便于区分 BQ76940 自动保护事件和应用层软件判断事件。

---

## 9. FET 与执行控制

### 9.1 BQ76940 内部 FET 控制

已实现 BQ76940 内部 FET 控制接口：

* CHG FET 控制
* DSG FET 控制
* CHG / DSG 同时控制
* SYS_CTRL2 读回验证
* 故障状态下关闭 CHG / DSG

---

### 9.2 BQ76200 执行层状态机

当前已实现 BQ76200 执行层接口设计：

* CHG_EN 控制
* DSG_EN 控制
* CP_EN 控制
* PCHG_EN 控制
* GPIO 读回
* 故障状态下 Force-Off

执行层状态包括：

| 状态            | 含义     |
| ------------- | ------ |
| OFF           | 全部关闭   |
| PRECHARGE     | 预充状态   |
| NORMAL_ON     | 正常充放电  |
| CHG_BLOCK     | 禁止充电   |
| DSG_BLOCK     | 禁止放电   |
| CHG_DSG_BLOCK | 充放电均禁止 |

执行层根据 OT / UT / OCD / SCD / RuntimeFault 等状态进行统一决策，避免多个模块直接操作执行 GPIO。

---

## 10. 自动均衡模块

已实现基于 BQ76940 CELLBAL 寄存器的自动均衡控制。

当前均衡策略支持：

* 均衡允许条件判断
* Enter / Exit 阈值迟滞
* 最低单体电压门限判断
* 最大均衡电流限制
* 多电芯 Balance Mask 选择
* 非相邻电芯筛选，避免相邻通道同时均衡
* 基于 FreeRTOS Tick 的周期刷新
* 奇偶窗口分时轮换
* 滞回保持区继续轮换
* CELLBAL1 / CELLBAL2 / CELLBAL3 写入
* CELLBAL 寄存器读回校验
* 新旧 Balance Mask 比较，避免无意义重复写入
* RuntimeFault / HwFault 下禁止旧均衡请求继续写入

均衡任务采用“决策 / 硬件写入 / 状态提交”三段式结构：

```text
BMS_BalanceTask
    ↓
BalanceDecide
    只读取 app 状态，生成 START / STOP / NONE 请求，不访问 I2C
    ↓
BalanceApplyHw
    需要动作时写入 CELLBAL1 / CELLBAL2 / CELLBAL3，并读回校验
    ↓
BalanceCommit
    提交均衡状态、目标电芯、Balance Mask 和 active 状态
```

当前均衡状态通过 CAN `0x306` 帧实时上报，PCAN-View 可观察到：

* 当前是否正在均衡
* 本轮均衡目标数量
* CELLBAL1 / CELLBAL2 / CELLBAL3 mask
* 奇偶窗口 phase
* 当前目标电芯标签

---

## 11. 异常安全处理

项目当前实现三类异常处理流程：

```text
1. BringUp 初始化异常
2. Runtime 运行时采样异常
3. HwFault 硬件保护异常
```

---

### 11.1 BringUp 初始化异常

初始化阶段会检查：

* BQ76940 唤醒
* 基础寄存器读取
* ADC 校准参数读取
* 硬件保护参数加载
* OCD / SCD 配置
* 采样链路自检

异常处理流程：

```text
BQ76940 初始化失败
    ↓
最多重试 3 次
    ↓
最终失败
    ↓
强制 Safe-Off
    ↓
关闭 BQ76200 CHG / DSG / CP / PCHG
    ↓
尝试关闭 BQ76940 FET 与 CELLBAL
    ↓
CAN 发送 0x305 故障帧
    ↓
LED 故障提示
    ↓
进入 STOP 低功耗故障保持态
```

该流程保证系统在 BQ76940 初始化失败时不会继续创建 FreeRTOS 任务，而是优先进入安全关断状态。

---

### 11.2 Runtime 运行时采样异常

运行过程中，如果连续采样失败，会进入 RuntimeDiag 诊断流程。

当前实现：

* 连续采样失败计数
* 总采样失败计数
* 最近故障码记录
* 最近故障阶段记录
* RuntimeFault 锁存
* AFE 写禁止标志
* Safe-Off 请求
* Safe-Off 失败重试
* 故障保持
* CAN 故障诊断帧上报

RuntimeFault 触发后：

```text
SampleTask 发现连续采样失败
    ↓
RuntimeDiag 记录失败
    ↓
首次进入 RuntimeFault
    ↓
设置 AFE 写禁止
    ↓
通知 RuntimeTask
    ↓
RuntimeTask 执行 Safe-Off
    ↓
关闭 BQ76200 执行层
    ↓
关闭 BQ76940 CELLBAL / CHG / DSG
    ↓
提交 Safe-Off 结果
    ↓
通知 ControlTask 保持安全状态
```

AFE 写禁止用于阻止 ProtectTask / BalanceTask 在故障后继续基于旧请求写入 BQ76940。

---

### 11.3 HwFault 硬件故障异常

HwFault 主要处理 BQ76940 硬件保护事件，例如 OCD / SCD。

处理流程：

```text
BQ76940 ALERT
    ↓
HwFaultTask 被唤醒
    ↓
读取 SYS_STAT
    ↓
识别 OCD / SCD
    ↓
生成硬件故障请求
    ↓
尝试补写 DSG OFF
    ↓
提交硬件故障状态
    ↓
通知 ControlTask 更新 BQ76200 执行层
    ↓
通过 CAN 0x305 上报 HwFault 诊断信息
```

该流程强调：硬件保护事件发生后，即使 BQ76940 已经自动关断，软件仍会锁存故障并同步执行层状态。

---

## 12. FreeRTOS 多任务架构

当前系统已迁移到 FreeRTOS 多任务架构。

已实现任务：

| 任务              | 功能                               |
| --------------- | -------------------------------- |
| BMS_SampleTask  | 电压 / 电流 / 温度 / SYS_STAT 采样       |
| BMS_ProtectTask | 软件告警与温度保护处理                      |
| BMS_BalanceTask | 自动均衡决策与 CELLBAL 写入               |
| BMS_ControlTask | BQ76200 执行层状态更新                  |
| BMS_RuntimeTask | RuntimeFault Safe-Off            |
| BMS_HwFaultTask | BQ76940 ALERT / OCD / SCD 硬件故障处理 |
| BMS_CANTask     | CAN 周期上报、RX 命令解析、ACK 响应          |
| BMS_AuxTask     | LED 与运行状态打印                      |
| BMS_GaugeTask   | BQ34Z100 电量计预留任务                 |

核心任务链路：

```text
SampleTask
    ↓
ProtectTask
    ↓
BalanceTask
    ↓
ControlTask
```

异常任务链路：

```text
SampleTask
    ↓
RuntimeTask
    ↓
ControlTask
```

硬件故障链路：

```text
BQ76940 ALERT
    ↓
HwFaultTask
    ↓
ControlTask
```

CAN 任务独立运行：

```text
BMS_CANTask
    ├── 周期上报 0x301 ~ 0x306
    └── 处理 RX Queue 中的 0x401 命令帧，并回复 0x307 ACK
```

---

## 13. 互斥锁、信号量与队列

当前已实现：

| RTOS 机制          | 用途                            |
| ---------------- | ----------------------------- |
| ctx mutex        | 保护全局 `BQ76940_AppCtx_t` 状态结构体 |
| i2c mutex        | 保护 Soft I2C 总线                |
| binary semaphore | 任务链路接力                        |
| CAN RX queue     | CAN 接收中断与 CANTask 解耦          |

锁粒度设计：

```text
I2C mutex：
    只保护硬件 I2C 读写

ctx mutex：
    只保护全局状态读取 / 提交

无锁阶段：
    数据换算、算法判断、状态决策
```

这种设计避免长时间持有全局锁，提高任务实时性和模块解耦程度。

---

## 14. CAN 通讯协议

当前已实现 CAN 周期上报、故障诊断上报、均衡状态上报以及 PC 端命令请求 / ACK 响应闭环。

CAN 波特率：

```text
500 kbps
```

---

### 14.1 BMS → PC 状态帧

| CAN ID  | 内容                                                                   |
| ------- | -------------------------------------------------------------------- |
| `0x301` | Pack 总压、Pack 电流                                                      |
| `0x302` | Cell 1 ~ Cell 4 电压                                                   |
| `0x303` | Cell 5 ~ Cell 8 电压                                                   |
| `0x304` | Cell 9 电压、TS1 温度、告警标志、保护标志、均衡目标、电流方向                                 |
| `0x305` | 故障诊断帧：BringUp / RuntimeFault / HwFault / None                        |
| `0x306` | 均衡状态帧：bal_active、target_count、CELLBAL1/2/3、parity_phase、target_label |

---

### 14.2 `0x301` Pack 状态帧

| Byte    | 内容                      |
| ------- | ----------------------- |
| Byte0~3 | Pack 总压，单位 mV，uint32，小端 |
| Byte4~7 | Pack 电流，单位 mA，int32，小端  |

---

### 14.3 `0x302` Cell 1 ~ Cell 4

| Byte    | 内容                    |
| ------- | --------------------- |
| Byte0~1 | Cell1 电压，mV，uint16，小端 |
| Byte2~3 | Cell2 电压，mV，uint16，小端 |
| Byte4~5 | Cell3 电压，mV，uint16，小端 |
| Byte6~7 | Cell4 电压，mV，uint16，小端 |

---

### 14.4 `0x303` Cell 5 ~ Cell 8

| Byte    | 内容                    |
| ------- | --------------------- |
| Byte0~1 | Cell5 电压，mV，uint16，小端 |
| Byte2~3 | Cell6 电压，mV，uint16，小端 |
| Byte4~5 | Cell7 电压，mV，uint16，小端 |
| Byte6~7 | Cell8 电压，mV，uint16，小端 |

---

### 14.5 `0x304` Cell 9 + 状态

| Byte    | 内容                    |
| ------- | --------------------- |
| Byte0~1 | Cell9 电压，mV，uint16，小端 |
| Byte2~3 | TS1 温度，单位 0.1°C       |
| Byte4   | alarm_flags           |
| Byte5   | protect_flags         |
| Byte6   | bal_target_label      |
| Byte7   | pack_current_dir      |

`alarm_flags`：

| bit  | 含义   |
| ---- | ---- |
| bit0 | UV   |
| bit1 | OV   |
| bit2 | DIFF |
| bit3 | OT   |
| bit4 | UT   |

`protect_flags`：

| bit  | 含义                     |
| ---- | ---------------------- |
| bit0 | OT cutoff active       |
| bit1 | UT charge block active |
| bit2 | HW DSG block active    |
| bit3 | HW OCD active          |
| bit4 | HW SCD active          |
| bit5 | Balance active         |

---

### 14.6 `0x305` 故障诊断帧

`0x305` 用于上报当前故障诊断状态。

故障类型：

| fault_type | 含义            |
| ---------- | ------------- |
| `0x00`     | None          |
| `0x01`     | BringUp Fault |
| `0x02`     | Runtime Fault |
| `0x03`     | HwFault       |

无故障时：

```text
0x305: 00 00 00 00 00 00 00 00
```

Runtime Fault 时：

| Byte    | 内容                      |
| ------- | ----------------------- |
| Byte0   | fault_type = Runtime    |
| Byte1   | runtime fault code      |
| Byte2   | runtime fault stage     |
| Byte3   | safe_off_result         |
| Byte4   | runtime fault active    |
| Byte5   | safe_off_retry_count    |
| Byte6~7 | total_sample_fail_count |

HwFault 时：

| Byte    | 内容                        |
| ------- | ------------------------- |
| Byte0   | fault_type = HwFault      |
| Byte1   | hw_fault_last_code        |
| Byte2   | hw_fault_sys_stat_latched |
| Byte3   | hw_fault_flags            |
| Byte4   | hw_fault_last_apply_ret   |
| Byte5   | reserved                  |
| Byte6~7 | hw_fault_count            |

---

### 14.7 `0x306` 均衡状态帧

| Byte  | 内容               |
| ----- | ---------------- |
| Byte0 | bal_active       |
| Byte1 | bal_target_count |
| Byte2 | CELLBAL1 mask    |
| Byte3 | CELLBAL2 mask    |
| Byte4 | CELLBAL3 mask    |
| Byte5 | bal_parity_phase |
| Byte6 | bal_target_label |
| Byte7 | reserved         |

示例：

```text
0x306: 01 02 02 00 02 01 02 00
```

含义：

```text
bal_active       = 1
target_count     = 2
CELLBAL1 mask    = 0x02
CELLBAL2 mask    = 0x00
CELLBAL3 mask    = 0x02
parity_phase     = 1
target_label     = 2
```

---

### 14.8 PC → BMS 命令帧 `0x401`

| CAN ID  | 方向       | 内容    |
| ------- | -------- | ----- |
| `0x401` | PC → BMS | 命令请求帧 |

数据格式：

| Byte    | 含义       |
| ------- | -------- |
| Byte0   | cmd      |
| Byte1   | seq      |
| Byte2~7 | reserved |

当前支持命令：

| CMD    | 功能                        |
| ------ | ------------------------- |
| `0x01` | 请求立即上报全部状态帧 `0x301~0x306` |
| `0x02` | 请求故障诊断帧 `0x305`           |
| `0x03` | 请求均衡状态帧 `0x306`           |
| 其他     | 返回 UNKNOWN_CMD ACK，不执行动作  |

示例：

```text
0x401: 01 01 00 00 00 00 00 00
```

含义：

```text
cmd = 0x01，请求全部状态
seq = 0x01，本次命令序号
```

---

### 14.9 BMS → PC ACK 帧 `0x307`

| CAN ID  | 方向       | 内容         |
| ------- | -------- | ---------- |
| `0x307` | BMS → PC | 命令 ACK 响应帧 |

数据格式：

| Byte    | 含义           |
| ------- | ------------ |
| Byte0   | 原命令 cmd      |
| Byte1   | 原命令 seq      |
| Byte2   | result       |
| Byte3   | detail       |
| Byte4   | status_flags |
| Byte5   | fault_type   |
| Byte6~7 | reserved     |

ACK result：

| result | 含义          |
| ------ | ----------- |
| `0x00` | OK          |
| `0x01` | UNKNOWN_CMD |
| `0x02` | INVALID_DLC |
| `0x03` | REJECTED    |
| `0x04` | EXEC_FAIL   |

`status_flags`：

| bit  | 含义                     |
| ---- | ---------------------- |
| bit0 | RuntimeFault active    |
| bit1 | HW DSG block active    |
| bit2 | Balance active         |
| bit3 | OT cutoff active       |
| bit4 | UT charge block active |

示例：

```text
0x307: FF 04 01 FF 04 00 00 00
```

含义：

```text
cmd          = 0xFF
seq          = 0x04
result       = UNKNOWN_CMD
detail       = 0xFF
status_flags = 0x04，当前正在均衡
fault_type   = 0x00，无故障
```

---

## 15. CAN RX Queue 设计

CAN 接收采用 **RX 中断 + FreeRTOS Queue** 的方式。

接收路径：

```text
PCAN 发送 0x401
    ↓
STM32 CAN 外设接收
    ↓
CAN RX FIFO0
    ↓
USB_LP_CAN1_RX0_IRQHandler
    ↓
HAL_CAN_IRQHandler
    ↓
HAL_CAN_RxFifo0MsgPendingCallback
    ↓
HAL_CAN_GetRxMessage 读取帧
    ↓
封装为 CAN_DrvRxFrame_t
    ↓
xQueueSendFromISR 投递到 CAN RX Queue
    ↓
BMS_CANTask xQueueReceive 取出
    ↓
BQ76940_AppHandleCanCommand 解析命令
    ↓
发送状态帧 / 故障帧 / 均衡帧
    ↓
发送 0x307 ACK
```

设计原则：

* 中断中只读取 CAN FIFO 并投递队列
* 不在中断中解析业务命令
* 不在中断中访问 BMS 全局状态
* 不在中断中执行复杂发送流程
* 命令解析统一在 BMS_CANTask 中完成
* 队列满时记录 drop count，避免中断阻塞

---

## 16. 运行时串口输出

当前默认串口输出采用一行摘要形式：

```text
[BMS] P=35346mV MAX=VC1:3967mV MIN=VC6:3842mV D=125mV I=2mA T=291dC ALM=00 PROT=20 BAL=1:VC1 SYS=00
```

字段说明：

| 字段   | 含义               |
| ---- | ---------------- |
| P    | Pack 总压          |
| MAX  | 最高单体             |
| MIN  | 最低单体             |
| D    | 最大压差             |
| I    | Pack 电流          |
| T    | TS1 温度，单位 0.1°C  |
| ALM  | 软件告警标志           |
| PROT | 保护状态标志           |
| BAL  | 均衡状态与目标电芯        |
| SYS  | BQ76940 SYS_STAT |

---

## 17. PCAN-View 实物验证

当前已通过 PCAN-View 完成 CAN V1 验证。

已验证内容：

* `0x301~0x306` 周期上报
* `0x306` 均衡状态帧上报
* `0x401` 请求全部状态
* `0x401` 请求故障诊断
* `0x401` 请求均衡状态
* `0x307` ACK 响应
* 未知命令 `0xFF` 返回 UNKNOWN_CMD
* ACK 中 status_flags 与当前均衡状态一致

示例 1：请求全部状态

```text
PC -> BMS:
0x401: 01 01 00 00 00 00 00 00

BMS -> PC:
0x301
0x302
0x303
0x304
0x305
0x306
0x307: 01 01 00 00 04 00 00 00
```

示例 2：未知命令

```text
PC -> BMS:
0x401: FF 04 00 00 00 00 00 00

BMS -> PC:
0x307: FF 04 01 FF 04 00 00 00
```

其中 `0x307` Byte4 = `0x04` 表示当前 `bal_active = 1`，系统正在均衡。

---

## 18. 软件架构

当前项目采用分层结构：

```text
STM32-BMS-48Pro
├── Users
│   ├── main.c
│   ├── bms_config.h
│   ├── bms_log.h
│   └── App
│       ├── bq76940_app
│       ├── bq76940_app_can
│       ├── bq76940_balance
│       ├── bq76940_protect
│       ├── bq76940_diag
│       ├── bms_tasks
│       └── bq34z100_app
│
├── Drivers
│   ├── BSP
│   │   ├── can
│   │   │   └── can_drv
│   │   ├── soft_i2c
│   │   │   └── soft_i2c1
│   │   ├── uart
│   │   ├── led
│   │   └── io_ctrl
│   │
│   ├── BQ76940
│   │   ├── bq76940_drv
│   │   ├── bq76940_alarm
│   │   ├── bq76940_protect
│   │   └── bq76940_print
│   │
│   └── BQ76200
│       ├── bq76200_exec
│       └── bq76200_exec_port
│
├── Middleware
│   └── FreeRTOS
│
├── docs
│   ├── flowcharts
│   ├── diagrams
│   └── test_logs
│
└── Projects
    └── MDK-ARM
```

---

## 19. 项目特点

本项目重点体现以下嵌入式工程能力：

* BQ76940 电池监控芯片真实硬件调试
* 多串电池采样与校准换算
* BMS 软件告警与硬件保护逻辑设计
* BQ76940 CELLBAL 自动均衡控制
* BQ76200 执行层状态机设计
* FreeRTOS 多任务拆分与同步
* I2C 总线互斥与全局状态保护
* CAN 周期上报、命令接收与 ACK 闭环
* CAN RX 中断与 FreeRTOS Queue 解耦
* RuntimeFault / HwFault / BringUpFault 安全处理
* Safe-Off 异常关断流程
* 模块化代码重构与 Git 分支管理
* PCAN-View 实物验证与测试截图留档

---

## 20. 项目边界说明

本项目为学习、重构与求职展示项目，当前 V1 重点验证 BMS 软件架构与核心控制流程。

当前 V1 已实现核心功能，但仍保留以下后续扩展方向：

* BQ34Z100-G1 电量计深度接入
* Qt 上位机状态显示
* Flash 参数保存
* 更完整的 SOC / SOH 估算
* 更完善的测试用例与自动化测试
* 更复杂的 CAN 上位机协议
* 更完整的实车 / 储能场景验证

出于安全边界考虑，当前 CAN V1 仅开放查询类命令，不开放远程强制开启 MOS、远程修改保护阈值、远程强制均衡等高风险控制命令。

---

## 21. 当前收尾阶段计划

当前项目已进入 V1 收尾阶段，后续重点不再继续堆功能，而是整理为求职展示作品。

收尾内容：

* 整理 README
* 补充流程图
* 保存 PCAN-View 截图
* 保存串口日志
* 整理 CAN 协议表
* 整理均衡策略说明
* 整理异常处理流程
* 准备项目复盘问答
* 补充 C / STM32 / FreeRTOS / CAN / I2C 基础知识

---

## 22. 作者

**Evan**

Embedded Developer

GitHub: [Secret-G](https://github.com/Secret-G)
