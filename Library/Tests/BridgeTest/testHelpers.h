#pragma once

// Shared fixtures for the Bridge unit tests.
//
// The tests deliberately do not know about any project's schema. They build records with the same value *shapes*
// that DsBridgeSqlQuery::queryTables() produces for each CMS field type, and check that the classes in
// dsBridgeDatabase.h behave with them.

#include "bridge/dsBridgeDatabase.h"

#include <QDateTime>
#include <QStringList>
#include <QVariantHash>
#include <QVariantList>

namespace dsqt::bridge {

/// Friend of DatabaseContent. Lets tests fill the database without running the SQL queries.
struct DatabaseContentTestAccess {
    /// Adds a record and files it under its variant, like queryTables() does.
    static void add(DatabaseContent& content, const DatabaseRecord& record) {
        const QString uid     = record.uid();
        const QString variant = record.value("variant").toString();
        content.m_records.insert(uid, record);
        if (variant == "SCHEDULE")
            content.m_events.append(uid);
        else if (variant == "ROOT_CONTENT")
            content.m_content.append(uid);
        else if (variant == "ROOT_PLATFORM")
            content.m_platforms.append(uid);
    }

    /// Links parents and children, and sorts by rank. Call after all records have been added.
    static void buildTree(DatabaseContent& content) { content.buildTree(); }

    static DatabaseRecordHash& records(DatabaseContent& content) { return content.m_records; }
};

} // namespace dsqt::bridge

namespace testing {

using namespace dsqt::bridge;

/// A record with only the given fields (plus its uid).
inline DatabaseRecord makeRecord(const QString& uid, QVariantHash fields = {}) {
    fields.insert("uid", uid);
    return DatabaseRecord(fields);
}

/// A record the way queryTables() builds it: uid, variant, rank and (optionally) parents.
inline DatabaseRecord makeRecord(const QString& uid, const QString& variant, int rank,
                                 const QStringList& parents = {}, QVariantHash extra = {}) {
    extra.insert("variant", variant);
    extra.insert("rank", rank);
    if (!parents.isEmpty()) extra.insert("parent_uid", parents);
    return makeRecord(uid, extra);
}

/// A media field value, shaped like DsResource::toQml().
inline QVariantHash makeResourceHash(const QString& filepath, const QString& type = "IMAGE", double width = 1920,
                                     double height = 1080,
                                     const QVariantList& crop = {0.0, 0.0, 1.0, 1.0}) {
    QVariantHash hash;
    hash.insert("filepath", filepath);
    hash.insert("type", type);
    hash.insert("width", width);
    hash.insert("height", height);
    hash.insert("crop", crop);
    return hash;
}

/// The date used by the event tests. 1 October 2026 is a Thursday (weekday bit 4).
inline const QDate kToday(2026, 10, 1);
constexpr int       kThursday = 0x10;
constexpr int       kAllDays  = 0x7F;

inline QDateTime at(int hour, int minute = 0, int second = 0, const QDate& date = kToday) {
    return QDateTime(date, QTime(hour, minute, second));
}

/// An event record the way queryTables() builds it. By default it is scheduled on kToday only.
inline DatabaseRecord makeEvent(const QString& uid, const QTime& start, const QTime& end, int days = kAllDays,
                                const QStringList& parents = {}, const QString& typeName = {},
                                const QDate& firstDate = kToday, const QDate& lastDate = kToday) {
    QVariantHash fields;
    fields.insert("type_name", typeName);
    fields.insert("record_name", uid);
    fields.insert("effective_days", days);
    fields.insert("start_date_time", QDateTime(firstDate, start));
    fields.insert("end_date_time", QDateTime(lastDate, end));
    return makeRecord(uid, "SCHEDULE", 0, parents, fields);
}

inline QStringList uids(const DatabaseRecordList& records) {
    QStringList result;
    for (const auto& record : records)
        result.append(record.uid());
    return result;
}

} // namespace testing
