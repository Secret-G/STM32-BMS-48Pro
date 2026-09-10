#ifndef BMSCANPROTOCOL_H
#define BMSCANPROTOCOL_H

#include <array>
#include <QString>
#include <QtGlobal>

class QCanBusFrame;

// 保存9节单体电压，以及每节电压是否已经收到有效数据。
struct BmsCellVoltageData
{
    std::array<quint16, 9> millivolts{};
    std::array<bool, 9> valid{};
};

// 保存0x301中的Pack总电压和Pack电流。
struct BmsPackStatusData
{
    quint32 voltageMillivolts = 0;
    qint32 currentMilliamps = 0;

    // 只有成功收到并解析0x301后，数据才可以用于界面显示。
    bool valid = false;
};

// 0x304：Cell9 之外的温度、告警、保护和方向状态。
struct BmsStatus304Data
{
    qint16 temperatureDeciC = 0;
    quint8 alarmFlags = 0;
    quint8 protectFlags = 0;
    quint8 balanceTargetLabel = 0;
    qint8 currentDirection = 0;
    bool valid = false;
};

// 0x305：故障类型决定 Byte1～Byte7 的含义，因此保留原始字段。
struct BmsFaultData
{
    quint8 type = 0;
    std::array<quint8, 7> detail{};
    bool valid = false;
};

// 0x306：均衡任务的实时状态和三个 CELLBAL 寄存器镜像。
struct BmsBalanceData
{
    bool active = false;
    quint8 targetCount = 0;
    quint8 cellBal1 = 0;
    quint8 cellBal2 = 0;
    quint8 cellBal3 = 0;
    quint8 parityPhase = 0;
    quint8 targetLabel = 0;
    bool valid = false;
};

// 0x307：BMS 对 PC 命令的应答。
struct BmsCommandAckData
{
    quint8 command = 0;
    quint8 sequence = 0;
    quint8 result = 0;
    quint8 detail = 0;
    quint8 statusFlags = 0;
    quint8 faultType = 0;
    bool valid = false;
};

// 0x308: transport validity does not certify calibration or learning accuracy.
struct BmsGaugeData
{
    quint8 socPercent = 0;
    quint8 sohPercent = 0;
    quint16 remainingCapacityMah = 0;
    quint16 fullChargeCapacityMah = 0;
    quint8 lastError = 0;
    bool received = false;
    bool valid = false;
};

class BmsCanProtocol
{
public:
    static bool parseGaugeStatus(const QCanBusFrame &frame, BmsGaugeData &gaugeData);
    // 解析0x302、0x303、0x304中的单体电压。
    // 如果当前帧属于单体电压报文并且长度正确，则返回true。
    static bool parseCellVoltages(const QCanBusFrame &frame,BmsCellVoltageData &cellData);

    // 解析0x301中的Pack总电压和有符号电流。
    static bool parsePackStatus(const QCanBusFrame &frame,BmsPackStatusData &packData);

    static bool parseStatus304(const QCanBusFrame &frame, BmsStatus304Data &statusData);
    static bool parseFaultStatus(const QCanBusFrame &frame, BmsFaultData &faultData);
    static bool parseBalanceStatus(const QCanBusFrame &frame, BmsBalanceData &balanceData);
    static bool parseCommandAck(const QCanBusFrame &frame, BmsCommandAckData &ackData);

    // 原始报文表格使用简短描述，不在 MainWindow 中重复判断 CAN ID。
    static QString describeFrame(const QCanBusFrame &frame);
};

#endif // BMSCANPROTOCOL_H
