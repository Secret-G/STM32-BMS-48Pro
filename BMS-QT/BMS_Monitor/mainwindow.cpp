#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "canconnection.h"
#include "chartwidgets.h"

#include <QCanBusFrame>
#include <QDateTime>
#include <QHeaderView>
#include <QList>
#include <QScrollBar>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTextDocument>
#include <QTime>
#include <array>
#include <QLabel>
#include <QFrame>
#include <QStyle>
#include <QTimer>
#include <QVector>

namespace
{

QString hexByte(quint8 value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString alarmFlagsToText(quint8 flags)
{
    QStringList names;
    if ((flags & 0x01U) != 0U) names << QStringLiteral("UV");
    if ((flags & 0x02U) != 0U) names << QStringLiteral("OV");
    if ((flags & 0x04U) != 0U) names << QStringLiteral("DIFF");
    if ((flags & 0x08U) != 0U) names << QStringLiteral("OT");
    if ((flags & 0x10U) != 0U) names << QStringLiteral("UT");
    return names.isEmpty() ? QStringLiteral("NONE") : names.join(QStringLiteral(" / "));
}

QString protectFlagsToText(quint8 flags)
{
    QStringList names;
    if ((flags & 0x01U) != 0U) names << QStringLiteral("OT_CUTOFF");
    if ((flags & 0x02U) != 0U) names << QStringLiteral("UT_CHG_BLOCK");
    if ((flags & 0x04U) != 0U) names << QStringLiteral("HW_DSG_BLOCK");
    if ((flags & 0x08U) != 0U) names << QStringLiteral("HW_OCD");
    if ((flags & 0x10U) != 0U) names << QStringLiteral("HW_SCD");
    if ((flags & 0x20U) != 0U) names << QStringLiteral("BALANCE");
    return names.isEmpty() ? QStringLiteral("NONE") : names.join(QStringLiteral(" / "));
}

QString currentDirectionToText(qint8 direction)
{
    if (direction > 0) return QStringLiteral("充电");
    if (direction < 0) return QStringLiteral("放电");
    return QStringLiteral("静止");
}

// BQ76940实际接入的是VC1、VC2、VC5、VC6、VC7、VC10、VC11、VC12、VC15。
int logicalCellNumberFromPhysicalLabel(quint8 physicalLabel)
{
    constexpr std::array<quint8, 9> physicalLabels{1, 2, 5, 6, 7, 10, 11, 12, 15};

    for (int index = 0; index < 9; index++)
    {
        if (physicalLabels[index] == physicalLabel)
        {
            return index + 1;
        }
    }
    return 0;
}

QString ackResultToText(quint8 result)
{
    switch (result)
    {
        case 0x00U: return QStringLiteral("OK");
        case 0x01U: return QStringLiteral("UNKNOWN_CMD");
        case 0x02U: return QStringLiteral("INVALID_DLC");
        case 0x03U: return QStringLiteral("REJECTED");
        case 0x04U: return QStringLiteral("EXEC_FAIL");
        default: return QStringLiteral("UNKNOWN_RESULT");
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_canConnection(new CanConnection(this))
{
    ui->setupUi(this);

    // 启动时尚未收到真实单体电压，先显示“-- V”。
    updateCellVoltageDisplay();

    // 启动时尚未收到0x301，顶部总电压和电流先显示为未知。
    updatePackStatusDisplay();

    // 清除.ui里的状态、故障和均衡演示值，等待真实CAN帧。
    updateStatus304Display();
    updateFaultDisplay();
    updateBalanceDisplay();

    // 最高、最低和均衡状态以后根据真实数据计算，暂时隐藏演示标记。
    ui->labelCellBadge2->hide();
    ui->labelCellBadge4->hide();
    ui->labelCellBalance2->hide();

    // 下拉框显示友好文字，itemData 保存程序实际需要的参数值。
    ui->comboBitrate->setItemData(0, 500000);
    ui->comboBitrate->setItemData(1, 250000);
    ui->comboFrameType->setItemData(0, false); // 11 位标准帧
    ui->comboFrameType->setItemData(1, true);  // 29 位扩展帧

    // 命令下拉框显示说明文字，itemData保存实际发送的cmd字节。
    ui->comboCommand->setItemData(0, 0x01);
    ui->comboCommand->setItemData(1, 0x02);
    ui->comboCommand->setItemData(2, 0x03);

    connect(ui->buttonSendCommand,
            &QPushButton::clicked,
            this,
            &MainWindow::sendSelectedCommand);

    // 刷新按钮
    connect(ui->buttonRefreshDevices,
            &QPushButton::clicked,
            this,
            &MainWindow::refreshCanDevices);

    connect(ui->buttonConnect,
            &QPushButton::clicked,
            this,
            &MainWindow::toggleCanConnection);

    // 监听底层连接状态，设备意外掉线时界面也能恢复到未连接状态。
    connect(m_canConnection,
            &CanConnection::connectionStateChanged,
            this,
            &MainWindow::handleCanConnectionStateChanged);

    connect(m_canConnection,
            &CanConnection::errorOccurred,
            this,
            &MainWindow::handleCanError);

    // CanConnection每收到一帧，就把原始帧交给MainWindow。
    connect(m_canConnection,
            &CanConnection::frameReceived,
            this,
            &MainWindow::handleCanFrame);

    connect(ui->buttonClearLog,
            &QPushButton::clicked,
            ui->plainTextEditCommLog,
            &QPlainTextEdit::clear);

    // 启动时只初始化空表格，真实报文将在接收后逐行加入。
    initializeCanFrameTable();

    // 删除 .ui 中的演示日志；运行时只显示真实连接事件和 CAN 报文。
    ui->plainTextEditCommLog->clear();
    ui->plainTextEditCommLog->document()->setMaximumBlockCount(1000);

    // 暂停按钮需要保存按下状态：按下表示暂停，再次点击表示继续。
    ui->buttonPauseFrames->setCheckable(true);

    connect(ui->buttonPauseFrames,&QPushButton::toggled,this,[this](bool paused)
    {
        m_canFrameTablePaused = paused;
        // 通过按钮文字告诉用户下一次点击会执行什么操作。
        ui->buttonPauseFrames->setText(paused ? QStringLiteral("继续"): QStringLiteral("暂停"));
    });

    connect(ui->buttonClearFrames,&QPushButton::clicked,this,[this]()
    {
        // setRowCount(0)会真正删除所有数据行，但保留原来的表头。
        ui->tableCanFrames->setRowCount(0);
    });

    // 当前协议没有连续帧序号，无法可靠计算丢帧数量。
    ui->labelDropCount->setText(QStringLiteral("丢帧：不可判定"));
    updateBottomStatus();
    ui->labelRunState->setText(QStringLiteral("未连接"));
    ui->labelRunState->setStyleSheet(QStringLiteral("color:#667085; font-weight:700;"));

    auto *clockTimer = new QTimer(this);

    connect(clockTimer, &QTimer::timeout, this, [this]()
    {
        ui->labelCurrentTime->setText(
            QStringLiteral("当前时间：%1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss"))));
    });

    clockTimer->start(1000);
    ui->labelCurrentTime->setText(
        QStringLiteral("当前时间：%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss"))));

