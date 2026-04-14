\page dsbridgedatabase_usage Using DsBridgeDatabase

[TOC]

This document describes how to use `DatabaseContent`, `DatabaseRecord`, and `DatabaseResource` from `dsBridgeDatabase.h` to access CMS content directly in C++.

\see bridge_usage For accessing the same content from QML via the `DsBridge` singleton.

# Overview

`DatabaseContent` is the C++ representation of the bridge database. It holds all records loaded from BridgeSync — content records, platform records, and event records — as plain value types. Because these types do not depend on the QML engine, they are safe to use from any thread and do not need to be created on the main thread.

The entry point is `DsQmlBridge::instance().database()`, which returns a `const DatabaseContent&`. The database is rebuilt automatically on a background thread whenever BridgeSync delivers new data. When the swap completes, `DsQmlBridge` emits `databaseChanged()`. You can connect to that signal to react to updates, or simply call `database()` at any time to read the current state.

\note Everything involving `ContentModel` — including `DsQmlBridge::content()`, `getPlatformRecord()`, and `getRecordById()` — is **not** thread-safe and must only be used from the main thread. Use `DatabaseContent` and `DatabaseRecord` whenever you need to work off the main thread.

# Importing the Header

```cpp
#include "bridge/dsBridgeDatabase.h"
```

# Reacting to Database Updates

Connect to `DsQmlBridge::databaseChanged()` to be notified when the database has been refreshed. After the signal fires, `database()` reflects the new state:

```cpp
connect(&DsQmlBridge::instance(), &DsQmlBridge::databaseChanged, this, [this]() {
    const auto& db = DsQmlBridge::instance().database();
    // Use db here.
});
```

If you do not need to react to updates — for example, in a one-shot query — you can call `database()` directly without connecting to any signal.

# DatabaseContent Reference

`DatabaseContent` organises all records into four overlapping views: all records, content root records, platform records, and event records. Records within each view are ordered by `rank`.

## Accessing Records

| Method | Description |
|--------|-------------|
| `find(uid)` | Returns the `DatabaseRecord` with the given UID, or an empty record if not found. |
| `find(uids)` | Returns a `DatabaseRecordList` for a list of UIDs, skipping any that are not found. |
| `records()` | Returns the full `DatabaseRecordHash` of all records, keyed by UID. |

## Accessing Record Groups

| Method | Description |
|--------|-------------|
| `content()` | Returns all content root records as a `DatabaseRecordList`. |
| `events()` | Returns all event records as a `DatabaseRecordList`. |
| `platforms()` | Returns all platform records as a `DatabaseRecordList`. |
| `recordUids()` | Returns all record UIDs in rank order. |
| `contentUids()` | Returns the UIDs of all content root records. |
| `eventUids()` | Returns the UIDs of all event records. |
| `platformUids()` | Returns the UIDs of all platform records. |

## Platform Helpers

| Method | Description |
|--------|-------------|
| `getPlatform()` | Returns the platform record whose UID matches `platform.id` in `app_settings`, or an empty record. |
| `getPlatform(typeName)` | Returns the first platform record whose `type_name` matches the given string, or an empty record. |

## Event Helpers

| Method | Description |
|--------|-------------|
| `getCurrentEvent(localDateTime)` | Returns the highest-priority event active at the given date and time, or an empty record if none is active. When multiple events are active simultaneously, priority is determined by recency of start time, duration (shorter wins), and number of scheduled days (fewer wins). |

# DatabaseRecord Reference

`DatabaseRecord` is a `QVariantHash`. Built-in fields have typed accessor methods; schema-defined fields are accessed via `value("app_key")`, the standard `QVariantHash` interface.

## Built-in Accessors

| Method | Return Type | Description |
|--------|-------------|-------------|
| `uid()` | `QString` | Unique identifier of the record. |
| `recordName()` | `QString` | Name of the record as entered in the CMS. |
| `typeName()` | `QString` | Display name of the content type. |
| `typeKey()` | `QString` | `appKey` of the content type. |
| `rank()` | `int` | Sort order within the parent slot, or `-1` if unknown. |
| `parentUids()` | `QStringList` | UIDs of the record's parent(s). Pass to `DatabaseContent::find()` to obtain the actual records. |
| `childUids()` | `QStringList` | UIDs of the record's children, in rank order. Pass to `DatabaseContent::find()` to obtain the actual records. |

## Record Type Checks

| Method | Description |
|--------|-------------|
| `isRoot()` | Returns `true` if this is a content root record. |
| `isPlatform()` | Returns `true` if this is a platform record. |
| `isEvent()` | Returns `true` if this is an event record. |

## Accessing Schema-Defined Fields

Fields defined in your schema are accessed by their `appKey` via `value()`:

```cpp
QString text = record.value("journey_text").toString();
QColor  color(record.value("brand_color").toString()); // delivered as "#rrggbb"
```

## Accessing List Fields

`list()` splits a string field by a separator character and returns a `QStringList`. Its primary use is splitting fields that contain multiple UIDs, which can then be passed directly to `DatabaseContent::find()`:

