import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import TouchTester
//an item that uses qt layouts to position items. it has a large font title and below that a horizontal flickable that
//contains a series of buttons with images and text below the image. the buttons should be square and clicking on one
//shows the image fullscreen.
Item {
    id: root
    anchors.fill: parent

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        //padding: 10

        Text {
            id: title
            text: "Image Gallery"
            font: Fnt.menuTitle
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }
    }

    PanAndZoom {
        id: panandzoom
        anchors.fill: parent
        Rectangle {
            x: (panandzoom.width - width) * 0.5
            y: (panandzoom.height - height) * 0.5
            width: 500
            height: 500
            id: targetRect
        }
        onDoubleTapped: {
            panandzoom.fitToView(true)
        }
    }
}