    updateConnectionControls(false);

    refreshCanDevices(); // 启动时自动扫描，刷新按钮用于后续 USB 热插拔。
}

MainWindow::~MainWindow()
{
    // ui 销毁前先切断状态回调并关闭 CAN，避免退出时回调访问已释放的控件。
    QObject::disconnect(m_canConnection, nullptr, this, nullptr);
    m_canConnection->disconnectDevice();
    delete ui;
}

void MainWindow::initializeCanFrameTable()
{
    // 固定紧凑行高，单屏至少保留7行；只在表格内部滚动。
    ui->tableCanFrames->verticalHeader()->setDefaultSectionSize(24);
    ui->tableCanFrames->verticalHeader()->setMinimumSectionSize(24);
    ui->tableCanFrames->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // 删除.ui文件或者上一次设计时残留的演示行。
    ui->tableCanFrames->setRowCount(0);

    // 时间、方向、CAN ID和DLC内容较短，按照内容自动调整宽度。
    ui->tableCanFrames->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);

    ui->tableCanFrames->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);

    ui->tableCanFrames->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);

    ui->tableCanFrames->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);

    // 数据和解析结果占用剩余空间。
    ui->tableCanFrames->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::Stretch);

    ui->tableCanFrames->horizontalHeader()->setSectionResizeMode(
        5, QHeaderView::Stretch);
}

void MainWindow::appendCanFrameToTable(const QCanBusFrame &frame,
                                       const QString &direction,
                                       const QString &parseResult)
{
    // 暂停时只停止更新原始报文表格，CAN接收和通信日志仍然正常工作。
    if (m_canFrameTablePaused)
    {
        return;
    }

    constexpr int maximumRowCount = 1000;

    // 表格达到1000行后，删除最早的一行，避免长期运行越来越卡。
    if (ui->tableCanFrames->rowCount() >= maximumRowCount)
    {
        ui->tableCanFrames->removeRow(0);
    }

    const int newRow = ui->tableCanFrames->rowCount();

    ui->tableCanFrames->insertRow(newRow);

    /*获取时间*/
    const QString timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));

    /*获取canid*/
    const QString canId = QStringLiteral("%1h") .arg(QString::number(frame.frameId(), 16).toUpper());

    /*获取数据*/
    const QString payload = QString::fromLatin1(frame.payload().toHex(' ').toUpper());

    QString frameTypeText;

    switch (frame.frameType())
    {
        case QCanBusFrame::DataFrame:
            frameTypeText = QStringLiteral("数据帧");
            break;

        case QCanBusFrame::RemoteRequestFrame:
            frameTypeText = QStringLiteral("远程帧");
            break;

        case QCanBusFrame::ErrorFrame:
            frameTypeText = QStringLiteral("错误帧");
            break;

        default:
            frameTypeText = QStringLiteral("其他帧");
            break;
    }

    // 按照.ui中的6列表头，依次准备每一列的内容。
    const QStringList columns
    {
        timestamp,
        direction,
        canId,
        QString::number(frame.payload().size()),
        payload,
        parseResult.isEmpty() ? frameTypeText : parseResult
    };

    for (int column = 0; column < columns.size(); column++)
    {
        auto *item = new QTableWidgetItem(columns.at(column));

        // 方向、CAN ID和DLC居中显示，表格看起来更整齐。
        if (column >= 1 && column <= 3)
        {
            item->setTextAlignment(Qt::AlignCenter);
        }

        ui->tableCanFrames->setItem(newRow, column, item);
    }

    // 新报文加入后，让表格显示最新一行。
    ui->tableCanFrames->scrollToBottom();
}

