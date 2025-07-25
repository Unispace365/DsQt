#include "schedule_helper.h"

#include "ds_qml_obj.h"
#include "dsqmlapplicationengine.h"
#include "ui/clockqml.h"

namespace dsqt::model {

ScheduledEvents::ScheduledEvents(QObject* parent)
    : QAbstractListModel(parent) {

    auto engine = DSQmlApplicationEngine::DefEngine();

    // Listen to content updates.
    connect(engine, &DSQmlApplicationEngine::rootUpdated, this, &ScheduledEvents::updateNow,
            Qt::ConnectionType::QueuedConnection);

    // Use the built-in QML clock if available. Refresh every minute.
    ui::ClockQML* clock = engine->rootContext()->contextProperty("clock").value<ui::ClockQML*>();
    if (clock) {
        connect(
            clock, &ui::ClockQML::minutesChanged, this, [this, clock]() { update(clock->localDateTime()); },
            Qt::ConnectionType::QueuedConnection);
    }

    // Refresh now.
    updateNow();
}

void ScheduledEvents::update(const QDateTime& localDateTime) {
    auto engine = DSQmlApplicationEngine::DefEngine();

    // Obtain events from engine.
    auto content   = engine->getContentRoot();
    auto allEvents = content.getChildByName("all_events");
    auto events    = allEvents.getChildren();

    // Only keep today's events.
    DsQmlObj::filterEvents(events, localDateTime.date());
    // Sort them by priority.
    DsQmlObj::sortEvents(events, localDateTime);

    // Remove existing events.
    for (size_t i = m_events.size(); i > 0; --i) {
        size_t index = i - 1;
        beginRemoveRows(QModelIndex(), index, index);
        m_events.removeAt(index);
        endRemoveRows();
    }

    // Convert events to internal representation.
    static std::vector<QColor> colors     = {0xa1cdf4}; //, 0xf4a1cd, 0xcdf4a1};
    size_t                     colorIndex = 0;
    for (const auto& event : events) {
        QString title     = event.getPropertyString("record_name");
        QTime   startTime = event.getPropertyTime("start_time");
        QTime   endTime   = event.getPropertyTime("end_time");
        beginInsertRows(QModelIndex(), m_events.size(), m_events.size());
        m_events.emplace_back(
            new ScheduledEvent(title, startTime, endTime, colors.at(colorIndex % colors.size()), this));
        endInsertRows();
        colorIndex++;
    }
}

} // namespace dsqt::model
