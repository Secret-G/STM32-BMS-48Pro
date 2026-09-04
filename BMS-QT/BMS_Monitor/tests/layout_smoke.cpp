#include "ui_mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QHeaderView>
#include <QScrollArea>
#include <array>

int main(int argc, char **argv)
{
    for(int i=1;i<argc;++i)
        if(QByteArray(argv[i])=="--hidpi") qputenv("QT_SCALE_FACTOR","1.25");
    QApplication app(argc, argv);
    // The offscreen Windows backend does not enumerate installed system fonts.
#ifdef Q_OS_WIN
    const QString fonts = qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts/");
    for (const QString &file : {QStringLiteral("msyh.ttc"), QStringLiteral("msyhbd.ttc"), QStringLiteral("segoeui.ttf"), QStringLiteral("consola.ttf")})
        QFontDatabase::addApplicationFont(fonts + file);
#endif
    QMainWindow window;
    Ui::MainWindow ui;
    ui.setupUi(&window);
    const QVector<double> cells{3.826,3.833,3.782,3.694,3.826,3.750,3.775,3.799,3.790};
    ui.widgetCellBarChart->setValues(cells);
    ui.labelCellBadge2->hide(); ui.labelCellBadge4->hide(); ui.labelCellBalance2->hide();
    ui.labelCellExtrema->setText(QStringLiteral("最高 C2 3.833 V    最低 C4 3.694 V"));
    ui.labelAlarmFlags->setText(QStringLiteral("Alarm 0x04"));
    ui.labelProtectFlags->setText(QStringLiteral("Protect 0x20"));
    ui.labelCurrentDirection->setText(QStringLiteral("方向 0（静止）"));
    ui.labelStatusTemperature->setText(QStringLiteral("温度 24.0 °C"));
    ui.labelQualityText->setText(QStringLiteral("单体和 34.075 V · 总压差异 17 mV"));
    ui.labelRuntimeFault->setText(QStringLiteral("Runtime 无"));
    ui.labelHwFault->setText(QStringLiteral("HwFault 无"));
    ui.labelBringUpFault->setText(QStringLiteral("BringUp 无"));
    ui.labelConnectionStatus->setText(QStringLiteral("未连接硬件 · 布局预览"));
    ui.comboCanDevice->clear();
    ui.comboCanDevice->addItem(QStringLiteral("PCAN-USB（预览）"));
    ui.labelRunState->setText(QStringLiteral("布局预览"));
    ui.plainTextEditCommLog->setPlainText(QStringLiteral("当前为离屏布局测试\n所有数值为测试数据\n未连接 CAN 设备\n\n已检查：\n· 9节单体同排显示\n· 图表并排\n· 原始报文固定可见\n· 仅表内滚动"));
    ui.frameCell2->setProperty("voltageState",QStringLiteral("highest"));
    ui.frameCell4->setProperty("voltageState",QStringLiteral("lowest"));
    ui.labelCellVoltage2->setProperty("voltageState",QStringLiteral("highest"));
    ui.labelCellVoltage4->setProperty("voltageState",QStringLiteral("lowest"));
    ui.labelCellName2->setText(QStringLiteral("C2 · 最高"));
    ui.labelCellName4->setText(QStringLiteral("C4 · 最低"));
    ui.tableCanFrames->verticalHeader()->setDefaultSectionSize(24);
    ui.tableCanFrames->verticalHeader()->setMinimumSectionSize(24);
    ui.tableCanFrames->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui.tableCanFrames->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.tableCanFrames->setRowCount(7);
    const QStringList bytes{QStringLiteral("0A 85 00 00 00 00 00 00"),QStringLiteral("F2 0E F9 0E C6 0E 6E 0E"),QStringLiteral("F2 0E A6 0E BF 0E D7 0E"),QStringLiteral("CE 0E F0 00 04 20 02 00"),QStringLiteral("00 00 00 00 00 00 00 00"),QStringLiteral("01 01 02 00 00 01 02 00"),QStringLiteral("02 02 00 00 04 00 00 00")};
    for(int row=0; row<7; ++row) {
        const QStringList fields{QStringLiteral("14:22:31.789"),QStringLiteral("RX"),QString::number(0x301+row,16),QStringLiteral("8"),bytes[row],QStringLiteral("状态帧")};
        for(int col=0; col<6; ++col) ui.tableCanFrames->setItem(row,col,new QTableWidgetItem(fields[col]));
    }
    for(int col=0;col<4;++col) ui.tableCanFrames->horizontalHeader()->setSectionResizeMode(col,QHeaderView::ResizeToContents);
    const QString screenshotDir = app.arguments().size()>1 && app.arguments().at(1)!=QStringLiteral("--hidpi")
        ? app.arguments().at(1) : QString();
    window.show();
    bool passed=true;
    for(const QSize size : {QSize(1440,820),QSize(1366,768),QSize(1280,720)}) {
        window.resize(size);
        app.processEvents(); app.processEvents();
        const QRect bounds=window.centralWidget()->rect();
        auto inside=[&](QWidget *w) {return bounds.contains(QRect(w->mapTo(window.centralWidget(),QPoint()),w->size()));};
        bool fits=window.size()==size && inside(ui.frameCanTable) && inside(ui.frameCells)
            && inside(ui.frameTrend) && inside(ui.frameBottomStatus)
            && window.findChildren<QScrollArea *>().isEmpty();
        const int rows=ui.tableCanFrames->viewport()->height()/ui.tableCanFrames->rowHeight(0);
        fits = fits && rows>=7;
        fits = fits && ui.labelQualityText->contentsRect().width() >= ui.labelQualityText->fontMetrics().horizontalAdvance(ui.labelQualityText->text());
        const std::array<QLabel *,9> voltages{ui.labelCellVoltage1,ui.labelCellVoltage2,ui.labelCellVoltage3,ui.labelCellVoltage4,ui.labelCellVoltage5,ui.labelCellVoltage6,ui.labelCellVoltage7,ui.labelCellVoltage8,ui.labelCellVoltage9};
        const int y=voltages[0]->mapTo(&window,QPoint()).y();
        for(QLabel *label:voltages) {
            const bool aligned=qAbs(label->mapTo(&window,QPoint()).y()-y)<=2;
            const bool readable=label->contentsRect().width()>=label->fontMetrics().horizontalAdvance(label->text());
            if(!aligned || !readable) qWarning()<<label->objectName()<<"aligned"<<aligned<<"readable"<<readable<<label->geometry()<<label->fontMetrics().horizontalAdvance(label->text());
            fits = fits && aligned && readable;
        }
        qInfo()<<"quality"<<ui.labelQualityText->geometry()<<"cards"<<ui.frameCells->size()<<"charts"<<ui.frameTrend->size()<<"details"<<ui.frameStatusInfo->size();
        qInfo() << size << "actual" << window.size() << "CAN visible rows" << rows << (fits?"PASS":"FAIL");
        passed=passed&&fits;
        if(!screenshotDir.isEmpty()) window.grab().save(QDir(screenshotDir).filePath(QStringLiteral("layout-%1x%2.png").arg(size.width()).arg(size.height())));
    }
    return passed ? 0 : 1;
}