void MainWindow::refreshCanDevices()
{
    if (m_canConnection->isConnected()) {
        return;
    }

    QString errorString;

    const QString previousInterface = ui->comboCanDevice->currentData().toString();

    const auto devices = m_canConnection->scanDevices(&errorString);

    ui->comboCanDevice->clear();

    if (!errorString.isEmpty()) {
        ui->comboCanDevice->addItem(QStringLiteral("设备扫描失败"));
        ui->comboCanDevice->setEnabled(false);
        ui->buttonConnect->setEnabled(false);
        setConnectionStatus(
            QStringLiteral("● 扫描失败：%1").arg(errorString),
            QStringLiteral("#d92d36"));
        return;
    }

    if (devices.isEmpty()) {
        ui->comboCanDevice->addItem(QStringLiteral("未发现 PCAN 设备"));
        ui->comboCanDevice->setEnabled(false);
        ui->buttonConnect->setEnabled(false);
        setConnectionStatus(
            QStringLiteral("● 未发现 PCAN 设备"),
            QStringLiteral("#667085"));
        return;
    }

    // 每扫描到一台设备就增加一个下拉选项；两台设备会产生两个可选项。
    for (const auto &deviceInfo : devices) {
        const QString description = deviceInfo.description().isEmpty()
                                        ? QStringLiteral("PCAN 设备")
                                        : deviceInfo.description();
        const QString displayText =
            QStringLiteral("%1 (%2)")
                .arg(description, deviceInfo.name());

        // 显示文本用于阅读，userData 保存真正连接时使用的 usb0/usb1。
        ui->comboCanDevice->addItem(displayText, deviceInfo.name());
    }

    // 刷新后尽量保留用户先前选择的设备。
    const int previousIndex = ui->comboCanDevice->findData(previousInterface);
    if (previousIndex >= 0) {
        ui->comboCanDevice->setCurrentIndex(previousIndex);
    }

    ui->comboCanDevice->setEnabled(true);
    ui->buttonConnect->setEnabled(true);
    setConnectionStatus(
        QStringLiteral("● 已发现 %1 台设备，未连接").arg(devices.size()),
        QStringLiteral("#667085"));
}

void MainWindow::toggleCanConnection()
{
    // 同一个按钮根据当前状态执行“连接”或“断开”。
    if (m_canConnection->isConnected())
    {
        m_canConnection->disconnectDevice();
        updateConnectionControls(false);

        refreshCanDevices();
        return;
    }

    // 下拉框显示设备描述，userData 中保存真实接口名，例如 usb0。
    const QString interfaceName = ui->comboCanDevice->currentData().toString();

    // 下拉框显示 500 kbps，userData 中保存实际值 500000 bit/s。
    const int bitRate = ui->comboBitrate->currentData().toInt();

    if (interfaceName.isEmpty())
    {
        setConnectionStatus(
            QStringLiteral("● 请先刷新并选择 CAN 设备"),
            QStringLiteral("#d92d36"));
        return;
    }

    if (bitRate <= 0)
    {
        setConnectionStatus(
            QStringLiteral("● 波特率无效"),
            QStringLiteral("#d92d36"));
        return;
    }

    QString errorString;
    ui->buttonConnect->setEnabled(false);
    setConnectionStatus(QStringLiteral("● 正在连接 %1...").arg(interfaceName),QStringLiteral("#f07818"));

    // MainWindow 只提供用户选择，实际的 PeakCAN 打开过程由 CanConnection 完成。
    const bool connected = m_canConnection->connectDevice(interfaceName,bitRate,&errorString);

    if (!connected) {
        ui->buttonConnect->setEnabled(true);
        setConnectionStatus(
            QStringLiteral("● 连接失败：%1").arg(errorString),
            QStringLiteral("#d92d36"));
        return;
    }

    updateConnectionControls(true);
    ui->labelRunState->setText(QStringLiteral("正常"));
    ui->labelRunState->setStyleSheet(QStringLiteral("color:#168a45; font-weight:700;"));
    setConnectionStatus(
        QStringLiteral("● 已连接：%1").arg(interfaceName),
        QStringLiteral("#168a45"));

    appendCommunicationLog(
        QStringLiteral("PCAN %1 连接成功，%2 kbps，%3")
            .arg(interfaceName)
            .arg(bitRate / 1000)
            .arg(ui->comboFrameType->currentText()));
}

