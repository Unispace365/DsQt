\page bridge_usage Using DsBridge

[TOC]

This document describes how to use the `DsBridge` QML singleton and related types to access CMS content from BridgeSync in your QML user interface.

\see best_practices For general QML layout and styling recommendations.

# Overview

DsQt's Bridge module connects your application to a BridgeSync-populated SQLite database. Once configured and running, content is loaded in the background and made available to QML through the `DsBridge` singleton. Any time the database is updated, `DsBridge` emits signals so your UI can react.

The bridge delivers two kinds of data:

- **Content records** — CMS records organized in a tree. Each record is a `ContentModel`, a property map whose keys correspond to the `appKey` values defined in your schema.
- **Event records** — scheduled content entries that can be filtered and queried by time of day and day of week.

# Importing the Module

```qml
import Dsqt.Bridge
```

This gives you access to the `DsBridge` singleton and the `DsEventSchedule` element.

# Reacting to Bridge Updates

`DsBridge` is a QML singleton — you never instantiate it. Connect to its `onBridgeUpdated` signal to know when the database has been refreshed:

```qml
import Dsqt.Bridge

Item {
    Connections {
        target: DsBridge
        function onBridgeUpdated() {
            console.log("Content updated!")
            // Refresh any local state here.
        }
    }
}
```

`bridgeUpdated` fires both when the content tree changes (`contentChanged`) and when the raw database is refreshed (`databaseChanged`). For most UI work, listening to `bridgeUpdated` is sufficient.

\note Always use `bridgeUpdated` in QML — do not connect to `contentChanged` or `databaseChanged` directly. Those signals exist for internal C++ use and do not guarantee that the full content pipeline (processing, linking, sorting) has completed. `bridgeUpdated` is emitted only after the pipeline finishes, so data is always in a consistent state when it fires.

# DsBridge Reference

| Member | Description |
|--------|-------------|
| `content` (property) | The root of the content tree as a `ContentModel`. |
| `bridgeUpdated` (signal) | Emitted when the full update pipeline completes. **Use this in QML.** |
| `contentChanged` (signal) | Internal — emitted mid-pipeline when the content tree is rebuilt. Do not use in QML. |
| `databaseChanged` (signal) | Internal — emitted mid-pipeline when the raw database records change. Do not use in QML. |
| `getPlatformRecord()` | Returns the `ContentModel` for the current platform, or `null`. |
| `getPlatformUid()` | Returns the UID string of the current platform. |
| `getPlatformUids()` | Returns a list of all platform UIDs. |
| `getRecordById(uid)` | Returns the `ContentModel` with the given UID, or `null`. |

The current platform is determined by the `platform.id` key in `app_settings`. This must match a platform UID in the database before `getPlatformRecord()` will return a result.

# ContentModel

Each CMS record is a `ContentModel` — a `QQmlPropertyMap` whose properties correspond directly to the `appKey` values defined in your schema. Access them in QML like any other property:

```qml
Text {
    text: platformRecord?.journey_text ?? ""
}
```

