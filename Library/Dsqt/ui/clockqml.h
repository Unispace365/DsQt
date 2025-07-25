#ifndef CLOCK_QML_H
#define CLOCK_QML_H

#include <QDateTime>
#include <QObject>
#include <QTimer>

namespace dsqt::ui {

class ClockQML : public QObject {
    Q_OBJECT
    Q_PROPERTY(double secondsSinceMidnight READ secondsSinceMidnight NOTIFY secondsChanged)
    Q_PROPERTY(QString timeHM READ timeHM NOTIFY minutesChanged)
    Q_PROPERTY(QString timeHMS READ timeHMS NOTIFY secondsChanged)
    Q_PROPERTY(QDateTime localDateTime READ localDateTime NOTIFY secondsChanged)

  public:
    explicit ClockQML(QObject* parent = nullptr)
        : QObject(parent) {
        connect(&m_timer, &QTimer::timeout, this, &ClockQML::update);
        m_timer.start(10);

        m_localDateTime = QDateTime::currentDateTime();
    }

    double secondsSinceMidnight() const { return QTime(0, 0).secsTo(m_localDateTime.time()); }

    QString timeHM() const { return m_localDateTime.toString("HH:mm"); }

    QString timeHMS() const { return m_localDateTime.toString("HH:mm:ss"); }

    QDateTime localDateTime() const { return m_localDateTime; }

  signals:
    void secondsChanged();
    void minutesChanged();
    void hoursChanged();

  private:
    void update() {
        const auto lastDateTime = m_localDateTime;
        const int  lastSeconds  = QTime(0, 0).secsTo(lastDateTime.time());

        // m_localDateTime = QDateTime::currentDateTime();
        m_localDateTime.setSecsSinceEpoch(lastDateTime.toSecsSinceEpoch() + 30);
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
