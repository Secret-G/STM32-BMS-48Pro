#ifndef CHARTWIDGETS_H
#define CHARTWIDGETS_H

#include <QVector>
#include <QWidget>

class VoltageBarChart final : public QWidget
{
public:
    explicit VoltageBarChart(QWidget *parent = nullptr);

    // 使用9节真实单体电压刷新柱状图，单位为V。
    void setValues(const QVector<double> &values);
    void clearValues();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_values;
    bool m_hasValues = false;
};

class TrendChart final : public QWidget
{
public:
    explicit TrendChart(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // CHARTWIDGETS_H