(See [Getting the Platform Record](#getting-the-platform-record) for how to obtain `platformRecord` from `DsBridge`.)

Every record also provides these built-in properties:

| Property | Type | Description |
|----------|------|-------------|
| `uid` | string | Unique identifier of the record. |
| `record_name` | string | Name of the record as entered in the CMS. |
| `type_name` | string | Display name of the content type. |
| `type_key` | string | `appKey` of the content type. |
| `children` | list | Child `ContentModel` records, in rank order. |
| `parent_uid` | list of strings | UIDs of the record's parent(s). |
| `rank` | int | Sort order within the parent slot. |

# Getting the Platform Record

The most common starting point is to get the current platform record and display its fields:

```qml
import Dsqt.Bridge

Item {
    id: root

    property var platform: null

    Connections {
        target: DsBridge
        function onBridgeUpdated() {
            root.platform = DsBridge.getPlatformRecord()
        }
    }

    Text {
        // Example using the Entry & Lounge platform
        // (appKey: "platform_entry_lounge").
        // Field: "Journey Text" (appKey: "journey_text", type: RICH_TEXT)
        text: root.platform?.journey_text ?? ""
    }
}
```

The `?.` optional-chaining operator prevents errors when `platform` is `null` — i.e., before the first bridge update has arrived.

# Accessing Text and Number Fields

Simple text and number fields are accessed directly by their `appKey`:

```qml
// Example schema:Agenda Item (appKey: "agenda_item_name")
// Fields:
//   "Time"        (appKey: "agenda_item_time",  type: DATE_TIME / TIME)
//   "Time String" (appKey: "agenda_time_string", type: TEXT)

Text { text: record.agenda_time_string }
Text { text: Qt.formatTime(record.agenda_item_time, "h:mm ap") }
```

# Accessing Color Fields

Color fields are delivered as strings in `#rrggbb` format, suitable for use directly in QML `color` properties:

```qml
// Example schema:Water Ripples Brand Profile (appKey: "brand_profile")
// Field: "Brand Color" (appKey: "brand_color", type: COLOR)

Rectangle {
    color: record.brand_color ?? "transparent"
}
```

# Accessing Media Fields

Image, video, and PDF fields are delivered as objects with the following sub-properties:

| Sub-property | Type | Description |
|--------------|------|-------------|
| `filepath` | string | Absolute local path to the file. |
| `type` | string | `"IMAGE"`, `"VIDEO"`, `"PDF"`, or `"WEB"`. |
| `width` | real | Native width of the media in pixels. |
| `height` | real | Native height of the media in pixels. |
| `crop` | list | Crop parameters `[x, y, w, h]` in normalized [0–1] coordinates. |

```qml
// Example schema:Water Ripples Brand Profile
// Field: "Brand Logo" (appKey: "brand_logo", type: IMAGE)

Image {
    source: "file:///" + (record.brand_logo?.filepath ?? "")
    visible: record.brand_logo !== undefined
}

// Example schema:Square Media (appKey: "square_media")
// Field: "Media" (appKey: "media", type: IMAGE or VIDEO)

Image {
    visible: record.media?.type === "IMAGE"
    source: "file:///" + (record.media?.filepath ?? "")
}
```

For video fields, use a `Video` or `MediaPlayer` element with the same `filepath` property.

# Navigating Child Records

Records that contain slots expose their children via the `children` property. Use a `Repeater` to iterate them:

```qml
// Example schema:Entry & Lounge platform
// Slot "Default Design Mode" contains:
//   Water Ripples Brand Profile, Current, Square Media Playlist

Repeater {
    model: root.platform?.children ?? []

    delegate: Text {
        required property var modelData
        text: modelData.record_name + " (" + modelData.type_key + ")"
    }
}
```

To display all children of a particular type, filter the `children` list and use a `Repeater`:

```qml
// Collect all children whose type_key is "brand_profile".
property var brandProfiles: {
    if (!root.platform) return []
    return root.platform.children.filter(c => c.type_key === "brand_profile")
}

Repeater {
    model: brandProfiles

    delegate: Image {
        required property var modelData
        source: "file:///" + (modelData.brand_logo?.filepath ?? "")
    }
}
```

# Looking Up Records by UID

If you have a UID — from a link field, or from the `content_uid` list on the root — use `getRecordById`:

```qml
property var linked: DsBridge.getRecordById(someUid)

Text {
    text: linked?.record_name ?? "(not found)"
}
```

# Working with Event Schedules

`DsEventSchedule` provides a filtered, time-aware view of the event records in the bridge database. It does not produce any visible UI.

```qml
import Dsqt.Bridge

DsEventSchedule {
    id: schedule
    type: "scheduled_content"  // Filter by type_name. Leave blank for all events.
}
```

| Property | Type | Description |
|----------|------|-------------|
| `type` | string | Filter events by `type_name`. Empty string includes all. |
| `events` | list | All events active today, sorted by priority. |
| `timeline` | list | Like `events`, but with overlapping events merged into one. |
| `current` | DsQmlEvent | The highest-priority event active right now, or `null`. |
| `clock` | DsQmlClock | Optional clock to drive automatic minute-by-minute updates. |

`DsEventSchedule` updates automatically when `DsBridge` signals a refresh. For live time-of-day filtering (so `current` changes as time passes without a database refresh), attach a `DsQmlClock` from `Dsqt.Core`:

```qml
import Dsqt.Core
import Dsqt.Bridge

DsQmlClock { id: clock }

DsEventSchedule {
    id: schedule
    type: "scheduled_content"
    clock: clock
}
```

## DsQmlEvent Properties

Each item in `events` or `timeline` is a `DsQmlEvent`:

| Property | Type | Description |
|----------|------|-------------|
| `uid` | string | Unique identifier. |
| `type` | string | Type name. |
| `title` | string | Event title (`record_name` from the CMS). |
| `start` | DateTime | Start date/time. |
| `end` | DateTime | End date/time. |
| `days` | int | Bitmask of active weekdays (bit 0 = Sunday, bit 1 = Monday, …, bit 6 = Saturday). |
| `secondsSinceMidnight` | real | Start time in seconds from midnight. |
| `durationInSeconds` | real | Duration in seconds. |
| `order` | int | Sort index, useful for color assignment. |
| `model` | ContentModel | The full content record for this event, giving access to all schema fields by `appKey`. |

## Displaying Events

```qml
// Show all events scheduled for today.
Column {
    Repeater {
        model: schedule.events
        delegate: Text {
            required property var modelData
            text: modelData.title + "  " +
                  Qt.formatTime(modelData.start, "h:mm ap") + " – " +
                  Qt.formatTime(modelData.end,   "h:mm ap")
            color: "white"
            font.pixelSize: 28
        }
    }
}

// Show only the currently active event.
Text {
    text: schedule.current?.title ?? "No active event"
}
```

## Drawing a Timeline Bar

Use `secondsSinceMidnight` and `durationInSeconds` to position events proportionally across a horizontal bar:

```qml
Item {
    id: timelineBar
    width: parent.width
    height: 40

    Repeater {
        model: schedule.timeline

        Rectangle {
            required property var modelData
            x:      (modelData.secondsSinceMidnight / 86400.0) * timelineBar.width
            width:  (modelData.durationInSeconds    / 86400.0) * timelineBar.width
            height: timelineBar.height
            radius: 8
            // color() picks from a list based on the event's order index.
            color:  modelData.color(["#4A90D9", "#7B68EE", "#50C878", "#FF8C00"])
        }
    }
}
```

# Full Example

The following combines the patterns above. It is based on `Bridge_Test/qml/BridgeDemo.qml` and extended with platform field access:

```qml
import QtQuick
import QtQuick.Layouts
import Dsqt.Bridge
import Dsqt.Core

Item {
    id: root

    property var platform: null
    property real lastUpdated: -1

    // Refresh platform and timestamp on every bridge update.
    Connections {
        target: DsBridge
        function onBridgeUpdated() {
            root.platform    = DsBridge.getPlatformRecord()
            root.lastUpdated = Date.now()
        }
    }

    // Update the "time since last update" display every second.
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: timeAgoText.refresh()
    }

    // Clock drives live schedule filtering.
    DsQmlClock { id: clock }

    DsEventSchedule {
        id: schedule
        clock: clock
        // Leave type blank to include all scheduled events.
    }

    // Background
    Rectangle {
        anchors.fill: parent
        color: "darkslategray"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 20

        Text {
            text: "Bridge Demo"
            font.pixelSize: 48
            color: "white"
        }

        // Last-updated timestamp.
        Text {
            id: timeAgoText
            font.pixelSize: 32
            color: "white"

            function refresh() {
                if (root.lastUpdated < 0) { text = "Never updated"; return }
                var s = Math.round((Date.now() - root.lastUpdated) / 1000)
                text = s < 60   ? s + " seconds ago"
                     : s < 3600 ? Math.round(s / 60)   + " minutes ago"
                     :             Math.round(s / 3600) + " hours ago"
            }
        }

        // Platform UID.
        Text {
            text: "Platform: " + (root.platform?.uid ?? "not found")
            font.pixelSize: 28
            color: "white"
        }

        // A platform field — "Journey Text" from the Entry & Lounge schema.
        Text {
            text: root.platform?.journey_text ?? ""
            font.pixelSize: 24
            color: "white"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Event summary.
        Text {
            text: "Events today: " + schedule.events.length
            font.pixelSize: 28
            color: "white"
        }

        Text {
            text: "Current event: " + (schedule.current?.title ?? "none")
            font.pixelSize: 28
            color: "white"
        }

        // Timeline bar.
        Item {
            id: timelineBar
            Layout.fillWidth: true
            height: 40

            Repeater {
                model: schedule.timeline

                Rectangle {
                    required property var modelData
                    x:      (modelData.secondsSinceMidnight / 86400.0) * timelineBar.width
                    width:  (modelData.durationInSeconds    / 86400.0) * timelineBar.width
                    height: timelineBar.height
                    radius: 8
                    color:  modelData.color(["#4A90D9", "#7B68EE", "#50C878", "#FF8C00"])
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
```
