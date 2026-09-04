#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("BMS Monitor Pro"));
    QApplication::setOrganizationName(QStringLiteral("Evan BMS"));
    a.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    MainWindow w;
    w.show();
    return QApplication::exec();
}