```cpp
// Split a UID list field and resolve the records it references.
QStringList linkedUids = record.list("linked_items_uid");
DatabaseRecordList linked = db.find(linkedUids);
```

The separator defaults to `','` but can be overridden for other list formats.

## Accessing Media Fields

Media fields (images, videos, PDFs) are stored as nested hashes. Use `resource()` to obtain a typed `DatabaseResource`, or `filepath()` as a shorthand when only the file path is needed:

```cpp
DatabaseResource logo = record.resource("brand_logo");
if (logo.isValid()) {
    QString path   = logo.filepath();
    QString type   = logo.type();   // "IMAGE", "VIDEO", "PDF", or "WEB"
    qreal   width  = logo.width();
    qreal   height = logo.height();
    QRectF  crop   = logo.crop();   // Normalised [0–1] coordinates: {x, y, w, h}
}

// Shorthand when only the path is needed.
QString path = record.filepath("brand_logo");
```

`isResource()` can be used to check whether a field holds a valid resource before calling `resource()`:

```cpp
if (record.isResource("brand_logo")) {
    // Safe to call resource().
}
```

## Event-Specific Accessors

These methods are meaningful only on records for which `isEvent()` returns `true`:

| Method | Return Type | Description |
|--------|-------------|-------------|
| `start()` | `QDateTime` | Start date and time of the event. |
| `end()` | `QDateTime` | End date and time of the event. |
| `days()` | `int` | Bitmask of active weekdays. Bit 0 = Sunday, bit 1 = Monday, …, bit 6 = Saturday. |

# DatabaseResource Reference

`DatabaseResource` is a `QVariantHash` with typed accessors for media fields. Instances are obtained via `DatabaseRecord::resource()`.

| Method | Return Type | Description |
|--------|-------------|-------------|
| `isValid()` | `bool` | Returns `true` if all required fields (`filepath`, `type`, `width`, `height`, `crop`) are present. |
| `filepath(defaultValue)` | `QString` | Absolute local path to the file. Returns `defaultValue` if missing. |
| `type()` | `QString` | `"IMAGE"`, `"VIDEO"`, `"PDF"`, or `"WEB"`. |
| `width()` | `qreal` | Native width in pixels, or `0` if missing. |
| `height()` | `qreal` | Native height in pixels, or `0` if missing. |
| `crop()` | `QRectF` | Crop rectangle in normalised [0–1] coordinates `{x, y, w, h}`. Defaults to `{0, 0, 1, 1}` if missing. |

# Event Utilities

`dsBridgeDatabase.h` provides a set of free functions in the `dsqt::bridge` namespace for filtering, sorting, and building timelines from event record lists. These are convenience utilities; they operate directly on `DatabaseRecordList` values obtained from `DatabaseContent`.

## Filtering

All filter functions take a `DatabaseRecordList` by reference and remove non-matching records in place. They return the number of records removed.

| Function | Description |
|----------|-------------|
| `filterEvents(events, typeName)` | Removes events whose `type_name` does not match `typeName`. Has no effect if `typeName` is empty. |
| `filterEvents(events, platform)` | Removes events that are not parented to the given platform record. Has no effect if the record is not a platform. |
| `filterEvents(events, localDateTime)` | Removes events that are not active at the given date and time, taking weekdays into account. |
| `filterEvents(events, localDate)` | Removes events not scheduled for the given date, taking weekdays into account. |
| `filterEvents(events, spanStart, spanEnd)` | Removes events that do not overlap the given time span. Does not check weekdays. |

## Sorting

```cpp
sortEvents(events, localDateTime);
```

Sorts a `DatabaseRecordList` from highest to lowest priority based on the given date and time. The ordering rules are: active events rank above inactive ones; among active events, a more recent start time ranks higher; ties are broken by duration (shorter wins) and then by number of scheduled days (fewer wins).

`localDateTime` defaults to `QDateTime::currentDateTime()` if omitted.

## Timeline

```cpp
DatabaseRecordList timeline = eventTimeline(events, localDate);
```

Returns a new `DatabaseRecordList` representing a non-overlapping timeline for the given date. At each point in time, the highest-priority event is selected; overlapping events are merged or truncated so that every moment is covered by at most one record. Records with zero or negative duration after merging are discarded.

`localDate` defaults to `QDate::currentDate()` if omitted.

## Standalone Predicates

These functions test a single event record and are useful when you need a custom loop rather than the bulk filter functions:

| Function | Description |
|----------|-------------|
| `isToday(localDate, eventStart, eventEnd, dayFlags)` | Returns `true` if the event is scheduled on `localDate`, considering the weekday bitmask. |
| `isNow(localDateTime, eventStart, eventEnd, dayFlags)` | Returns `true` if the event is active at the given date and time. |
| `isWithinSpan(spanStart, spanEnd, eventStart, eventEnd)` | Returns `true` if the event overlaps the given time span. |
| `secondsSinceMidnight(localDateTime)` | Returns the number of seconds elapsed since midnight for the given date and time. |
| `durationInSeconds(spanStart, spanEnd)` | Returns the duration in seconds between two date-times. |
