#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "bmscanprotocol.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CanConnection;
class QCanBusFrame;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;


private:
    // 初始化CAN原始报文表格，只设置显示格式，不添加演示数据。
    void initializeCanFrameTable();

    // 每收到一帧真实CAN报文，就向原始报文表格增加一行。
    void appendCanFrameToTable(const QCanBusFrame &frame,
                               const QString &direction,
                               const QString &parseResult);

    void refreshCanDevices();
    void toggleCanConnection();
    void updateConnectionControls(bool connected);
    void setConnectionStatus(const QString &text, const QString &color);
    void handleCanConnectionStateChanged(bool connected);
    void handleCanError(const QString &message);
    void handleCanFrame(const QCanBusFrame &frame);
    void appendCommunicationLog(const QString &message);
    void sendSelectedCommand();

    // 将解析后的9节单体电压更新到界面。
    void updateCellVoltageDisplay();

    // 更新顶部的Pack总电压和Pack电流。
    void updatePackStatusDisplay();

    // 比较Pack电压与9节单体电压总和。
    void updateVoltageConsistencyDisplay();
    void updateStatus304Display();
    void updateFaultDisplay();
    void updateBalanceDisplay();
    void updateBottomStatus();

    Ui::MainWindow *ui;

    CanConnection *m_canConnection;

    // 只控制原始报文表格是否继续增加新行，不影响底层CAN接收。
    bool m_canFrameTablePaused = false;

    // 保存最近一次收到的9节单体电压。
    BmsCellVoltageData m_cellVoltageData;

    // 保存最近一次从0x301解析出的总电压和电流。
    BmsPackStatusData m_packStatusData;

    BmsStatus304Data m_status304Data;
    BmsFaultData m_faultData;
    BmsBalanceData m_balanceData;
    BmsCommandAckData m_ackData;

    quint64 m_receivedFrameCount = 0;
    quint64 m_transmittedFrameCount = 0;
    quint64 m_parseErrorCount = 0;
    quint8 m_commandSequence = 0;
};
#endif // MAINWINDOW_H
