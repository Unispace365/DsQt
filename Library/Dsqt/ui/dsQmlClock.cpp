#include "ui/dsQmlClock.h"

#include <QPainterPath>
#include <QtQuickShapes/QtQuickShapes>

namespace dsqt::ui {

DsQmlClock::DsQmlClock(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_timer(this) {
    m_local_date_time = m_last_update = QDateTime::currentDateTime();

    connect(&m_timer, &QTimer::timeout, this, &DsQmlClock::updateTime);
    m_timer.start(10);

    // Trigger repaints on time changes
    connect(this, &DsQmlClock::secondsChanged, this, [this]() { update(); });

    // Enable antialiasing for smoother paths
    setAntialiasing(true);
}

void DsQmlClock::paint(QPainter* painter) {
    // Skip painting if no size is specified (default is 0x0)
    if (width() <= 0 || height() <= 0) return;

    painter->save();
    {
        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        // Scale to the smaller dimension for a circular clock
        qreal side = qMin(width(), height());
        painter->translate(width() / 2, height() / 2);
        painter->scale(side / 200.0, side / 200.0); // Base radius of 100 for scaling

        // Hour marks
        QPainterPath hourMarkPath;
        for (int i = 0; i < 12; ++i) {
            hourMarkPath.moveTo(88, 0);
            hourMarkPath.lineTo(96, 0);
            painter->save();
            painter->rotate(30.0 * i); // 360/12 = 30 degrees per hour
            painter->setPen(QPen(m_hour_mark_color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(hourMarkPath);
            painter->restore();
        }

        // Get current time for hands
        double time = secondsSinceMidnight();

        // Hour hand
        painter->save();
        QPainterPath hourHandPath;
        hourHandPath.moveTo(0, 10);
        hourHandPath.lineTo(0, -70);
        painter->setPen(QPen(m_hour_hand_color, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->rotate(30.0 * time / 3600.0);
        painter->drawPath(hourHandPath);
        painter->restore();

        // Minute hand
        painter->save();
        QPainterPath minuteHandPath;
        minuteHandPath.moveTo(0, 10);
        minuteHandPath.lineTo(0, -90);
        painter->setPen(QPen(m_minute_hand_color, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->rotate(360.0 * time / 3600.0);
        painter->drawPath(minuteHandPath);
        painter->restore();

        // Second hand
        painter->save();
        QPainterPath secondHandPath;
        secondHandPath.moveTo(0, 10);
        secondHandPath.lineTo(0, -95);
        painter->setPen(QPen(m_second_hand_color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->rotate(360.0 * time / 60.0);
        painter->drawPath(secondHandPath);
        painter->restore();
    }
    painter->restore();
}

void DsQmlClock::updateTime() {
    const auto currentDateTime = QDateTime::currentDateTime();
    const auto lastDateTime    = m_local_date_time;
    const int  lastSeconds     = QTime(0, 0).secsTo(m_local_date_time.time());

    auto elapsedMilliseconds = std::chrono::milliseconds(m_last_update.msecsTo(currentDateTime));
    m_local_date_time        = lastDateTime.addDuration(elapsedMilliseconds * m_speed);
    m_last_update            = currentDateTime;

    const int currentSeconds = QTime(0, 0).secsTo(m_local_date_time.time());
    if (currentSeconds != lastSeconds) emit secondsChanged();
    if (currentSeconds / 60 != lastSeconds / 60) emit minutesChanged();
    if (currentSeconds / 3600 != lastSeconds / 3600) emit hoursChanged();
    if (lastDateTime.date() != m_local_date_time.date()) emit dateChanged();
}

} // namespace dsqt::ui
