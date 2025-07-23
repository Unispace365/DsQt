#include "schedule_helper.h"

#include "ds_qml_obj.h"
#include "dsqmlapplicationengine.h"

namespace dsqt::model {

ScheduledEvents::ScheduledEvents(QObject* parent)
    : QAbstractListModel(parent) {

    // connect(
    //    parent, &DSQmlApplicationEngine::onInit, this,
    //    [this]() {
    auto engine = DSQmlApplicationEngine::DefEngine();

    connect(
        engine, &DSQmlApplicationEngine::rootUpdated, this,
        [this, engine]() {
            // Obtain events from engine.
            auto content   = engine->getContentRoot();
            auto allEvents = content.getChildByName("all_events");
            auto events    = allEvents.getChildren();

            auto now = QDateTime::currentDateTime();
            DsQmlObj::filterEvents(events, now.date());

            DsQmlObj::sortEvents(events, now);

            // Remove existing events.
            for (size_t i = m_events.size(); i > 0; --i) {
                size_t index = i - 1;
                beginRemoveRows(QModelIndex(), index, index);
                m_events.removeAt(index);
                endRemoveRows();
            }

            // Convert events to internal representation.
            static std::vector<QColor> colors     = {0xa1cdf4, 0xf4a1cd, 0xcdf4a1};
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
        },
        Qt::ConnectionType::QueuedConnection);
    //   },
    //   Qt::ConnectionType::QueuedConnection);
}

} // namespace dsqt::model
