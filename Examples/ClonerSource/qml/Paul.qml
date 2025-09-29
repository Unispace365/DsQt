import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.VirtualKeyboard
import Dsqt
import WhiteLabelWaffles

Item {

    // Allows us to measure the exact text size.
    DsTextMeasurer {
        id: textMeasurer
    }

    // TODO Theme.
    Rectangle {
        id: themeBase
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Vertical // or Gradient.Horizontal
            GradientStop { position: 0.0; color: "pink" }
            GradientStop { position: 1.0; color: "red" }
        }
    }

    ShaderEffectSource {
        // Captures the theme into a texture.
        id: theme
        sourceItem: themeBase
        hideSource: true
    }

    //
    DsEventSchedule {
        id: schedule
        clock: clock
    }

    DsPlaylist {
        id: playlist
        interval: 15000
        templates: {
            "z9P90ysGZv4V" : "file:%APP%/qml/templates/FullScreenMedia.qml",
            "uTt8HaFq871J" : "file:%APP%/qml/templates/Agenda.qml",
        }
        components: {
            "z9P90ysGZv4V" : FullScreenMedia,
            "uTt8HaFq871J" : Agenda,
        }
        modes: {
            "ambient":{"platformKey":"default_ambient_playlist","eventKey":"ambient_playlist"},
            "interactive":{"platformKey":"default_interactive_playlist","eventKey":"interactive_playlist"}
        }
        mode: Ds.engine.idle.idling ? "ambient" : "interactive"

        event: schedule.current
    }

    DsEditableSource {
        id: edit
        source: playlist.source
    }

    // DsCrossFadingLoader {
    //     id: main
    //     anchors.fill: parent
    //     source: edit.source
    //     //sourceComponent: playlist.component
    //     fadeDuration: 500
    //     fadeDelay: 500
    //     //model: playlist.model
    // }

    Loader {
        id: editable
        anchors.fill: parent

        Connections {
            target: playlist
            function onModelChanged() {
                if(edit.source && playlist.model)
                    editable.setSource(edit.source, { model: playlist.model })
            }
        }
        Connections {
            target: edit
            function onSourceChanged() {
                if(edit.source && playlist.model)
                    editable.setSource(edit.source, { model: playlist.model })
            }
        }
    }
}

