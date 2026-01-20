#ifndef DSQMLCLOCK_H
#define DSQMLCLOCK_H

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>

#include <qpainter.h>

namespace dsqt::ui {

class DsQmlClock : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsClock)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(QDateTime now READ now NOTIFY secondsChanged)
    Q_PROPERTY(QDate date READ date NOTIFY dateChanged)
    Q_PROPERTY(QString dateYMD READ dateYMD NOTIFY dateChanged)
    Q_PROPERTY(QString dateFull READ dateFull NOTIFY dateChanged)
    Q_PROPERTY(QTime time READ time NOTIFY secondsChanged)
    Q_PROPERTY(QString timeHM READ timeHM NOTIFY minutesChanged)
    Q_PROPERTY(QString timeHMS READ timeHMS NOTIFY secondsChanged)
    Q_PROPERTY(double secondsSinceMidnight READ secondsSinceMidnight NOTIFY secondsChanged)
    Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY speedChanged FINAL)
    Q_PROPERTY(QColor hourMarkColor READ hourMarkColor WRITE setHourMarkColor NOTIFY colorChanged)
    Q_PROPERTY(QColor hourHandColor READ hourHandColor WRITE setHourHandColor NOTIFY colorChanged)
    Q_PROPERTY(QColor minuteHandColor READ minuteHandColor WRITE setMinuteHandColor NOTIFY colorChanged)
    Q_PROPERTY(QColor secondHandColor READ secondHandColor WRITE setSecondHandColor NOTIFY colorChanged)

  public:
    /// \brief Constructs a DsQmlClock instance with an optional parent item.
    explicit DsQmlClock(QQuickItem* parent = nullptr);

    /// \brief Returns the refresh interval in milliseconds.
    int interval() const { return m_timer.interval(); }
    /// \brief Sets the refresh interval in milliseconds.
    void setInterval(int interval) {
        if (m_timer.interval() == interval) return;
        m_timer.setInterval(interval);
        emit intervalChanged();
    }

    /// \brief Returns the color used for hour marks.
    QColor hourMarkColor() const { return m_hour_mark_color; }
    /// \brief Returns the color used for the hour hand.
    QColor hourHandColor() const { return m_hour_hand_color; }
    /// \brief Returns the color used for the minute hand.
    QColor minuteHandColor() const { return m_minute_hand_color; }
    /// \brief Returns the color used for the second hand.
    QColor secondHandColor() const { return m_second_hand_color; }

    /// \brief Sets the color used for hour marks.
    void setHourMarkColor(const QColor& color) {
        if (m_hour_mark_color == color) return;
        m_hour_mark_color = color;
        emit colorChanged();
    }
    /// \brief Sets the color used for the hour hand.
    void setHourHandColor(const QColor& color) {
        if (m_hour_hand_color == color) return;
        m_hour_hand_color = color;
        emit colorChanged();
    }
    /// \brief Sets the color used for the minute hand.
    void setMinuteHandColor(const QColor& color) {
        if (m_minute_hand_color == color) return;
        m_minute_hand_color = color;
        emit colorChanged();
    }
    /// \brief Sets the color used for the second hand.
    void setSecondHandColor(const QColor& color) {
        if (m_second_hand_color == color) return;
        m_second_hand_color = color;
        emit colorChanged();
    }

    /// \brief Returns the current local date and time.
    QDateTime now() const { return m_local_date_time; }
    /// \brief Returns the current local date.
    QDate date() const { return m_local_date_time.date(); }
    /// \brief Returns the current local date as "yyyy-MM-dd" string.
    QString dateYMD() const { return m_local_date_time.toString("yyyy-MM-dd"); }
    /// \brief Returns the current local date as "dddd, MMMM d, yyyy" string.
    QString dateFull() const { return m_local_date_time.toString("dddd, MMMM d, yyyy"); }
    /// \brief Returns the current local time.
    QTime time() const { return m_local_date_time.time(); }
    /// \brief Returns the current local time as "HH:mm" string.
    QString timeHM() const { return m_local_date_time.toString("HH:mm"); }
    /// \brief Returns the current local time as "HH:mm:ss" string.
    QString timeHMS() const { return m_local_date_time.toString("HH:mm:ss"); }
    /// \brief Returns the current local time in seconds since midnight.
    double secondsSinceMidnight() const { return QTime(0, 0).secsTo(m_local_date_time.time()); }
    /// \brief Returns the speed multiplier for time progression.
    int speed() const { return m_speed; }
    /// \brief Sets the speed multiplier for time progression.
    void setSpeed(int speed) {
        if (m_speed == speed) return;
        m_speed = speed;
        emit speedChanged();
    }

  protected:
    /// \brief Paints the clock using the provided painter.
    void paint(QPainter* painter) override;

  signals:
    /// \brief Emitted when the refresh interval changes.
    void intervalChanged();
    /// \brief Emitted when any color property changes.
    void colorChanged();
    /// \brief Emitted when the seconds change.
    void secondsChanged();
    /// \brief Emitted when the minutes change.
    void minutesChanged();
    /// \brief Emitted when the hours change.
    void hoursChanged();
    /// \brief Emitted when the date changes.
    void dateChanged();
    /// \brief Emitted when the speed changes.
    void speedChanged();

  private:
    /// \brief Updates the internal time state.
    void updateTime();

    QColor    m_hour_mark_color{Qt::white};
    QColor    m_hour_hand_color{Qt::white};
    QColor    m_minute_hand_color{Qt::white};
    QColor    m_second_hand_color{Qt::white};
    QTimer    m_timer;
    QDateTime m_last_update;
    QDateTime m_local_date_time;
    qint64    m_speed{1};
};

} // namespace dsqt::ui

#endif
