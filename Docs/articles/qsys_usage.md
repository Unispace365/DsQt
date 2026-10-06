# Q-Sys integration

`Dsqt.Qsys` connects a DsQt application to the Elevate FE Q-Sys plugin.
The application is a **WebSocket server**; the plugin (or the
`Qsys-Dsqt-Tester` simulator) connects to the application's IP and port.

## Enable the listener

Add this to `settings/engine.toml`:

```toml
[engine.qsys]
enabled = true
port = 9988
```

The defaults are `enabled = false` and `port = 9988`. Ports are integers from
1 to 65535; decimal strings such as `"9988"` also work. Invalid ports leave
the listener stopped and report an error. The listener uses plain `ws://`
on all network interfaces and accepts one plugin connection at a time.

Importing `Dsqt.Qsys` initializes the application-wide service. In a C++-only
application, call `dsqt::DsQsys::instance()` instead. The service uses DsQt's
`engine` settings collection, normally loaded by `DsQmlApplicationEngine::initialize()`.
It also supports being created before that collection is loaded. Changes to
`engine.qsys.enabled` or `engine.qsys.port` restart/stop the listener immediately;
the Q-Sys plugin is responsible for reconnecting.

## QML

```qml
import QtQuick
import Dsqt.Qsys

Item {
    Connections {
        target: Qsys
        function onUpdated(parameter, value) {
            if (parameter === "Entry Door")
                console.log("Door state:", value)
        }
        function onErrorOccurred(message) {
            console.warn(message)
        }
    }

    function setAmbientLighting() {
        return Qsys.send("Lighting Preset 2", true)
    }
}
```

`send()` preserves JSON value types. Prefer QML booleans and numbers:
`Qsys.send("Lights On", true)` and `Qsys.send("Volume Level", -10)`.
For manifest-declared `toggle`, `momentary`, and `indicator` controls, strings
`"true"` and `"false"` (case-insensitive, ignoring surrounding whitespace) are
converted to booleans. Thus `Qsys.send("Lighting Preset 2", "true")` works once
that toggle has been announced. Other strings remain strings, including listbox
selections. Unknown controls are sent unchanged; the plugin decides whether to
accept them. Ownership controls initialization, not write permission; indicators
may reject writes.

A `true` return from a send means the message was queued on the connected
socket, **not** that hardware accepted or acted on it. Disconnected sends return
`false`, emit `errorOccurred`, and are dropped. There is no offline replay queue.
Sending does not update the received-value dictionary; incoming feedback does.

## Updates, discovery, and readiness

| Member | Meaning |
| --- | --- |
| `enabled`, `port` | Current engine configuration; read-only in QML. Invalid ports are exposed as `0`. |
| `listening` | The application's listening socket is open. |
| `connected` | A plugin has connected; its snapshot may still be arriving. |
| `ready` | The plugin sent `qsys_init_finished` for this session. |
| `controls` | Manifest entries with `parameter`, `control_type`, `owner`, and extra metadata such as `min`, `max`, and `unit`. |
| `values` | Map of received parameter values. Use `Qsys.values["Entry Door"]` for reactive QML bindings. |
| `hasValue(name)`, `value(name)` | Imperative lookups; an unknown value is invalid/undefined. |
| `serverName` | Writable sender label, initially `"DsQt"`; informational, not a routing ID. |
| `lastError` | Most recent error; cleared when listening starts successfully. |
| `restart()` | Retry binding after an error, or restart the configured listener. |

`updated(parameter, value)` fires for **every received value**, including initial
snapshot values and repeated equal values. The dictionary is updated before the
signal fires. This preserves repeated momentary pulses and leaves change/edge
detection to the application. Signals do not batch changes or include unrelated
values; `values` provides the complete current dictionary when needed.

The normal connection sequence is `manifestReceived()`, zero or more `updated()`
signals, then `snapshotReady()`. Discovery is pushed automatically; no polling
or query is needed. A manifest can announce controls that have no value yet,
especially listboxes and momentary buttons. `ready` means the initial dump is
complete, not that every control has a value.

