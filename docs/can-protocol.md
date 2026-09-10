# CAN 通讯协议

当前已实现 CAN 周期上报、故障诊断上报、均衡状态上报以及 PC 端命令请求 / ACK 响应闭环。

CAN 波特率：

```text
500 kbps
```

---

## 1 BMS → PC 状态帧

| CAN ID  | 内容                                                                   |
| ------- | -------------------------------------------------------------------- |
| `0x301` | Pack 总压、Pack 电流                                                      |
| `0x302` | Cell 1 ~ Cell 4 电压                                                   |
| `0x303` | Cell 5 ~ Cell 8 电压                                                   |
| `0x304` | Cell 9 电压、TS1 温度、告警标志、保护标志、均衡目标、电流方向                                 |
| `0x305` | 故障诊断帧：BringUp / RuntimeFault / HwFault / None                        |
| `0x306` | 均衡状态帧：bal_active、target_count、CELLBAL1/2/3、parity_phase、target_label |
| `0x308` | BQ34Z100：SOC、SOH、剩余容量、满充容量、数据有效标志、错误码 |

---

## 2 `0x301` Pack 状态帧

| Byte    | 内容                      |
| ------- | ----------------------- |
| Byte0~3 | Pack 总压，单位 mV，uint32，小端 |
| Byte4~7 | Pack 电流，单位 mA，int32，小端  |

---

## 3 `0x302` Cell 1 ~ Cell 4

| Byte    | 内容                    |
| ------- | --------------------- |
| Byte0~1 | Cell1 电压，mV，uint16，小端 |
| Byte2~3 | Cell2 电压，mV，uint16，小端 |
| Byte4~5 | Cell3 电压，mV，uint16，小端 |
| Byte6~7 | Cell4 电压，mV，uint16，小端 |

---

## 4 `0x303` Cell 5 ~ Cell 8

| Byte    | 内容                    |
| ------- | --------------------- |
| Byte0~1 | Cell5 电压，mV，uint16，小端 |
| Byte2~3 | Cell6 电压，mV，uint16，小端 |
| Byte4~5 | Cell7 电压，mV，uint16，小端 |
| Byte6~7 | Cell8 电压，mV，uint16，小端 |

---

## 5 `0x304` Cell 9 + 状态

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

## 6 `0x305` 故障诊断帧

`0x305` 用于上报当前故障诊断状态。

故障类型：

| fault_type | 含义            |
| ---------- | ------------- |
| `0x00`     | None          |
| `0x01`     | BringUp Fault |
| `0x02`     | Runtime Fault |
| `0x03`     | HwFault       |
| `0x04`     | RTOS Init Fault |

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

## 7 `0x306` 均衡状态帧

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

## 8 PC → BMS 命令帧 `0x401`

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

## 9 BMS → PC ACK 帧 `0x307`

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

## 补充：启动诊断字段与故障语义

本文由原 README 的协议章节迁移，并按当前固件补充启动诊断。实现入口为 [bq76940_app_can.c](../Users/App/bq76940_app_can.c)。所有多字节整数使用小端编码；TS1 温度为有符号 int16，电流方向为有符号 int8。

### BringUp Fault（fault_type = 0x01）

| Byte | 内容 |
| --- | --- |
| 0 | 0x01 |
| 1 | main_ret |
| 2 | bringup_last_stage |
| 3 | bringup_last_error |
| 4 | bringup_attempt_count |
| 5 | safe_off_result |
| 6 | bit0：BringUp 故障有效；bit1：Safe-Off 返回非零 |
| 7 | reserved，0 |

### RTOS Init Fault（fault_type = 0x04）

| Byte | 内容 |
| --- | --- |
| 0 | 0x04 |
| 1 | RTOS 初始化错误码 err_code |
| 2 | 0 |
| 3 | safe_off_result |
| 4～7 | 0 |

### HwFault 与周期诊断优先级

周期诊断选择顺序为 Runtime Fault → HwFault 历史记录 → None。只要 `hw_fault_count != 0`，仍可上报 HwFault 类型；这不等同于当前 OCD/SCD 仍有效。

HwFault Byte3：bit0 为 OCD active，bit1 为 SCD active，bit2 为 DSG block。应结合这些标志判断当前状态。BringUp 和 RTOS 初始化失败由启动故障路径单独发送。

### `0x308` 电量计状态帧

11 位标准数据帧，DLC 固定为 8。每秒随其他状态帧上报；收到 `0x401` 的 `0x01` 查询全部状态命令（DLC=8）时也发送，随后沿用原有 ACK。ACK 表示命令已处理，不保证 PC 已收到每个状态帧。

| Byte | 内容 |
| --- | --- |
| 0 | SOC，uint8，0～100% |
| 1 | SOH，uint8，0～100% |
| 2～3 | RemainingCapacity，uint16，小端，mAh |
| 4～5 | FullChargeCapacity，uint16，小端，mAh |
| 6 | data_valid：1=最近完整采样有效，0=不可使用数值 |
| 7 | last_error：0=成功；非零错误见下表 |

容量单位沿用当前驱动的未缩放 mAh。若生产配置使用容量/电流缩放，必须在固件中加入与该配置匹配的换算并重新核对协议量程，不能直接将缩放后的原始值当作 mAh。

| last_error | 含义 |
| --- | --- |
| 10～21（十进制） | 依次为 SOC、MaxError、RM、FCC、电压、平均电流、电流、温度、Flags、FlagsB、循环次数、SOH 读取失败 |
| 22（十进制） | SOC、SOH 或 MaxError 超出 0～100 |
| 0xF0 | I²C 总线互斥锁超时 |
| 0xF1 | 尚未完成首次采样 |
| 0xF2 | 编译配置关闭电量计任务 |
| 0xF3 | 最后一次成功采样已超过或等于 3000 ms |

无效时 Byte0～5 清零，接收方必须以 Byte6 判断有效性，不能将无效帧中的 SOC=0 解读为电池耗尽。恢复完整采样后自动恢复有效。

示例：`4B 5F B8 0B A0 0F 01 00` 表示 SOC=75%、SOH=95%、RM=3000 mAh、FCC=4000 mAh、有效。

Qt 显示 SOC 和 SOH；鼠标悬停在 SOC 或 SOH 上可查看容量。断开连接清空状态；连续 3500 ms 未收到合法格式的 `0x308` 时显示超时并隐藏数值。格式错误帧不会刷新接收时间。

`data_valid` 只表示通信、数值范围和采样新鲜度，不认证 ChemID、校准、学习、IT 启用状态或 SOC 精度。本帧不包含 Qmax；Flags、MaxError、循环次数等仍保留在固件采样上下文中，可通过周期调试日志查看。

验证：构建 Qt 时启用 `BMS_BUILD_PROTOCOL_TESTS=ON`，运行 `ctest --test-dir <build目录> --output-on-failure`。测试将实际固件采样/打包函数与实际 Qt 解析函数连接，覆盖逐项读取失败、恢复、小端容量、无效状态和错误帧格式。上板还需验证 `0x308` 周期收发、查询全部状态、I²C 断开/恢复和 CAN 停发超时。

> **待补充：**各故障码/阶段码枚举说明、带时间戳的命令与 ACK 实测报文，以及协议版本记录。
