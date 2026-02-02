pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Dsqt

/// \brief main window for DsQt QML applications.
ApplicationWindow {
    id: window
    flags: Qt.Window
    title: `${Application.name} (${Application.version})`
    visible: false // Will become visible once setup.
    color: "black"

    // Keep track of screen changes.
    onScreenChanged: {
        const screens = Qt.application.screens
        for(var i=0; i<screens.length; ++i) {
            if(window.screen.virtualX === screens[i].virtualX &&
               window.screen.virtualY === screens[i].virtualY) {
                _.displayIndex = i
                if(_.displayName !== "")
                _.displayName = screens[i].name
                return
            }
        }
    }

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
            const screens = Qt.application.screens
            console.log("screens",screens.length,_.displayIndex)

            if(_.displayName !== "") {
                for(var i=0; i<screens.length; ++i) {
                    if(screens[i].name === _.displayName) {
                        return i
                    }
                }
            }

            // Default to main screen if index is invalid.
            return _.displayIndex >= screens.length ? 0 : _.displayIndex
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
        window.visible = false

        // Create a borderless window.
        window.flags |= Qt.FramelessWindowHint

        // Find the origin.
        const screens = Qt.application.screens
        for(let i=0; i<screens.length; ++i) {
            window.x = Math.min(x, screens[i].virtualX)
            window.y = Math.min(y, screens[i].virtualY)
        }

        // Span the full desktop.
        for(i=0; i<screens.length; ++i) {
            window.width = Math.max(width, screens[i].width + (screens[i].virtualX - x))
            window.height = Math.max(height, screens[i].height + (screens[i].virtualY - y))
        }

        window.visible = true
    }

    function spanDisplay() {
        window.visible = false

        // Create a borderless window.
        window.flags |= Qt.FramelessWindowHint

        // Assign the window to its screen.
        const screens = Qt.application.screens
        window.screen = screens[_.getDisplayIndex()]

        console.log("window.screen",window.screen)

        // Move and resize the window.
        window.x = screen.virtualX
        window.y = screen.virtualY
        window.width = screen.width
        window.height = screen.height

        window.visible = true
    }

    function centerOnScreen() {
        window.visible = false

        // Create a bordered window.
        window.flags &= ~Qt.FramelessWindowHint

        // Assign the window to its screen.
        const screens = Qt.application.screens
        window.screen = screens[_.getDisplayIndex()]

        console.log("window.screen",window.screen.width,_.getDisplayIndex())

        // Adjust the preferred dimensions if they don't fit on the selected screen.
        var availableWidth = window.screen.width - 2 * _.margin
        var availableHeight = window.screen.height - 2 * _.margin
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
        window.x = window.screen.virtualX + 0.5 * (window.screen.width - preferredWidth)
        window.y = window.screen.virtualY + 0.5 * (window.screen.height - preferredHeight)
        window.width = preferredWidth
        window.height = preferredHeight

        window.visible = true
    }

    /// Setup the window after construction.
    Component.onCompleted: {
        //
        Qt.application.synthesizeMouseFromTouch = false
        Qt.application.synthesizeTouchFromMouse = false

        // Adjust position and size based on specified mode.
        if(_.mode === "desktop") { window.spanDesktop() }
        else if(_.mode === "display") { window.spanDisplay() }
        else { window.centerOnScreen() }
    }

    /// Allow full-screen toggle for windows.
    Shortcut {
        sequence: "F"
        autoRepeat: false
        context: Qt.ApplicationShortcut
        enabled: _.mode !== "desktop"
        onActivated: {
            if( window.flags & Qt.FramelessWindowHint ) { window.centerOnScreen() }
            else { window.spanDisplay() }
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
        palette: window.palette
        visible: false

        property DsTextFileViewer bridgeSyncLogWindow: null
        property DsContentBrowser contentBrowser: null
        onLogsBridgeSyncTriggered: (isChecked) => {
            if(bridgeSyncLogWindow === null) {
                bridgeSyncLogWindow = bridgeSyncLog.createObject(window)
                bridgeSyncLogWindow.closing.connect( () => { windowMenuBar.logsBridgeSyncChecked = false } )
            }
            bridgeSyncLogWindow.visible = isChecked
        }
        onContentBrowseToggled: (isChecked) => {
            if(contentBrowser === null) {
                contentBrowser = contentViewer.createObject(window)
                contentBrowser.closing.connect( () => { windowMenuBar.contentBrowseChecked = false } )
            }
            contentBrowser.visible = isChecked
        }
    }

    /// TODO Application log viewer.

    /// BridgeSync log viewer.
    Component {
        id: bridgeSyncLog
        DsTextFileViewer {
            title: "BridgeSync Log"
            file: Ds.env.expand("%LOCAL%/ds_waffles/logs/bridgesync.log")
            onVisibleChanged: (isVisible) => { windowMenuBar.logsBridgeSyncChecked = isVisible }
        }
    }

    /// Content browser.
    Component {
        id: contentViewer
        DsContentBrowser {
        }
    }

    /// TODO AppHost log viewer.
}
