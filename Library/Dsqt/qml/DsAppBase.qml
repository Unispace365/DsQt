import QtQuick
import QtQuick.Controls
import Dsqt

/// \brief main window for DsQt QML applications.
ApplicationWindow {
    id: window
    flags: Qt.Window
    title: `${Application.name} (${Application.version})`
    visible: false // Will become visible once setup.
    color: "black"

    // Encapsulte properties to make them private.
    Item {
        id: _
        property string mode: windowProxy.getString("mode", "window")            // "window", "display", "desktop"
        property real margin: windowProxy.getInt("margin", 100)                  // Default to 0.
        property string displayName : windowProxy.getString("displayName", "")   // If specified, will lookup the display index by name...
        property int  displayIndex : windowProxy.getInt("displayIndex", 1)       // ...otherwise, will use this index.
        property int preferredWidth : windowProxy.getInt("width", 0)             //
        property int preferredHeight : windowProxy.getInt("height", 0)           //

        function getDisplayIndex() : int {
            var screens = Qt.application.screens

            if(displayName !== "") {
                for(var i=0; i<screens.length; ++i) {
                    if(screens[i].name === displayName) {
                        return i
                    }
                }
            }

            // Default to main screen if index is invalid.
            return displayIndex >= screens.length ? 0 : displayIndex
        }
    }

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

    function spanDesktop() {
        visible = false

        // Create a borderless window.
        flags |= Qt.FramelessWindowHint

        // Find the origin.
        var screens = Qt.application.screens
        for(let i=0; i<screens.length; ++i) {
            x = Math.min(x, screens[i].virtualX)
            y = Math.min(y, screens[i].virtualY)
        }

        // Span the full desktop.
        for(i=0; i<screens.length; ++i) {
            width = Math.max(width, screens[i].width + (screens[i].virtualX - x))
            height = Math.max(height, screens[i].height + (screens[i].virtualY - y))
        }

        visible = true
    }

    function spanDisplay() {
        visible = false

        // Create a borderless window.
        flags |= Qt.FramelessWindowHint

        // Assign the window to its screen.
        var screens = Qt.application.screens
        screen = screens[_.getDisplayIndex()]

        // Move and resize the window.
        x = screen.virtualX
        y = screen.virtualY
        width = screen.width
        height = screen.height

        visible = true
    }

    function centerOnScreen() {
        visible = false

        // Create a bordered window.
        flags &= ~Qt.FramelessWindowHint

        // Assign the window to its screen.
        var screens = Qt.application.screens
        screen = screens[_.getDisplayIndex()]

        // Adjust the preferred dimensions if they don't fit on the selected screen.
        var availableWidth = screen.width - 2 * _.margin
        var availableHeight = screen.height - 2 * _.margin
        var preferredWidth = _.preferredWidth > 0 ? _.preferredWidth : availableWidth
        var preferredHeight = _.preferredHeight > 0 ? _.preferredHeight : availableHeight
        if(preferredWidth === availableWidth && preferredHeight === availableHeight ) {
            return spanDisplay()
        }
        else if(preferredWidth > availableWidth || preferredHeight > availableHeight ) {
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

        visible = true
    }

    /// Setup the window after construction.
    Component.onCompleted: {
        // Adjust position and size based on specified mode.
        if(_.mode === "desktop") { spanDesktop() }
        else if(_.mode === "display") { spanDisplay() }
        else { centerOnScreen() }
    }

    /// Allow full-screen toggle for windows.
    Shortcut {
        sequence: "F"
        autoRepeat: false
        context: Qt.ApplicationShortcut
        enabled: _.mode !== "desktop"
        onActivated: {
            if( flags & Qt.FramelessWindowHint ) { centerOnScreen() }
            else { spanDisplay() }
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

    Component {
        id: contentViewer
        DsContentBrowser {
        }
    }

    /// The default menu bar.
    menuBar: DsAppMenuBar {
        id: windowMenuBar
        visible: false

        property DsContentBrowser contentBrowser: null
        onLogsBridgeSyncTriggered: (isChecked) => { bridgeSyncLog.visible = isChecked }
        onContentBrowseToggled: (isChecked) => {
            if(isChecked) {
                if(contentBrowser === null) {
                    contentBrowser = contentViewer.createObject(window)
                    contentBrowser.closing.connect( () => { windowMenuBar.contentBrowseChecked = false } )                }
                contentBrowser.visible = true
            } else if(contentBrowser !== null) {
                contentBrowser.visible = false
            }
        }
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
