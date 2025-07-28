#ifndef CLOCK_QML_H
#define CLOCK_QML_H

#include <QDateTime>
#include <QObject>
#include <QTimer>

namespace dsqt::ui {

class ClockQML : public QObject {
    Q_OBJECT
    Q_PROPERTY(QDateTime now READ now NOTIFY secondsChanged)
    Q_PROPERTY(QDate date READ date NOTIFY dateChanged)
    Q_PROPERTY(QTime time READ time NOTIFY secondsChanged)
    Q_PROPERTY(QString timeHM READ timeHM NOTIFY minutesChanged)
    Q_PROPERTY(QString timeHMS READ timeHMS NOTIFY secondsChanged)
    Q_PROPERTY(double secondsSinceMidnight READ secondsSinceMidnight NOTIFY secondsChanged)

  public:
    explicit ClockQML(QObject* parent = nullptr)
        : QObject(parent) {
        connect(&m_timer, &QTimer::timeout, this, &ClockQML::update);
        m_timer.start(10);

        m_localDateTime = QDateTime::currentDateTime();
    }

    // Returns the current local date and time.
    QDateTime now() const { return m_localDateTime; }
    // Returns the current local date.
    QDate date() const { return m_localDateTime.date(); }
    // Returns the current local time.
    QTime time() const { return m_localDateTime.time(); }
    // Returns the current local time as HH:mm string.
    QString timeHM() const { return m_localDateTime.toString("HH:mm"); }
    // Returns the current local time as HH:mm:ss string.
    QString timeHMS() const { return m_localDateTime.toString("HH:mm:ss"); }
    // Returns the current local time in seconds since midnight.
    double secondsSinceMidnight() const { return QTime(0, 0).secsTo(m_localDateTime.time()); }

  signals:
    void secondsChanged();
    void minutesChanged();
    void hoursChanged();
    void dateChanged();

  private:
    void update() {
        const auto lastDateTime = m_localDateTime;
        const int  lastSeconds  = QTime(0, 0).secsTo(lastDateTime.time());

        // m_localDateTime = QDateTime::currentDateTime();
        m_localDateTime.setSecsSinceEpoch(lastDateTime.toSecsSinceEpoch() + 30);
        if (lastDateTime.date() != m_localDateTime.date()) emit dateChanged();

        const int currentSeconds = QTime(0, 0).secsTo(m_localDateTime.time());
        if (currentSeconds != lastSeconds) emit secondsChanged();
        if (currentSeconds / 60 != lastSeconds / 60) emit minutesChanged();
        if (currentSeconds / 3600 != lastSeconds / 3600) emit hoursChanged();
    }

    QTimer    m_timer;
    QDateTime m_localDateTime;
};

} // namespace dsqt::ui

#endif
