import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.VirtualKeyboard
import Dsqt
import "qml"

/// Sets up our window, menu bar and basic functionality.
DsAppBase {
    id: base

    // Load fonts.
    FontLoader {
        id: helveticaNeueThin
        source: Ds.env.expand("file://%APP%/fonts/HelveticaNeueThin.otf")
    }
    FontLoader {
        id: helveticaNeueMedium
        source: Ds.env.expand("file://%APP%/fonts/HelveticaNeueMedium.otf")
    }
    FontLoader {
        id: helveticaNeueRoman
        source: Ds.env.expand("file://%APP%/fonts/HelveticaNeueRoman.otf")
    }

    // Allows us to measure the exact text size.
    DsTextMeasurer {
        id: textMeasurer
    }

    // Allow zooming and panning of the content.
    DsFitView {
        id: fit
        anchors.fill: parent
        preferredWidth: 1920
        preferredHeight: 1080
        fitEnabled: false

        Loader {
            id: main
            source: "/qt/qml/ClonerSource/qml/MainView.qml"
            anchors.fill: parent
        }

        DsClock {
            id: clock
            //speed: 1800
        }

        CustomClock {
            anchors.fill: parent
            clock: clock
        }

        // Press S to toggle fit-to-view
        Shortcut {
            sequence: "S"
            autoRepeat: false
            context: Qt.ApplicationShortcut
            onActivated: fit.fitEnabled = !fit.fitEnabled
        }

        // Press 0 to toggle fit-to-original (only active if fit-to-view is enabled)
        Shortcut {
            sequence: "0"
            autoRepeat: false
            context: Qt.ApplicationShortcut
            enabled: fit.fitEnabled
            onActivated: fit.fitOriginal = !fit.fitOriginal
        }

        // Press SPACE to toggle fit-centered (only active if fit-to-view is enabled)
        Shortcut {
            sequence: "SPACE"
            autoRepeat: false
            context: Qt.ApplicationShortcut
            enabled: fit.fitEnabled
            onActivated: fit.fitCentered = !fit.fitCentered
        }
    }
}

