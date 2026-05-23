import QtQuick
import Dsqt.Waffles
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage

DsControlSet {
    id: topButtons
    edge: DsControlSet.Edge.TopOuter
    height: titleContainer.height
    property var model: ({})

    // This control set paints its OWN glass background (the title strip), the same way the viewer
    // and other controls do — using the glass element + the context the viewer provides.
    DsGlassBackground {
        anchors.fill: parent
        z: -1
        context: topButtons.glass
        topLeftRadius: topButtons.glass ? topButtons.glass.radius : 12
        topRightRadius: topButtons.glass ? topButtons.glass.radius : 12
    }

    RowLayout {
        anchors.fill: topButtons
        spacing: 0



        Item {
            id: titleContainer
            height: 43
            width:topButtons.width
            // Background is provided by the viewer's glass panel behind this control set.

            Text {
                id:titleText
                font.family: "Roboto"
                font.pixelSize: 16
                height:26
                verticalAlignment: Text.AlignVCenter
                x:20
                y: 8.5
                color: DsTheme.surfaceText
                text: topButtons.model.title ?? "Media"
            }
        }

        Item { //spacer
            Layout.fillWidth: true
        }
    }
}