void MainWindow::updateConnectionControls(bool connected)
{
    // 连接期间锁定设备参数，防止界面选择与实际已打开通道不一致。
    ui->comboCanDevice->setEnabled(!connected);
    ui->comboBitrate->setEnabled(!connected);
    ui->comboFrameType->setEnabled(!connected);
    ui->buttonRefreshDevices->setEnabled(!connected);

    ui->buttonConnect->setText(
        connected ? QStringLiteral("■ 断开连接")
                  : QStringLiteral("▶ 连接设备"));
    ui->buttonConnect->setEnabled(true);

    // 命令只能在 CAN 通道已经打开时发送。
    ui->buttonSendCommand->setEnabled(connected);
}

void MainWindow::setConnectionStatus(const QString &text, const QString &color)
{
    ui->labelConnectionStatus->setText(text);
    ui->labelConnectionStatus->setToolTip(text);
    ui->labelConnectionStatus->setStyleSheet(
        QStringLiteral("color: %1; font-weight: 600; padding: 4px 0;")
            .arg(color));
}

void MainWindow::handleCanConnectionStateChanged(bool connected)
{
    updateConnectionControls(connected);

    ui->labelRunState->setText(
        connected ? QStringLiteral("正常") : QStringLiteral("未连接"));

    ui->labelRunState->setStyleSheet(
        connected ? QStringLiteral("color:#168a45; font-weight:700;")
                  : QStringLiteral("color:#667085; font-weight:700;"));

    if (!connected)
    {
        // 断开后清除实时快照，避免用户把上一轮数据误认为当前值。
        m_cellVoltageData = {};
        m_packStatusData = {};
        m_status304Data = {};
        m_faultData = {};
        m_balanceData = {};
        m_ackData = {};
        updateCellVoltageDisplay();
        updatePackStatusDisplay();
        updateStatus304Display();
        updateFaultDisplay();
        updateBalanceDisplay();
        ui->widgetCellBarChart->clearValues();

        setConnectionStatus(QStringLiteral("● 未连接"),QStringLiteral("#667085"));

        appendCommunicationLog(QStringLiteral("CAN 通道已断开"));
    }
}

void MainWindow::handleCanError(const QString &message)
{
    // 底层错误可能来自设备拔出、总线错误或驱动异常。
    setConnectionStatus(
        QStringLiteral("● CAN 错误：%1").arg(message),
        QStringLiteral("#d92d36"));
    appendCommunicationLog(
        QStringLiteral("CAN 错误：%1").arg(message));
    ui->labelRunState->setText(QStringLiteral("异常"));
    ui->labelRunState->setStyleSheet(QStringLiteral("color:#d92d36; font-weight:700;"));
}

void MainWindow::handleCanFrame(const QCanBusFrame &frame)
{

    m_receivedFrameCount++;

    updateBottomStatus();

    QString frameDescription;

    /*解析CanID*/
    const QString canId = QStringLiteral("%1h").arg(QString::number(frame.frameId(), 16).toUpper());

    /*解析数据*/
    const QString payload = QString::fromLatin1(frame.payload().toHex(' ').toUpper());

    switch (frame.frameType())
    {
        case QCanBusFrame::DataFrame:
            frameDescription = QStringLiteral("接收");
            break;

        case QCanBusFrame::RemoteRequestFrame:
            frameDescription = QStringLiteral("接收远程帧");
            break;

        case QCanBusFrame::ErrorFrame:
            frameDescription = QStringLiteral("接收错误帧");
            break;

        default:
            frameDescription = QStringLiteral("接收其他帧");
            break;
    }

    /*根据id*/
    QString parseResult = BmsCanProtocol::describeFrame(frame);

    /*判断canid的范围*/
    const bool knownBmsId = frame.frameId() >= 0x301U && frame.frameId() <= 0x307U;

    if (knownBmsId
        && (frame.frameType() != QCanBusFrame::DataFrame
            || frame.hasExtendedFrameFormat()
            || frame.payload().size() != 8))
    {
        m_parseErrorCount++;
        updateBottomStatus();
        parseResult = QStringLiteral("协议格式错误");
    }

    // 同一帧报文除了写入通信日志，也加入CAN原始报文表格。
    appendCanFrameToTable(frame, QStringLiteral("接收"), parseResult);

    // 只有当前帧属于单体电压报文时，才刷新9节电压显示。
    if (BmsCanProtocol::parseCellVoltages(frame, m_cellVoltageData))
    {
        updateCellVoltageDisplay();
    }

    // 收到0x301后，解析Pack总电压和有符号电流。
    if (BmsCanProtocol::parsePackStatus(frame, m_packStatusData))
    {
        updatePackStatusDisplay();
    }

    /**/
    if (BmsCanProtocol::parseStatus304(frame, m_status304Data))
    {
        updateStatus304Display();
    }

    if (BmsCanProtocol::parseFaultStatus(frame, m_faultData))
    {
        updateFaultDisplay();
    }

    if (BmsCanProtocol::parseBalanceStatus(frame, m_balanceData))
    {
        updateBalanceDisplay();
    }

    if (BmsCanProtocol::parseCommandAck(frame, m_ackData))
    {
        const QString sequenceState = m_ackData.sequence == m_commandSequence
                                          ? QStringLiteral("序号匹配")
                                          : QStringLiteral("序号不匹配");

        appendCommunicationLog(
            QStringLiteral("命令ACK：cmd=0x%1 seq=%2 result=%3 detail=0x%4 status=0x%5 fault=%6，%7")
                .arg(hexByte(m_ackData.command))
                .arg(m_ackData.sequence)
                .arg(ackResultToText(m_ackData.result))
                .arg(hexByte(m_ackData.detail))
                .arg(hexByte(m_ackData.statusFlags))
                .arg(m_ackData.faultType)
                .arg(sequenceState));
    }


}

