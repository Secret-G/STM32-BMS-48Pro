#include "bmscanprotocol.h"

#include <QByteArray>
#include <QCanBusFrame>

namespace
{

// 从payload指定位置读取一个uint16小端数据。
// 例如 F2 0E 会被还原成0x0EF2，也就是3826。
quint16 readU16LittleEndian(const QByteArray &payload, int offset)
{
    const quint16 lowByte = static_cast<quint8>(payload.at(offset));

    const quint16 highByte = static_cast<quint8>(payload.at(offset + 1));

    return static_cast<quint16>(lowByte | (highByte << 8));
}

// 从payload指定位置读取一个uint32小端数据。
quint32 readU32LittleEndian(const QByteArray &payload, int offset)
{
    const quint32 byte0 =
        static_cast<quint8>(payload.at(offset));

    const quint32 byte1 =
        static_cast<quint8>(payload.at(offset + 1));

    const quint32 byte2 =
        static_cast<quint8>(payload.at(offset + 2));

    const quint32 byte3 =
        static_cast<quint8>(payload.at(offset + 3));

    return byte0
           | (byte1 << 8)
           | (byte2 << 16)
           | (byte3 << 24);
}

// 按照int32补码格式读取有符号小端数据，用于解析Pack电流。
qint32 readI32LittleEndian(const QByteArray &payload, int offset)
{
    const quint32 rawValue = readU32LittleEndian(payload, offset);

    // 最高位为0，表示非负数。
    if ((rawValue & 0x80000000U) == 0U)
    {
        return static_cast<qint32>(rawValue);
    }

    // 最高位为1，按照32位补码转换成负数。
    const qint64 signedValue = static_cast<qint64>(rawValue) - 0x100000000LL;

    return static_cast<qint32>(signedValue);
}

qint16 readI16LittleEndian(const QByteArray &payload, int offset)
{
    const quint16 rawValue = readU16LittleEndian(payload, offset);
    if ((rawValue & 0x8000U) == 0U)
    {
        return static_cast<qint16>(rawValue);
    }
    return static_cast<qint16>(static_cast<qint32>(rawValue) - 0x10000);
}

qint8 readI8(const QByteArray &payload, int offset)
{
    const quint8 rawValue = static_cast<quint8>(payload.at(offset));
    if ((rawValue & 0x80U) == 0U)
    {
        return static_cast<qint8>(rawValue);
    }
    return static_cast<qint8>(static_cast<qint16>(rawValue) - 0x100);
}

bool isEightByteStandardDataFrame(const QCanBusFrame &frame, quint32 expectedId)
{
    return frame.frameType() == QCanBusFrame::DataFrame
           && !frame.hasExtendedFrameFormat()
           && frame.frameId() == expectedId
           && frame.payload().size() == 8;
}

} // namespace

bool BmsCanProtocol::parseCellVoltages(const QCanBusFrame &frame,BmsCellVoltageData &cellData)
{
    // 单体电压只存在于普通数据帧中，远程帧和错误帧不进行解析。
    if (frame.frameType() != QCanBusFrame::DataFrame)
    {
        return false;
    }

    // 当前BMS协议使用11位标准帧，防止误解析同ID的扩展帧。
    if (frame.hasExtendedFrameFormat())
    {
        return false;
    }

    const QByteArray payload = frame.payload();

    switch (frame.frameId())
    {
        case 0x302:
            // 0x302必须至少有8字节，分别保存Cell1～Cell4。
            if (payload.size() != 8)
            {
                return false;
            }

            for (int index = 0; index < 4; ++index)
            {
                cellData.millivolts[index] = readU16LittleEndian(payload, index * 2);
                cellData.valid[index] = true;
            }
            return true;

        case 0x303:
            // 0x303必须至少有8字节，分别保存Cell5～Cell8。
            if (payload.size() != 8)
            {
                return false;
            }

            for (int index = 0; index < 4; ++index)
            {
                const int cellIndex = index + 4;
                cellData.millivolts[cellIndex] = readU16LittleEndian(payload, index * 2);
                cellData.valid[cellIndex] = true;
            }
            return true;

        case 0x304:
            // 0x304的Byte0～1是Cell9，后面的字节以后再解析。
            if (payload.size() != 8)
            {
                return false;
            }

            cellData.millivolts[8] = readU16LittleEndian(payload, 0);
            cellData.valid[8] = true;
            return true;

        default:
            // 当前帧不是单体电压报文。
            return false;
    }
}

