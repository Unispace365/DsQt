import QtQuick
import Dsqt
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage

DsTitledMediaViewer {
            id: viewer

            controls: [
                TopOuterControls {
                    id: controlRoot
                    signalObject: viewer.signalObject
                    model: viewer.model
                    DragHandler {
                        target: viewer
                    }
                },
                RightOuterControls {
                    id: rightButtons
                    signalObject: viewer.signalObject
                }
            ]
        }

