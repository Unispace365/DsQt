#ifndef SCHEDULE_HELPER_H
#define SCHEDULE_HELPER_H

#include <QAbstractListModel>
#include <QColor>
#include <QDateTime>
#include <QObject>

namespace dsqt::model {

class ScheduledEvent : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QTime startTime READ startTime WRITE setStartTime NOTIFY startTimeChanged)
    Q_PROPERTY(QTime endTime READ endTime WRITE setEndTime NOTIFY endTimeChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(double secondsSinceMidnight READ secondsSinceMidnight NOTIFY startTimeChanged)
    Q_PROPERTY(double durationInSeconds READ durationInSeconds NOTIFY endTimeChanged)

  public:
    explicit ScheduledEvent(QObject* parent = nullptr)
        : QObject(parent) {}

    ScheduledEvent(const QString& title, QTime startTime, QTime endTime, QColor color = "lightblue",
                   QObject* parent = nullptr)
        : QObject(parent)
        , m_title(title)
        , m_startTime(startTime)
        , m_endTime(endTime)
        , m_color(color) {}

    QString title() const { return m_title; }
    void    setTitle(const QString& title) {
        if (m_title != title) {
            m_title = title;
            emit titleChanged();
        }
    }

    QTime startTime() const { return m_startTime; }
    void  setStartTime(QTime startTime) {
        if (m_startTime != startTime) {
            m_startTime = startTime;
            emit startTimeChanged();
        }
    }

    QTime endTime() const { return m_endTime; }
    void  setEndTime(QTime endTime) {
        if (m_endTime != endTime) {
            m_endTime = endTime;
            emit endTimeChanged();
        }
    }

    QColor color() const { return m_color; }
    void   setColor(QColor color) {
        if (m_color != color) {
            m_color = color;
            emit colorChanged();
        }
    }

    double secondsSinceMidnight() const { return QTime(0, 0).secsTo(m_startTime); }
    double durationInSeconds() const { return m_startTime.secsTo(m_endTime); }

  signals:
    void titleChanged();
    void startTimeChanged();
    void endTimeChanged();
    void colorChanged();

  private:
    QString m_title;
    QTime   m_startTime;
    QTime   m_endTime;
    QColor  m_color;
};

class ScheduledEvents : public QAbstractListModel {
    Q_OBJECT

  public:
    explicit ScheduledEvents(QObject* parent = nullptr);

    enum Roles {
        TitleRole = Qt::UserRole + 1,
        StartTimeRole,
        EndTimeRole,
        ColorRole,
        SecondsSinceMidnightRole,
        DurationSecondsRole
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_events.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_events.size()) return QVariant();

        const auto event = m_events[index.row()];
        switch (role) {
        case TitleRole:
            return event->title();
        case StartTimeRole:
            return QVariant::fromValue(event->startTime());
        case EndTimeRole:
            return QVariant::fromValue(event->endTime());
        case ColorRole:
            return QVariant::fromValue(event->color());
        case SecondsSinceMidnightRole:
            return event->secondsSinceMidnight();
        case DurationSecondsRole:
            return event->durationInSeconds();
        default:
            return QVariant();
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> roles;
        roles[TitleRole]                = "title";
        roles[StartTimeRole]            = "startTime";
        roles[EndTimeRole]              = "endTime";
        roles[ColorRole]                = "color";
        roles[SecondsSinceMidnightRole] = "secondsSinceMidnight";
        roles[DurationSecondsRole]      = "durationInSeconds";
        return roles;
    }

  private:
    QList<std::shared_ptr<ScheduledEvent>> m_events;
};

} // namespace dsqt::model

#endif // SCHEDULE_HELPER_H
