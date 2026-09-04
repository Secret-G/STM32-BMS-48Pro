#include "canconnection.h"

#include <QCanBus>
#include <QCanBusDevice>
#include <QVariant>
#include <QCanBusFrame>
#include <QDebug>


CanConnection::CanConnection(QObject *parent)
    : QObject(parent),
      m_device(nullptr)
{
}

CanConnection::~CanConnection()
{
    // 程序退出时主动关闭PCAN通道，避免设备对象仍处于连接状态。
    disconnectDevice();
}

QList<QCanBusDeviceInfo> CanConnection::scanDevices(QString *errorString) const
{
    if (errorString != nullptr)
    {
        errorString->clear();
    }

    QCanBus *canBus = QCanBus::instance();

    if (!canBus->plugins().contains(QStringLiteral("peakcan")))
    {
        if (errorString != nullptr)
        {
            *errorString = QStringLiteral("未找到 Qt PeakCAN 插件");
        }

        return {};
    }

    return canBus->availableDevices(QStringLiteral("peakcan"), errorString);
}

bool CanConnection::connectDevice(const QString &interfaceName,int bitRate,QString *errorString)
{
    if (errorString != nullptr)
    {
        errorString->clear();
    }

    if (m_device != nullptr)
    {
        if (isConnected())
        {
            if (errorString != nullptr)
            {
                *errorString = QStringLiteral("CAN 设备已经连接");
            }
            return false;
        }

        // 设备异常掉线后对象可能仍存在；重连前先清理旧对象。
        disconnectDevice();
    }

    if (interfaceName.isEmpty())
    {
        if (errorString != nullptr)
        {
            *errorString = QStringLiteral("CAN 设备接口名为空");
        }
        return false;
    }

    if (bitRate <= 0)
    {
        if (errorString != nullptr)
        {
            *errorString = QStringLiteral("CAN 波特率无效");
        }
        return false;
    }

    // createDevice() 只创建 Qt 设备对象，真正打开通道由 connectDevice() 完成。
    m_device = QCanBus::instance()->createDevice(QStringLiteral("peakcan"),interfaceName,errorString);

    if (m_device == nullptr)
    {
        return false;
    }

    m_device->setParent(this);

    // PCAN-USB FD 支持 CAN FD，但本 BMS 固件使用经典 CAN，因此明确关闭 CAN FD。
    m_device->setConfigurationParameter(QCanBusDevice::CanFdKey,false);

    // 设置仲裁波特率，当前固件默认使用 500000 bit/s。
    m_device->setConfigurationParameter(QCanBusDevice::BitRateKey,bitRate);

    // 将底层错误转为 CanConnection 信号，供界面显示设备掉线等信息。
    connect(m_device,&QCanBusDevice::errorOccurred,this,[this](QCanBusDevice::CanBusError error)
    {
        if (error != QCanBusDevice::NoError && m_device != nullptr)
        {
            emit errorOccurred(m_device->errorString());
        }

    });

    connect(m_device, &QCanBusDevice::stateChanged,this,[this](QCanBusDevice::CanBusDeviceState state)
    {
        if (state == QCanBusDevice::ConnectedState)
        {
            emit connectionStateChanged(true);

        }
        else if (state == QCanBusDevice::UnconnectedState)
        {
            emit connectionStateChanged(false);
        }
    });

    // framesReceived 只表示“队列里有数据”，实际帧需要继续调用 readFrame() 取出。
    connect(m_device,&QCanBusDevice::framesReceived,this,&CanConnection::readPendingFrames);

    /*连接设备*/
    if (!m_device->connectDevice())
    {
        if (errorString != nullptr)
        {
            *errorString = m_device->errorString();
        }

        delete m_device;
        m_device = nullptr;
        return false;
    }

    return true;
}

void CanConnection::disconnectDevice()
{
    // 空指针表示没有创建 CAN 设备，重复调用断开也能安全返回。
    if (m_device == nullptr)
    {
        return;
    }

    // 先关闭底层 PeakCAN 通道，再释放 Qt 设备对象。
    if (m_device->state() != QCanBusDevice::UnconnectedState)
    {
        m_device->disconnectDevice();
    }

    delete m_device;
    m_device = nullptr;
}

bool CanConnection::isConnected() const
{
    // 对象存在且 Qt 状态为 ConnectedState，才表示通道真正可用。
    return m_device != nullptr && m_device->state() == QCanBusDevice::ConnectedState;
}

bool CanConnection::sendFrame(const QCanBusFrame &frame, QString *errorString)
{
    if (errorString != nullptr)
    {
        errorString->clear();
    }

    if (!isConnected())
    {
        if (errorString != nullptr)
        {
            *errorString = QStringLiteral("CAN 设备尚未连接");
        }
        return false;
    }

    if (!frame.isValid())
    {
        if (errorString != nullptr)
        {
            *errorString = QStringLiteral("待发送的 CAN 帧无效");
        }
        return false;
    }

    if (!m_device->writeFrame(frame))
    {
        if (errorString != nullptr)
        {
            *errorString = m_device->errorString().isEmpty()
                               ? QStringLiteral("CAN 帧未能加入发送队列")
                               : m_device->errorString();
        }
        return false;
    }

    return true;
}

void CanConnection::readPendingFrames()
{
        // 设备对象不存在时不能访问接收队列。
        if (m_device == nullptr)
        {
            return;
        }

        // 一次framesReceived通知可能对应多条报文，因此需要把队列读空。
        while (m_device->framesAvailable() > 0)
        {
            const QCanBusFrame frame = m_device->readFrame();

            if (!frame.isValid())
            {
                continue;
            }
            emit frameReceived(frame);
        }
}