bool BmsCanProtocol::parsePackStatus(const QCanBusFrame &frame,BmsPackStatusData &packData)
{
    // Pack状态只解析普通数据帧。
    if (frame.frameType() != QCanBusFrame::DataFrame)
    {
        return false;
    }

    // 当前协议使用11位标准帧，不解析扩展帧。
    if (frame.hasExtendedFrameFormat())
    {
        return false;
    }

    // 只有0x301属于Pack总电压和电流报文。
    if (frame.frameId() != 0x301)
    {
        return false;
    }

    const QByteArray payload = frame.payload();

    // 0x301需要完整的8字节数据。
    if (payload.size() != 8)
    {
        return false;
    }

    // Byte0～3：Pack总电压，单位mV，无符号32位小端。
    packData.voltageMillivolts = readU32LittleEndian(payload, 0);

    // Byte4～7：Pack电流，单位mA，有符号32位小端。
    packData.currentMilliamps = readI32LittleEndian(payload, 4);

    packData.valid = true;

    return true;
}

bool BmsCanProtocol::parseStatus304(const QCanBusFrame &frame,BmsStatus304Data &statusData)
{
    if (!isEightByteStandardDataFrame(frame, 0x304U))
    {
        return false;
    }

    const QByteArray payload = frame.payload();
    statusData.temperatureDeciC = readI16LittleEndian(payload, 2);
    statusData.alarmFlags = static_cast<quint8>(payload.at(4));
    statusData.protectFlags = static_cast<quint8>(payload.at(5));
    statusData.balanceTargetLabel = static_cast<quint8>(payload.at(6));
    statusData.currentDirection = readI8(payload, 7);
    statusData.valid = true;
    return true;
}

bool BmsCanProtocol::parseFaultStatus(const QCanBusFrame &frame,BmsFaultData &faultData)
{
    if (!isEightByteStandardDataFrame(frame, 0x305U))
    {
        return false;
    }

    const QByteArray payload = frame.payload();

    faultData.type = static_cast<quint8>(payload.at(0));

    for (int index = 0; index < 7; ++index)
    {
        faultData.detail[index] = static_cast<quint8>(payload.at(index + 1));
    }
    faultData.valid = true;
    return true;
}

bool BmsCanProtocol::parseBalanceStatus(const QCanBusFrame &frame,BmsBalanceData &balanceData)
{
    if (!isEightByteStandardDataFrame(frame, 0x306U))
    {
        return false;
    }

    const QByteArray payload = frame.payload();

    balanceData.active = static_cast<quint8>(payload.at(0)) != 0U;

    balanceData.targetCount = static_cast<quint8>(payload.at(1));

    balanceData.cellBal1 = static_cast<quint8>(payload.at(2));

    balanceData.cellBal2 = static_cast<quint8>(payload.at(3));

    balanceData.cellBal3 = static_cast<quint8>(payload.at(4));

    balanceData.parityPhase = static_cast<quint8>(payload.at(5));

    balanceData.targetLabel = static_cast<quint8>(payload.at(6));
    balanceData.valid = true;
    return true;
}

bool BmsCanProtocol::parseCommandAck(const QCanBusFrame &frame, BmsCommandAckData &ackData)
{
    if (!isEightByteStandardDataFrame(frame, 0x307U))
    {
        return false;
    }

    const QByteArray payload = frame.payload();

    ackData.command = static_cast<quint8>(payload.at(0));
    ackData.sequence = static_cast<quint8>(payload.at(1));
    ackData.result = static_cast<quint8>(payload.at(2));
    ackData.detail = static_cast<quint8>(payload.at(3));
    ackData.statusFlags = static_cast<quint8>(payload.at(4));
    ackData.faultType = static_cast<quint8>(payload.at(5));
    ackData.valid = true;
    return true;
}

QString BmsCanProtocol::describeFrame(const QCanBusFrame &frame)
{
    if (frame.frameType() != QCanBusFrame::DataFrame)
    {
        return QStringLiteral("非数据帧");
    }

    switch (frame.frameId())
    {
        case 0x301U: return QStringLiteral("Pack总压 / 电流");
        case 0x302U: return QStringLiteral("Cell1 ～ Cell4");
        case 0x303U: return QStringLiteral("Cell5 ～ Cell8");
        case 0x304U: return QStringLiteral("Cell9 / 温度 / 状态");
        case 0x305U: return QStringLiteral("故障诊断");
        case 0x306U: return QStringLiteral("均衡状态");
        case 0x307U: return QStringLiteral("命令ACK");
        case 0x401U: return QStringLiteral("PC命令请求");
        default: return QStringLiteral("未定义报文");
    }
}
