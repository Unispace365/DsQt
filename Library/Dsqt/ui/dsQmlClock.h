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
    explicit DsQmlClock(QQuickItem* parent = nullptr);

    // Returns the refresh interval in milliseconds.
    int interval() const { return m_timer.interval(); }
    // Sets the refresh interval in milliseconds.
    void setInterval(int interval) {
        if (m_timer.interval() == interval) return;
        m_timer.setInterval(interval);
        emit intervalChanged();
    }

    //
    QColor hourMarkColor() const { return m_hour_mark_color; }
    //
    QColor hourHandColor() const { return m_hour_hand_color; }
    //
    QColor minuteHandColor() const { return m_minute_hand_color; }
    //
    QColor secondHandColor() const { return m_second_hand_color; }

    //
    void setHourMarkColor(const QColor& color) {
        if (m_hour_mark_color == color) return;
        m_hour_mark_color = color;
        emit colorChanged();
    }
    //
    void setHourHandColor(const QColor& color) {
        if (m_hour_hand_color == color) return;
        m_hour_hand_color = color;
        emit colorChanged();
    }
    //
    void setMinuteHandColor(const QColor& color) {
        if (m_minute_hand_color == color) return;
        m_minute_hand_color = color;
        emit colorChanged();
    }
    //
    void setSecondHandColor(const QColor& color) {
        if (m_second_hand_color == color) return;
        m_second_hand_color = color;
        emit colorChanged();
    }

    // Returns the current local date and time.
    QDateTime now() const { return m_local_date_time; }
    // Returns the current local date.
    QDate date() const { return m_local_date_time.date(); }
    // Returns the current local date as "yyyy-MM-dd" string.
    QString dateYMD() const { return m_local_date_time.toString("yyyy-MM-dd"); }
    // Returns the current local date as "dddd, MMMM d, yyyy" string.
    QString dateFull() const { return m_local_date_time.toString("dddd, MMMM d, yyyy"); }
    // Returns the current local time.
    QTime time() const { return m_local_date_time.time(); }
    // Returns the current local time as "HH:mm" string.
    QString timeHM() const { return m_local_date_time.toString("HH:mm"); }
    // Returns the current local time as "HH:mm:ss" string.
    QString timeHMS() const { return m_local_date_time.toString("HH:mm:ss"); }
    // Returns the current local time in seconds since midnight.
    double secondsSinceMidnight() const { return QTime(0, 0).secsTo(m_local_date_time.time()); }
    //
    int speed() const { return m_speed; }
    //
    void setSpeed(int speed) {
        if (m_speed == speed) return;
        m_speed = speed;
        emit speedChanged();
    }

  protected:
    void paint(QPainter* painter) override;

  signals:
    void intervalChanged();
    void colorChanged();
    void secondsChanged();
    void minutesChanged();
    void hoursChanged();
    void dateChanged();
    void speedChanged();

  private:
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
