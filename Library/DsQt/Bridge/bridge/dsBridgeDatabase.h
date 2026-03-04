#ifndef DSBRIDGEDATABASE_H
#define DSBRIDGEDATABASE_H

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariantHash>

#include <bitset>

namespace dsqt::bridge {

class DatabaseRecord : public QVariantHash {
  public:
    QString uid() const { return value("uid", {}).toString(); }
    QString recordName() const { return value("record_name", {}).toString(); }
    QString typeName() const { return value("type_name", {}).toString(); }
    QString typeKey() const { return value("type_key", {}).toString(); }

    /// Returns a list of UIDs.
    QStringList list(const QString& key) const { return value(key, {}).toString().split(','); }

    ///
    QStringList children() const { return list("child_uid"); }

    /// Returns the file path if this record is a resource.
    QString filepath(const QString& key, const QString& defaultValue = "") const {
        QVariantMap fileinfo = value(key, {}).toMap();
        return QUrl(fileinfo.value("filepath", defaultValue).toString()).toLocalFile();
    }

    /// Returns the start date and time if this record is an event.
    QDateTime start() const { return value("start_date_time", {}).toDateTime(); }
    /// Returns the end date and time if this record is an event.
    QDateTime end() const { return value("end_date_time", {}).toDateTime(); }
    /// Returns the weekdays for which the event is scheduled.
    int days() const { return value("effective_days", 0x7F).toInt() & 0x7F; }
};

using DatabaseRecordList = QList<DatabaseRecord>;
using DatabaseRecordHash = QHash<QString, DatabaseRecord>;

class DatabaseContent {
  public:
    DatabaseContent() = default;

    /// Finds a record by its uid. If the record is not found, an empty record is returned.
    DatabaseRecord find(const QString& uid) const { return m_records.value(uid); }
    /// Finds records by their uids. If a record is not found, it is skipped.
    DatabaseRecordList find(const QStringList& uids) const {
        DatabaseRecordList result;
        for (const auto& uid : uids) {
            if (auto itr = m_records.constFind(uid); itr != m_records.constEnd()) result.append(itr.value());
        }
        return result;
    }

    /// Returns a list of record uids, sorted in the correct order.
    const QStringList& recordUids() const { return m_sorted; }
    /// Returns a list of record uids for all content root records.
    const QStringList& contentUids() const { return m_content; }
    /// Returns a list of record uids for all event records.
    const QStringList& eventUids() const { return m_events; }
    /// Returns a list of record uids for all platform records.
    const QStringList& platformUids() const { return m_platforms; }

    /// Returns all records.
    const DatabaseRecordHash& records() const { return m_records; }
    /// Returns all content root records.
    DatabaseRecordList content() const { return find(m_content); }
    /// Returns all event records.
    DatabaseRecordList events() const { return find(m_events); }
    /// Returns all platform records.
    DatabaseRecordList platforms() const { return find(m_platforms); }

    /// Returns the platform record based on the [platform.id] in app_settings.
    DatabaseRecord getPlatform() const;
    /// Returns the first platform record found with the specified type name.
    DatabaseRecord getPlatform(const QString& typeName) const;
    /// Returns the currently active event record based on the current local date and time. If there are multiple active
    /// events, the one with the highest priority (lowest rank) is returned. If there are no active events, an empty
    /// record is returned.
    DatabaseRecord getCurrentEvent(const QDateTime& localDateTime) const;

  private:
    friend class DatabaseIterator;
    friend class DatabaseTree;
    friend class DsBridgeSqlQuery;

