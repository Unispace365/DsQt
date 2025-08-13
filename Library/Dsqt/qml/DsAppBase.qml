import QtQuick
import QtQuick.Controls
import Dsqt

/// \brief main window for DsQt QML applications.
ApplicationWindow {
    id: window
    visible: true
    flags: Qt.Window
    title: `${Application.name} (${Application.version})`

    /// Allow the application to quit by pressing ESCAPE. CTRL+Q is handled by the menu bar. ALT+F4 is supported by default.
    Shortcut {
        sequence: "ESCAPE"
        autoRepeat: false
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    /// Provides access to the window settings.
    DsSettingsProxy {
        id: windowProxy
        target: "engine"
        prefix: "engine.window"
    }

    /// Setup the window after construction.
    Component.onCompleted: {
        // Setup the window based on the settings.
        var mode = windowProxy.getString("mode", "window")         // "window", "display", "desktop"
        var displayName = windowProxy.getString("displayName", "") // If specified, will lookup the display index by name...
        var displayIndex = windowProxy.getInt("displayIndex", 1)   // ...otherwise, will use this index.
        var margin = windowProxy.getString("margin", 100)          // Allows a bit of space around the window.

        // Get the list of available screens.
        var screens = Qt.application.screens

        // Find display index if name is specified.
        if(displayName !== ""){
            for(var i=0; i<screens.length; ++i) {
                if(screens[i].name === displayName) {
                    displayIndex = i;
                    break;
                }
            }
        }

        // Default to main screen if index is invalid.
        if(displayIndex >= screens.length)
            displayIndex = 0

        // Adjust position and size based on specified mode.
        var preferredWidth = windowProxy.getInt("width", screens[displayIndex].width - 2 * margin)     //
        var preferredHeight = windowProxy.getInt("height", screens[displayIndex].height - 2 * margin)  //

        if(mode === "desktop") {
            // Create a borderless window.
            flags |= Qt.FramelessWindowHint

            // Find the origin.
            for(let i=0; i<screens.length; ++i) {
                x = Math.min(x, screens[i].virtualX)
                y = Math.min(y, screens[i].virtualY)
            }

            // Span the full desktop.
            for(i=0; i<screens.length; ++i) {
                width = Math.max(width, screens[i].width + (screens[i].virtualX - x))
                height = Math.max(height, screens[i].height + (screens[i].virtualY - y))
            }
        }
        else if(mode === "display" || (preferredWidth === screens[displayIndex].width && preferredHeight === screens[displayIndex].height)) {
            // Create a borderless window.
            flags |= Qt.FramelessWindowHint

            // Assign the window to its screen.
            screen = screens[displayIndex]

            // Move and resize the window.
            x = screen.virtualX
            y = screen.virtualY
            width = screen.width
            height = screen.height
        }
        else {
            // Assign the window to its screen.
            screen = screens[displayIndex]

            // Adjust the preferred dimensions if they don't fit on the selected screen.
            var availableWidth = screen.width - 2 * margin
            var availableHeight = screen.height - 2 * margin
            if(preferredWidth > availableWidth || preferredHeight > availableHeight ) {
                var preferredAspect = preferredWidth / preferredHeight
                var availableAspect = availableWidth / availableHeight
                if( availableAspect > preferredAspect) {
                    preferredHeight = Math.min(preferredHeight, availableHeight)
                    preferredWidth = preferredHeight * preferredAspect;
                } else {
                    preferredWidth = Math.min(preferredWidth, availableWidth)
                    preferredHeight = preferredWidth / preferredAspect;
                }
            }

            // Move and resize the window.
            x = screen.virtualX + 0.5 * (screen.width - preferredWidth)
            y = screen.virtualY + 0.5 * (screen.height - preferredHeight)
            width = preferredWidth
            height = preferredHeight
        }
    }

    /// Allow showing and hiding the menu bar by pressing E.
    Shortcut {
        sequence: "E"
        autoRepeat: false
        context: Qt.ApplicationShortcut
        onActivated: {
            windowMenuBar.visible = !windowMenuBar.visible
        }
    }

    /// The default menu bar.
    menuBar: DsAppMenuBar {
        id: windowMenuBar
        visible: false

        onLogsBridgeSyncTriggered: (isChecked) => { bridgeSyncLog.visible = isChecked }
    }

    /// TODO Application log viewer.

    /// BridgeSync log viewer.
    DsTextFileViewer {
        id: bridgeSyncLog
        title: "BridgeSync Log"
        file: Ds.env.expand("%LOCAL%/ds_waffles/logs/bridgesync.log")
        //screen: Window.screen

        onVisibleChanged: (isVisible) => { windowMenuBar.logsBridgeSyncChecked = isVisible }
    }

    /// TODO AppHost log viewer.
}