`cleared()` invalidates the dictionary and manifest on disconnect, restart,
and the beginning of a new manifest. `ready` becomes false. Clear any local
transition-tracking state in that handler. `parameterRenamed(oldParameter,
newParameter)` fires after both the dictionary and manifest have been updated.
Follow renames if an application tracks a configured control; no synthetic
`updated()` event is emitted just for a rename.

Momentary controls report `true` pulses. The service emits a local `false`
update 500 ms after the most recent pulse, without sending that reset to Q-Sys.
Pending resets follow renames and cannot survive disconnects into a new session.
Reserved protocol messages and errors never enter `values`.

### Reacting to a door opening

In the Sharks installation, `true` means shut and `false` means open. The
first value after connecting is state, not an opening event. An application
can implement that distinction like this:

```qml
Item {
    property string doorParameter: "Entry Door"
    property var previousDoor: undefined
    signal doorOpened()

    Connections {
        target: Qsys
        function onUpdated(parameter, value) {
            if (parameter !== doorParameter || typeof value !== "boolean")
                return
            const opened = previousDoor === true && value === false
            previousDoor = value
            if (opened)
                doorOpened()
        }
        function onCleared() { previousDoor = undefined }
        function onParameterRenamed(oldParameter, newParameter) {
            if (doorParameter === oldParameter)
                doorParameter = newParameter
        }
    }
}
```

The generic service preserves incoming types; apps whose sensors send boolean
strings should normalize those explicitly. Show arming, lighting preset mapping,
and tablet/player relaying remain application responsibilities.

## Listboxes

Listboxes are frontend-owned at initialization. Populate them after discovery
or `snapshotReady()` on every connection:

```qml
Qsys.sendList("Stories", ["Story A", "Story B"]) // choices only
Qsys.selectListItem("Stories", "Story B")       // selection only
Qsys.sendList("Stories", [])                    // explicitly clear choices
```

A choices-only message omits `value`; it does not clear the current selection.
An empty list intentionally sends an empty array (unlike the original tester's
empty-list no-op). Selection confirmation arrives through `updated()`.

## C++ and build integration

```cmake
find_package(Dsqt REQUIRED COMPONENTS Core Qsys)
target_link_libraries(myApp PRIVATE Dsqt::Qsys)
```

For a static QML application, import the plugin in `main.cpp`, following the
same pattern as the other DsQt modules:

```cpp
#include <QtQml/qqmlextensionplugin.h>
#include <dsQsys.h>
Q_IMPORT_QML_PLUGIN(Dsqt_QsysPlugin)

// After constructing the application, on the application thread:
auto* qsys = dsqt::DsQsys::instance();
QObject::connect(qsys, &dsqt::DsQsys::updated, receiver,
                 [](const QString& parameter, const QVariant& value) {
    // Same updates and types as QML.
});
qsys->send("Lighting Preset 2", true);
```

QML and C++ share the same application-owned instance. Use its API on the
application thread; use queued Qt connections when invoking it from workers.
Qt Core, QML, Network, and WebSockets are linked along with Dsqt Core.
The ClonerSource template includes this module and its static plugin import.

## Compatibility and verification

The wire protocol follows `QSysServer`/`QSysListener` in the Elevate frontend
tester, also vendored unchanged in San-Jose-Sharks. DsQt implements the service
with settings and QML support, consistent rename metadata, session-safe pulse
timers, and one active plugin connection. It does not depend on either sibling
checkout at build time.

Enable `DSQT_BUILD_TESTS` and run `test_ds_qsys` through CTest. Tests cover real
localhost WebSocket traffic, QML singleton access and `Connections`, snapshots,
typed sends, listboxes, errors, renames, pulse timers, reconnects, and settings.
For manual testing, connect the sibling `Qsys-Dsqt-Tester` simulator to the
application's host and configured port. Hardware feedback timing depends on
the Q-Sys design; the simulator's immediate feedback is not a hardware guarantee.
