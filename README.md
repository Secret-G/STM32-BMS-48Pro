项目名称
# STM32-BMS-48Pro

基于 STM32F103C8T6 + BQ76940 的电池管理系统（BMS）项目。

该项目以学习和重构商业 BMS 架构为目标，逐步实现电池电压采集、电流采集、温度监测、均衡控制、故障保护、执行控制、CAN 通讯等功能。

目前项目仍在持续开发中。
项目简介
## 项目简介

本项目用于学习和实践嵌入式电池管理系统（BMS）设计。

硬件平台：

- STM32F103C8T6
- BQ76940 电池监控芯片
- BQ76200 高边驱动器（规划中）
- BQ34Z100-G1 电量计（规划中）

软件平台：

- C语言
- FreeRTOS
- Keil MDK

项目采用模块化设计，包含驱动层、应用层、保护层和执行层。
已实现功能
## 已实现功能

### 电池采样

- 单体电压采集
- 总电压计算
- 最大/最小单体统计
- 压差统计

### 电流采样

- Coulomb Counter 电流读取
- 充放电方向判断

### 温度监测

- TS1 温度采集
- 热敏电阻换算
- 过温保护
- 低温保护

### 软件告警

- 欠压保护（UV）
- 过压保护（OV）
- 压差保护（DIFF）

### FET控制

- CHG FET 控制
- DSG FET 控制
- SYS_CTRL2 状态验证

### 均衡功能

- 手动均衡
- 自动均衡
软件架构
## 软件架构

Drivers
├── bq76940_drv
├── soft_i2c
└── uart

App
├── voltage monitor
├── current monitor
├── temperature protect
├── balance control
└── fault management

Middleware
└── FreeRTOS
项目进度
## 项目进度

- [x] BQ76940 Bring-up
- [x] 单体电压采集
- [x] 电流采集
- [x] 温度采集
- [x] UV/OV保护
- [x] 均衡控制
- [x] FreeRTOS移植
- [ ] CAN通讯
- [ ] BQ76200执行层
- [ ] BQ34Z100电量计
作者
## 作者

Evan

Embedded Developer

GitHub:
https://github.com/Secret-G