void MainWindow::sendSelectedCommand()
{
    const int commandValue = ui->comboCommand->currentData().toInt();
    if (commandValue < 0x01 || commandValue > 0x03)
    {
        appendCommunicationLog(QStringLiteral("发送失败：命令值无效"));
        return;
    }

    // 0作为初始未发送状态，命令序号从1开始，溢出后跳过0。
    m_commandSequence++;

    if (m_commandSequence == 0U)
    {
        m_commandSequence = 1U;
    }

    QByteArray payload(8, char(0));
    payload[0] = static_cast<char>(commandValue);
    payload[1] = static_cast<char>(m_commandSequence);

    QCanBusFrame frame(0x401U, payload);
    frame.setExtendedFrameFormat(ui->comboFrameType->currentData().toBool());

    QString errorString;
    if (!m_canConnection->sendFrame(frame, &errorString))
    {
        appendCommunicationLog(
            QStringLiteral("命令发送失败：%1").arg(errorString));
        return;
    }

    m_transmittedFrameCount++;
    updateBottomStatus();

    appendCanFrameToTable(frame,QStringLiteral("发送"),BmsCanProtocol::describeFrame(frame));

    appendCommunicationLog(
        QStringLiteral("发送：401h  cmd=0x%1  seq=%2  DATA=%3")
            .arg(hexByte(static_cast<quint8>(commandValue)))
            .arg(m_commandSequence)
            .arg(QString::fromLatin1(payload.toHex(' ').toUpper())));
}