    /// Links all records to their parents and all parents to their children.
    void buildTree() {
        // Clear existing list of children.
        for (auto node : m_records.asKeyValueRange()) {
            node.second["child_uid"] = QStringList();
        }
        // Create the sorted list of records.
        m_sorted = m_records.keys();
        std::sort(m_sorted.begin(), m_sorted.end(), [&](const QString& uid1, const QString& uid2) {
            int rank1 = m_records.value(uid1).value("rank").toInt();
            int rank2 = m_records.value(uid2).value("rank").toInt();
            return rank1 < rank2; // Ascending order
        });
        // Populate list of children. Children are sorted in order.
        for (auto node : m_records.asKeyValueRange()) {
            const auto uid         = node.second["uid"].toString();
            const auto parent_uids = node.second["parent_uid"].toStringList();
            if (parent_uids.size() != 1) continue; // Skip records with multiple parents, or no parents at all.
            for (const auto& parent_uid : parent_uids) {
                const auto id = parent_uid.trimmed();
                if (m_records.contains(id)) {
                    auto children = m_records[id]["child_uid"].toStringList();
                    if (!children.contains(uid)) children.append(uid);
                    std::sort(children.begin(), children.end(), [&](const QString& uid1, const QString& uid2) {
                        int rank1 = m_records.value(uid1).value("rank").toInt();
                        int rank2 = m_records.value(uid2).value("rank").toInt();
                        return rank1 < rank2; // Ascending order
                    });
                    m_records[id]["child_uid"] = children;
                }
            }
        }
    }

    DatabaseRecordHash m_records;   // All records.
    QStringList        m_content;   // All content records.
    QStringList        m_platforms; // All platform records.
    QStringList        m_events;    // All event records.
    QStringList        m_sorted;    // All records sorted in order.
    QStringList        m_queue;     // All records in the order in which they need to be processed.
};

// Iterator class for tree traversal
class DatabaseIterator {
  public:
    DatabaseIterator(DatabaseRecordHash& nodes, const QStringList& order, qsizetype index)
        : m_nodes(nodes)
        , m_order(order)
        , m_index(index) {}

    // Dereference operator
    const DatabaseRecord& operator*() const { return m_nodes[m_order[m_index]]; }

    // Pointer operator
    const DatabaseRecord* operator->() const { return &m_nodes[m_order[m_index]]; }

    // Increment operator (pre-increment)
    DatabaseIterator& operator++() {
        if (m_index < m_order.size()) {
            ++m_index;
        }
        return *this;
    }

    // Equality operator
    bool operator==(const DatabaseIterator& other) const {
        return m_index == other.m_index && &m_nodes == &other.m_nodes;
    }

    // Inequality operator
    bool operator!=(const DatabaseIterator& other) const { return !(*this == other); }

  private:
    DatabaseRecordHash& m_nodes; // Reference to the node hash
    const QStringList&  m_order; // UIDs in the correct order
    qsizetype           m_index; // Current position in traversal
};

// Helper class to traverse the database.
class DatabaseTree {
  public:
    enum class Traversal { PreOrder, PostOrder, Roots };

    DatabaseTree(DatabaseRecordHash& nodes, const QString& rootUid, Traversal order = Traversal::PostOrder)
        : m_nodes(nodes) {
        if (order == Traversal::Roots) {
            // rootsTraversal() iterates all nodes and does not use rootUid,
            // so it is safe to call even when rootUid is empty or not in the map.
            rootsTraversal();
        } else if (m_nodes.contains(rootUid)) {
            switch (order) {
            case Traversal::PreOrder: // Visits the root node first, then recursively traverses its children.
                preOrderTraversal(rootUid);
                break;
            case Traversal::PostOrder: // Visits the child nodes before their parents, with the root node visited
                                       // last.
                postOrderTraversal(rootUid);
                break;
            default:
                break;
            }
        }
    }

    DatabaseTree(DatabaseContent& content, Traversal order = Traversal::PostOrder)
        : DatabaseTree(content.m_records, content.m_content.isEmpty() ? QString() : content.m_content.front(), order) {}

    // Begin iterator
    DatabaseIterator begin() const { return DatabaseIterator(m_nodes, m_order, 0); }

    // End iterator
    DatabaseIterator end() const { return DatabaseIterator(m_nodes, m_order, m_order.size()); }

