#ifndef CANCONNECTION_H
#define CANCONNECTION_H

#include <QObject>
#include <QCanBusDeviceInfo>
#include <QCanBusFrame>
#include <QList>
#include <QString>


class QCanBusDevice;

class CanConnection final : public QObject
{
    Q_OBJECT

public:
    explicit CanConnection(QObject *parent = nullptr);
    ~CanConnection() override;

    // 扫描由 Qt PeakCAN 插件识别到的全部 PEAK-System CAN 设备。
    QList<QCanBusDeviceInfo> scanDevices(QString *errorString = nullptr) const;

    // 使用经典 CAN 模式打开指定接口；bitRate 的单位为 bit/s。
    bool connectDevice(const QString &interfaceName,int bitRate,QString *errorString = nullptr);

    void disconnectDevice();

    bool isConnected() const;

    // 将完整 CAN 帧写入当前设备；连接检查和错误文本统一放在连接层。
    bool sendFrame(const QCanBusFrame &frame, QString *errorString = nullptr);

signals:
    // 将底层设备状态和错误转给界面，便于处理掉线等非主动事件。
    void connectionStateChanged(bool connected);
    void errorOccurred(const QString &message);

    // 每收到一帧完整 CAN 报文就通知上层；协议解析不放在连接层中。
    void frameReceived(const QCanBusFrame &frame);

private:
    // 读取 Qt CAN 接收队列中等待处理的全部报文。
    void readPendingFrames();

    QCanBusDevice *m_device;
};

#endif // CANCONNECTION_H
