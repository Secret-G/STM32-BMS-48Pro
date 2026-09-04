#include "chartwidgets.h"

#include <algorithm>
#include <iterator>
#include <QLinearGradient>
#include <QPainter>
#include <QtMath>

VoltageBarChart::VoltageBarChart(QWidget *parent)
    : QWidget(parent),
      m_values(9, 0.0)
{
    setMinimumHeight(96);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VoltageBarChart::setValues(const QVector<double> &values)
{
    if (values.size() != 9)
    {
        return;
    }

    m_values = values;
    m_hasValues = true;
    update();
}

void VoltageBarChart::clearValues()
{
    m_hasValues = false;
    update();
}

void VoltageBarChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (!m_hasValues)
    {
        p.setPen(QColor("#98a2b3"));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待9节单体电压"));
        return;
    }

    const QRectF plot = rect().adjusted(42, 24, -12, -26);
    const auto minIt = std::min_element(m_values.cbegin(), m_values.cend());
    const auto maxIt = std::max_element(m_values.cbegin(), m_values.cend());
    const int lowestIndex = static_cast<int>(std::distance(m_values.cbegin(), minIt));
    const int highestIndex = static_cast<int>(std::distance(m_values.cbegin(), maxIt));

    // 固定纵轴范围，避免实时电压变化时坐标轴自动缩放造成视觉跳动。
    constexpr double minV = 1.5;
    constexpr double maxV = 4.3;
    p.fillRect(plot, QColor("#fffaf0"));

    QFont axisFont = font();
    axisFont.setPointSize(8);
    p.setFont(axisFont);
    for (int i = 0; i <= 4; ++i) {
        const double value = minV + i * (maxV - minV) / 4.0;
        const qreal y = plot.bottom() - (value - minV) / (maxV - minV) * plot.height();
        p.setPen(QPen(QColor("#dfe4ea"), 1, Qt::DashLine));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(QColor("#667085"));
        p.drawText(QRectF(2, y - 9, 34, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(value, 'f', 1));
    }

    p.setPen(QColor("#344054"));
    p.drawText(QRectF(4, 4, 70, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("电压 (V)"));
    const qreal slot = plot.width() / m_values.size();
    const qreal barWidth = qMin<qreal>(42.0, slot * 0.60);
    for (int i = 0; i < m_values.size(); ++i) {
        const double value = m_values.at(i);
        // 超出显示范围的数据贴到图表边界，但上方仍显示真实数值。
        const double plottedValue = qBound(minV, value, maxV);
        const qreal x = plot.left() + slot * i + (slot - barWidth) / 2.0;
        const qreal y = plot.bottom() - (plottedValue - minV) / (maxV - minV) * plot.height();
        QColor color("#34a853");
        if (i == highestIndex) color = QColor("#1677ff");
        if (i == lowestIndex) color = QColor("#e5484d");

        QLinearGradient gradient(x, y, x, plot.bottom());
        gradient.setColorAt(0.0, color.lighter(108));
        gradient.setColorAt(1.0, color.darker(108));
        p.setPen(Qt::NoPen);
        p.setBrush(gradient);
        p.drawRoundedRect(QRectF(x, y, barWidth, plot.bottom() - y), 3, 3);
        p.setPen(QColor("#344054"));
        p.drawText(QRectF(x - 8, y - 23, barWidth + 16, 20), Qt::AlignCenter,
                   QString::number(value, 'f', 3));
        p.drawText(QRectF(x - 8, plot.bottom() + 7, barWidth + 16, 20), Qt::AlignCenter,
                   QStringLiteral("C%1").arg(i + 1));
    }
}

TrendChart::TrendChart(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(96);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TrendChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor("#98a2b3"));
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("趋势曲线暂未接入"));
}