  private:
    // Recursive pre-order traversal
    void preOrderTraversal(const QString& uid) {
        // Visit the node itself first
        m_order.append(uid);
        // Then visit children
        const QStringList children = m_nodes[uid]["child_uid"].toStringList();
        for (const QString& childUid : children) {
            if (m_nodes.contains(childUid)) {
                preOrderTraversal(childUid);
            }
        }
    }

    // Recursive post-order traversal
    void postOrderTraversal(const QString& uid) {
        // Visit children first
        const QStringList children = m_nodes[uid]["child_uid"].toStringList();
        for (const QString& childUid : children) {
            if (m_nodes.contains(childUid)) {
                postOrderTraversal(childUid);
            }
        }
        // Then visit the node itself
        m_order.append(uid);
    }

    // Roots traversal
    void rootsTraversal() {
        for (const auto itr : m_nodes.asKeyValueRange()) {
            const auto& record = itr.second;
            if (!record.contains("parent_uid")) continue;
            const auto parents = record["parent_uid"].toStringList();
            if (parents.size() != 1) m_order.append(itr.first);
        }
    }

    DatabaseRecordHash& m_nodes; // Reference to the node hash (non-const for modification)
    QStringList         m_order; // UIDs of nodes in post-order
};

/// Returns whether the event is scheduled for the specified day, taking into account the weekdays.
static bool isToday(const QDate& localDate, const QDateTime& eventStart, const QDateTime& eventEnd,
                    int dayFlags = 0x7F) {
    if (!eventStart.isValid() || !eventEnd.isValid() || localDate < eventStart.date() || localDate > eventEnd.date())
        return false;

    // Returns the weekday (0 to 6, where 0 = Sunday, 1 = Monday, ..., 6 = Saturday).
    const int dayNumber = localDate.dayOfWeek() % 7;
    const int dayFlag   = 0x1 << dayNumber;
    if (dayFlags & dayFlag) return true;

    return false;
}

/// Returns whether the event is scheduled for the specified date and time, taking into account specific times or
/// weekdays.
static bool isNow(const QDateTime& localDateTime, const QDateTime& eventStart, const QDateTime& eventEnd,
                  int dayFlags = 0x7F) {
    if (!isToday(localDateTime.date(), eventStart, eventEnd, dayFlags)) return false;
    if (localDateTime.time() < eventStart.time()) return false;
    if (localDateTime.time() >= eventEnd.time()) return false;
    return true;
}

/// Returns whether the event is scheduled during the time span.
static bool isWithinSpan(const QDateTime& spanStart, const QDateTime& spanEnd, const QDateTime& eventStart,
                         const QDateTime& eventEnd) {
    if (!spanStart.isValid() || !spanEnd.isValid() || spanStart > spanEnd) return false;     // Invalid span.
    if (!eventStart.isValid() || !eventEnd.isValid() || eventStart > eventEnd) return false; // Invalid event.
    return (spanEnd >= eventStart && eventEnd >= spanStart);
}

/// Returns the number of seconds since midnight for the given local date and time.
static int secondsSinceMidnight(const QDateTime& localDateTime) {
    return QTime(0, 0).secsTo(localDateTime.time());
}

/// Returns the duration in seconds between the start and end times of the event.
static int durationInSeconds(const QDateTime& spanStart, const QDateTime& spanEnd) {
    return spanStart.time().secsTo(spanEnd.time());
}

// Returns whether the specified event is currently scheduled, taking into account specific times or weekdays.
static bool isEventNow(const DatabaseRecord& event, const QDateTime& localDateTime) {
    return isNow(localDateTime, event.start(), event.end(), event.days());
}

// Returns whether the specified event is scheduled for today, taking into account the weekdays.
static bool isEventToday(const DatabaseRecord& event, const QDate& localDate) {
    return isToday(localDate, event.start(), event.end(), event.days());
}

// Returns whether the specified event is within the time span.
static bool isEventWithinSpan(const DatabaseRecord& event, const QDateTime& spanStart, const QDateTime& spanEnd) {
    return isWithinSpan(spanStart, spanEnd, event.start(), event.end());
}

// Removes all events that are not of the specified type. Returns the number of removed events.
static size_t filterEvents(DatabaseRecordList& events, const QString& typeName) {
    auto heuristic = [&typeName](const DatabaseRecord& record) -> bool {
        return record.value("type_name", "").toString() != typeName;
    };

    size_t sz = events.size();
    if (!typeName.isEmpty()) events.removeIf(heuristic);
    return sz - events.size();
}

// Removes all events that are not scheduled at the specified date and time. Returns the number of removed events.
static size_t filterEvents(DatabaseRecordList& events, const QDateTime& localDateTime) {
    auto heuristic = [&localDateTime](const DatabaseRecord& record) -> bool {
        return !isEventNow(record, localDateTime);
    };

    size_t sz = events.size();
    events.removeIf(heuristic);
    return sz - events.size();
}

// Removes all events that are not scheduled at the specified date. Returns the number of removed events.
static size_t filterEvents(DatabaseRecordList& events, const QDate& localDate) {
    auto heuristic = [&localDate](const DatabaseRecord& record) -> bool {
        return !isEventToday(record, localDate);
    };

    size_t sz = events.size();
    events.removeIf(heuristic);
    return sz - events.size();
}

// Removes all events that are not within the specified time range. Does not check for specific times or weekdays.
// Returns the number of removed events.
static size_t filterEvents(DatabaseRecordList& events, const QDateTime& spanStart, const QDateTime& spanEnd) {
    auto heuristic = [&spanStart, &spanEnd](const DatabaseRecord& record) -> bool {
        return !isEventWithinSpan(record, spanStart, spanEnd);
    };

    size_t sz = events.size();
    events.removeIf(heuristic);
    return sz - events.size();
}

// Sorts events by the default sorting heuristic. Sorted from highest to lowest priority.
static void sortEvents(DatabaseRecordList& events, const QDateTime& localDateTime = QDateTime().currentDateTime()) {
    auto heuristic = [&localDateTime](const DatabaseRecord& a, const DatabaseRecord& b) -> bool {
        const auto startA = a.start();
        const auto startB = b.start();
        const auto endA   = a.end();
        const auto endB   = b.end();
        const auto daysA  = a.days();
        const auto daysB  = b.days();

        // Active Event trumps Inactive event
        const auto isActiveA = isNow(localDateTime, startA, endA, daysA);
        const auto isActiveB = isNow(localDateTime, startB, endB, daysB);
        if (isActiveA != isActiveB) return isActiveA;

        // Recently Started trumps Previously started
        const auto sinceStartA = startA.time().secsTo(localDateTime.time());
        const auto sinceStartB = startB.time().secsTo(localDateTime.time());
        if (sinceStartA == sinceStartB) { // Starting at the same time.
            const auto durationA = durationInSeconds(startA, endA);
            const auto durationB = durationInSeconds(startB, endB);
            if (durationA == durationB)                // Same duration:
                return std::bitset<8>(daysA).count() < // Fewer days has higher priority
                       std::bitset<8>(daysB).count();
            else                                           // Different duration:
                return durationA < durationB;              // Shorter duration has higher priority.
        } else if ((sinceStartA < 0) != (sinceStartB < 0)) // Only one has already started.
            return sinceStartA >= 0;                       // A has started, priority over B.

        return sinceStartA >= 0
                   ? sinceStartA < sinceStartB  // Both have started: later start time has higher priority.
                   : sinceStartA > sinceStartB; // Both have not yet started: earlier start time has higher priority.
    };

    // Sort from highest priority to lowest priority.
    std::stable_sort(events.begin(), events.end(), heuristic);
}

} // namespace dsqt::bridge

#endif