void MainWindow::appendCommunicationLog(const QString &message)
{
    const QString timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));

    ui->plainTextEditCommLog->appendPlainText(QStringLiteral("%1  %2").arg(timestamp, message));

    // 取消“自动滚动”后仍然记录报文，只是不强制跳到日志末尾。
    if (ui->checkAutoScrollLog->isChecked())
    {
        QScrollBar *scrollBar = ui->plainTextEditCommLog->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void MainWindow::updateCellVoltageDisplay()
{
    // 9个单体名称标签，与数组中的Cell1～Cell9一一对应。
    const std::array<QLabel *, 9> nameLabels
    {
        ui->labelCellName1,
        ui->labelCellName2,
        ui->labelCellName3,
        ui->labelCellName4,
        ui->labelCellName5,
        ui->labelCellName6,
        ui->labelCellName7,
        ui->labelCellName8,
        ui->labelCellName9
    };

    // 9个单体电压标签，用于显示解析后的真实电压。
    const std::array<QLabel *, 9> voltageLabels
    {
        ui->labelCellVoltage1,
        ui->labelCellVoltage2,
        ui->labelCellVoltage3,
        ui->labelCellVoltage4,
        ui->labelCellVoltage5,
        ui->labelCellVoltage6,
        ui->labelCellVoltage7,
        ui->labelCellVoltage8,
        ui->labelCellVoltage9
    };

    // 9个单体卡片，用于动态标记最高和最低单体。
    const std::array<QFrame *, 9> cellFrames
    {
        ui->frameCell1,
        ui->frameCell2,
        ui->frameCell3,
        ui->frameCell4,
        ui->frameCell5,
        ui->frameCell6,
        ui->frameCell7,
        ui->frameCell8,
        ui->frameCell9
    };

    bool allCellVoltagesValid = true;

    // 第一步：先更新9节电压的文字。
    for (int index = 0; index < 9; ++index)
    {
        nameLabels[index]->setText(QStringLiteral("C%1").arg(index + 1));

        if (!m_cellVoltageData.valid[index])
        {
            voltageLabels[index]->setText(QStringLiteral("-- V"));
            allCellVoltagesValid = false;
            continue;
        }

        const double voltageVolts =  m_cellVoltageData.millivolts[index] / 1000.0;

        voltageLabels[index]->setText(QStringLiteral("%1 V").arg(voltageVolts,0,'f',3));
    }

    // 必须等0x302、0x303、0x304全部到达后，才能计算9节统计值。
    if (!allCellVoltagesValid)
    {
        ui->labelCellDiff->setText(QStringLiteral("-- mV"));
        ui->labelCellExtrema->setText(QStringLiteral("最高 --    最低 --"));

        // 一致性提示统一交给专门函数更新。
        updateVoltageConsistencyDisplay();

        // 数据不完整时，所有卡片都恢复为普通状态。
        for (int index = 0; index < 9; ++index)
        {
            cellFrames[index]->setProperty("voltageState",QStringLiteral("normal"));

            voltageLabels[index]->setProperty("voltageState",QStringLiteral("normal"));

            // 动态属性变化后，通知Qt重新应用样式。
            cellFrames[index]->style()->unpolish(cellFrames[index]);
            cellFrames[index]->style()->polish(cellFrames[index]);

            voltageLabels[index]->style()->unpolish(voltageLabels[index]);
            voltageLabels[index]->style()->polish(voltageLabels[index]);
        }

        return;
    }


    // 第二步：9节数据完整后，计算最大值、最小值和总和。
    int highestCellIndex = 0;
    int lowestCellIndex = 0;
    quint32 cellVoltageSumMv = 0;

    for (int index = 0; index < 9; ++index)
    {
        const quint16 voltageMv = m_cellVoltageData.millivolts[index];

        /*计算总电压*/
        cellVoltageSumMv += voltageMv;

        if (voltageMv > m_cellVoltageData.millivolts[highestCellIndex])
        {
            highestCellIndex = index;
        }

        if (voltageMv < m_cellVoltageData.millivolts[lowestCellIndex])
        {
            lowestCellIndex = index;
        }
    }

    /*获取压差*/
    const quint16 cellVoltageDifferenceMv =
        static_cast<quint16>(
            m_cellVoltageData.millivolts[highestCellIndex]
          - m_cellVoltageData.millivolts[lowestCellIndex]);

    // 第三步：显示真实单体压差。
    ui->labelCellDiff->setText(QStringLiteral("%1 mV").arg(cellVoltageDifferenceMv));
    ui->labelCellExtrema->setText(
        QStringLiteral("最高 C%1 %2 V    最低 C%3 %4 V")
            .arg(highestCellIndex + 1)
            .arg(m_cellVoltageData.millivolts[highestCellIndex] / 1000.0, 0, 'f', 3)
            .arg(lowestCellIndex + 1)
            .arg(m_cellVoltageData.millivolts[lowestCellIndex] / 1000.0, 0, 'f', 3));

    // 第四步：根据真实结果标记最高和最低单体。
    for (int index = 0; index < 9; ++index)
    {
        QString voltageState = QStringLiteral("normal");

        if (index == highestCellIndex)
        {
            voltageState = QStringLiteral("highest");
            nameLabels[index]->setText(QStringLiteral("C%1 · 最高").arg(index + 1));
        }
        else if (index == lowestCellIndex)
        {
            voltageState = QStringLiteral("lowest");

            nameLabels[index]->setText(QStringLiteral("C%1 · 最低").arg(index + 1));
        }

        cellFrames[index]->setProperty("voltageState",voltageState);

        voltageLabels[index]->setProperty("voltageState",voltageState);

        // setProperty以后主动刷新样式，让边框和文字颜色立即变化。
        cellFrames[index]->style()->unpolish(cellFrames[index]);
        cellFrames[index]->style()->polish(cellFrames[index]);
        cellFrames[index]->update();

        voltageLabels[index]->style()->unpolish(voltageLabels[index]);
        voltageLabels[index]->style()->polish(voltageLabels[index]);
        voltageLabels[index]->update();
    }

    QVector<double> chartValues;
    chartValues.reserve(9);
    for (quint16 voltageMv : m_cellVoltageData.millivolts)
    {
        chartValues.append(voltageMv / 1000.0);
    }

    // 柱状图属于当前快照显示，不是用户暂缓实现的历史趋势曲线。
    ui->widgetCellBarChart->setValues(chartValues);

    // 单体电压变化后，重新比较单体总和与Pack电压。
    updateVoltageConsistencyDisplay();
}

void MainWindow::updatePackStatusDisplay()
{
    if (!m_packStatusData.valid)
    {
        // 还没收到0x301时，不显示虚假的0值。
        ui->labelPackVoltage->setText(QStringLiteral("-- V"));
        ui->labelPackCurrent->setText(QStringLiteral("-- A"));

        updateVoltageConsistencyDisplay();
        return;
    }

    const double packVoltageVolts = m_packStatusData.voltageMillivolts / 1000.0;

    const double packCurrentAmps = m_packStatusData.currentMilliamps / 1000.0;

    // 协议单位为mV和mA，界面转换成V和A。
    ui->labelPackVoltage->setText(QStringLiteral("%1 V").arg(packVoltageVolts, 0, 'f', 3));

    // currentMilliamps是有符号数，负号会被保留下来。
    ui->labelPackCurrent->setText(QStringLiteral("%1 A").arg(packCurrentAmps, 0, 'f', 3));

    updateVoltageConsistencyDisplay();
}

void MainWindow::updateVoltageConsistencyDisplay()
{
    quint32 cellVoltageSumMv = 0;

    // 只有9节单体全部收到后，单体总和才有意义。
    for (int index = 0; index < 9; ++index)
    {
        if (!m_cellVoltageData.valid[index])
        {
            ui->labelQualityText->setText( QStringLiteral("正在等待完整的9节单体电压"));
            return;
        }

        cellVoltageSumMv +=  m_cellVoltageData.millivolts[index];
    }

    // 单体电压已经完整，但0x301还没有到达。
    if (!m_packStatusData.valid)
    {
        ui->labelQualityText->setText(QStringLiteral("单体总和 %1 V，Pack电压待解析").arg(cellVoltageSumMv / 1000.0, 0, 'f', 3));
        return;
    }

    // 使用有符号64位数做减法，防止无符号数相减发生下溢。
    const qint64 signedDifferenceMv =
        static_cast<qint64>(m_packStatusData.voltageMillivolts)
        - static_cast<qint64>(cellVoltageSumMv);

    const quint64 absoluteDifferenceMv =
        signedDifferenceMv >= 0
            ? static_cast<quint64>(signedDifferenceMv)
            : static_cast<quint64>(-signedDifferenceMv);

    // 显示九节单体总和，以及它与0x301 Pack电压的绝对差值。
    ui->labelQualityText->setText(
        QStringLiteral("单体和 %1 V · 总压差异 %2 mV")
            .arg(cellVoltageSumMv / 1000.0, 0, 'f', 3)
            .arg(absoluteDifferenceMv));
}

void MainWindow::updateStatus304Display()
{
    if (!m_status304Data.valid)
    {
        ui->labelTemperature->setText(QStringLiteral("-- °C"));
        ui->labelAlarmStatus->setText(QStringLiteral("--"));
        ui->labelAlarmDetail->setText(QStringLiteral("等待0x304"));
        ui->labelAlarmFlags->setText(QStringLiteral("Alarm --"));
        ui->labelProtectFlags->setText(QStringLiteral("Protect --"));
        ui->labelCurrentDirection->setText(QStringLiteral("方向 --"));
        ui->labelStatusTemperature->setText(QStringLiteral("温度 -- °C"));
        ui->labelAlarmFlags->setToolTip(QString());
        ui->labelProtectFlags->setToolTip(QString());
        ui->labelAlarmStatus->setToolTip(QString());
        return;
    }

    /*计算温度*/
    const double temperatureC = m_status304Data.temperatureDeciC / 10.0;

    const QString alarmText = alarmFlagsToText(m_status304Data.alarmFlags);

    /**/
    const QString protectText = protectFlagsToText(m_status304Data.protectFlags);

    ui->labelTemperature->setText(QStringLiteral("%1 °C").arg(temperatureC, 0, 'f', 1));

    if (m_status304Data.alarmFlags == 0U)
    {
        ui->labelAlarmStatus->setText(QStringLiteral("正常"));
        ui->labelAlarmDetail->setText(QStringLiteral("无软件告警"));
        ui->labelAlarmStatus->setStyleSheet(QStringLiteral("color:#168a45;"));
    }
    else
    {
        ui->labelAlarmStatus->setText(alarmText.size() > 9 ? QStringLiteral("多项告警") : alarmText);
        ui->labelAlarmDetail->setText(QStringLiteral("存在软件告警"));
        ui->labelAlarmStatus->setStyleSheet(QStringLiteral("color:#d92d36;"));
    }

    ui->labelAlarmFlags->setText(QStringLiteral("Alarm 0x%1").arg(hexByte(m_status304Data.alarmFlags)));
    ui->labelProtectFlags->setText(QStringLiteral("Protect 0x%1").arg(hexByte(m_status304Data.protectFlags)));
    ui->labelAlarmFlags->setToolTip(alarmText);
    ui->labelProtectFlags->setToolTip(protectText);
    ui->labelAlarmStatus->setToolTip(alarmText);

    ui->labelCurrentDirection->setText(
        QStringLiteral("方向 %1（%2）")
            .arg(static_cast<int>(m_status304Data.currentDirection))
            .arg(currentDirectionToText(m_status304Data.currentDirection)));
    ui->labelStatusTemperature->setText(QStringLiteral("温度 %1 °C").arg(temperatureC, 0, 'f', 1));
}

void MainWindow::updateFaultDisplay()
{
    ui->labelFaultSummary->setToolTip(QString());
    ui->labelRuntimeFault->setText(QStringLiteral("Runtime 无"));
    ui->labelHwFault->setText(QStringLiteral("HwFault 无"));
    ui->labelBringUpFault->setText(QStringLiteral("BringUp 无"));
    ui->labelRuntimeFault->setToolTip(QString());
    ui->labelHwFault->setToolTip(QString());
    ui->labelBringUpFault->setToolTip(QString());

    if (!m_faultData.valid)
    {
        ui->labelFaultSummary->setText(QStringLiteral("等待0x305"));
        ui->labelFaultSummary->setStyleSheet(QStringLiteral("color:#667085;"));
        return;
    }

    if (m_faultData.type == 0U)
    {
        ui->labelFaultSummary->setText(QStringLiteral("无故障"));
        ui->labelFaultSummary->setStyleSheet(QStringLiteral("color:#168a45;"));
        if (m_canConnection->isConnected())
        {
            ui->labelRunState->setText(QStringLiteral("正常"));
            ui->labelRunState->setStyleSheet(QStringLiteral("color:#168a45; font-weight:700;"));
        }
        return;
    }

    ui->labelFaultSummary->setStyleSheet(QStringLiteral("color:#d92d36;"));
    ui->labelRunState->setText(QStringLiteral("故障"));
    ui->labelRunState->setStyleSheet(QStringLiteral("color:#d92d36; font-weight:700;"));

    switch (m_faultData.type)
    {
        case 1U:
            ui->labelFaultSummary->setText(QStringLiteral("BringUp故障"));
            ui->labelBringUpFault->setText(
                QStringLiteral("●  BringUp  main=0x%1 stage=0x%2 error=0x%3")
                    .arg(hexByte(m_faultData.detail[0]),
                         hexByte(m_faultData.detail[1]),
                         hexByte(m_faultData.detail[2])));
            break;

        case 2U:
        {
            const quint16 sampleFailCount =
                static_cast<quint16>(m_faultData.detail[5]
                                     | (static_cast<quint16>(m_faultData.detail[6]) << 8));
            ui->labelFaultSummary->setText(QStringLiteral("Runtime故障"));
            ui->labelRuntimeFault->setText(
                QStringLiteral("●  Runtime  code=0x%1 stage=0x%2 fail=%3")
                    .arg(hexByte(m_faultData.detail[0]),
                         hexByte(m_faultData.detail[1]))
                    .arg(sampleFailCount));
            break;
        }

        case 3U:
        {
            const quint16 faultCount =
                static_cast<quint16>(m_faultData.detail[5]
                                     | (static_cast<quint16>(m_faultData.detail[6]) << 8));
            ui->labelFaultSummary->setText(QStringLiteral("硬件故障"));
            ui->labelHwFault->setText(
                QStringLiteral("●  HwFault  code=0x%1 SYS_STAT=0x%2 count=%3")
                    .arg(hexByte(m_faultData.detail[0]),
                         hexByte(m_faultData.detail[1]))
                    .arg(faultCount));
            break;
        }

        case 4U:
            // 固件实际还会上报type=4，表示RTOS初始化失败。
            ui->labelFaultSummary->setText(QStringLiteral("RTOS初始化故障"));
            ui->labelBringUpFault->setText(
                QStringLiteral("●  RTOS Init  error=0x%1 safeOff=0x%2")
                    .arg(hexByte(m_faultData.detail[0]),
                         hexByte(m_faultData.detail[2])));
            break;

        default:
            ui->labelFaultSummary->setText(
                QStringLiteral("未知故障类型 0x%1").arg(hexByte(m_faultData.type)));
            break;
    }

    // 紧凑卡片显示故障类别；悬停仍可读取完整的 code/stage/count。
    for (QLabel *label : {ui->labelRuntimeFault, ui->labelHwFault, ui->labelBringUpFault})
    {
        const QString detail = label->text();
        if (detail.contains(QLatin1Char('=')))
        {
            label->setToolTip(detail);
            ui->labelFaultSummary->setToolTip(detail);
            label->setText(label == ui->labelRuntimeFault ? QStringLiteral("Runtime 故障（详情）")
                           : label == ui->labelHwFault ? QStringLiteral("HwFault 故障（详情）")
                                                      : QStringLiteral("启动故障（详情）"));
        }
    }
}

void MainWindow::updateBalanceDisplay()
{
    if (!m_balanceData.valid)
    {
        ui->labelBalanceStatus->setText(QStringLiteral("--"));
        ui->labelBalanceTargetSummary->setText(QStringLiteral("目标：--"));
        ui->labelBalanceActive->setText(QStringLiteral("--"));
        ui->labelBalanceTargetCount->setText(QStringLiteral("--"));
        ui->labelBalanceTarget->setText(QStringLiteral("--"));
        ui->labelCellBal1->setText(QStringLiteral("--"));
        ui->labelCellBal2->setText(QStringLiteral("--"));
        ui->labelCellBal3->setText(QStringLiteral("--"));
        ui->labelBalancePhase->setText(QStringLiteral("--"));
        return;
    }

    const QString stateText = m_balanceData.active? QStringLiteral("Active"): QStringLiteral("Inactive");

    const int logicalCell = logicalCellNumberFromPhysicalLabel(m_balanceData.targetLabel);

    const QString targetText = logicalCell > 0 ? QStringLiteral("Cell %1").arg(logicalCell) : QStringLiteral("--");

    ui->labelBalanceStatus->setText(stateText);
    ui->labelBalanceActive->setText(stateText);

    ui->labelBalanceTargetCount->setText(QString::number(m_balanceData.targetCount));
    ui->labelBalanceTarget->setText(targetText);

    ui->labelBalanceTargetSummary->setText(QStringLiteral("目标：%1").arg(targetText));

    ui->labelCellBal1->setText(QStringLiteral("0x%1").arg(hexByte(m_balanceData.cellBal1)));
    ui->labelCellBal2->setText(QStringLiteral("0x%1").arg(hexByte(m_balanceData.cellBal2)));
    ui->labelCellBal3->setText(QStringLiteral("0x%1").arg(hexByte(m_balanceData.cellBal3)));

    ui->labelBalancePhase->setText(QString::number(m_balanceData.parityPhase));

    const QString color = m_balanceData.active? QStringLiteral("#168a45"): QStringLiteral("#667085");

    ui->labelBalanceStatus->setStyleSheet(QStringLiteral("color:%1;").arg(color));
    ui->labelBalanceActive->setStyleSheet(QStringLiteral("color:%1;").arg(color));
}

void MainWindow::updateBottomStatus()
{
    ui->labelRxCount->setText(QStringLiteral("接收：%1").arg(m_receivedFrameCount));

    ui->labelTxCount->setText(QStringLiteral("发送：%1").arg(m_transmittedFrameCount));

    ui->labelParseErrorCount->setText(QStringLiteral("解析错误：%1").arg(m_parseErrorCount));
}
