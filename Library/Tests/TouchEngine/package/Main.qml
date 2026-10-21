import QtQuick
import Dsqt.TouchEngine

Item {
    width: 64
    height: 64

    DsTouchEngineSession {
        id: touchEngineSession
        objectName: "touchEngineSession"
    }

    DsTouchEngineTextureView {
        objectName: "touchEngineView"
        anchors.fill: parent
        session: touchEngineSession
    }
}
