import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt

Button {
    id: root
    height: 37
    width: 37
    padding: 0
    property var signalObject: null
    property string iconSource: ""
    property int edge: DsControlSet.Edge.RightOuter

    background: Rectangle {
        color: palette.button
        border.width: 1
        border.color: palette.light
        anchors.fill: parent
        topRightRadius: edge === DsControlSet.Edge.RightOuter ||
                        edge === DsControlSet.Edge.TopOuter ||
                        edge === DsControlSet.Edge.BottomInner ||
                        edge === DsControlSet.Edge.LeftInner ? 20 : 0
        topLeftRadius: edge === DsControlSet.Edge.LeftOuter ||
                        edge === DsControlSet.Edge.TopOuter ||
                        edge === DsControlSet.Edge.BottomInner ||
                        edge === DsControlSet.Edge.RightInner ? 20 : 0
        bottomLeftRadius: edge === DsControlSet.Edge.LeftOuter ||
                        edge === DsControlSet.Edge.BottomOuter ||
                        edge === DsControlSet.Edge.TopInner ||
                        edge === DsControlSet.Edge.RightInner ? 20 : 0
        bottomRightRadius: edge === DsControlSet.Edge.RightOuter ||
                        edge === DsControlSet.Edge.BottomOuter ||
                        edge === DsControlSet.Edge.TopInner ||
                        edge === DsControlSet.Edge.LeftInner ? 20 : 0
    }
    Item {
        x: {
            switch(edge){
                case DsControlSet.Edge.TopOuter:
                case DsControlSet.Edge.TopInner:
                case DsControlSet.Edge.BottomOuter:
                case DsControlSet.Edge.BottomInner:
                    return (parent.width - 16)/2;
                    break;
                case DsControlSet.Edge.RightOuter:
                case DsControlSet.Edge.LeftInner:
                    return 6;
                    break;
                case DsControlSet.Edge.LeftOuter:
                case DsControlSet.Edge.RightInner:
                    return parent.width - 16 - 6;
                    break;
            }
        }

        y: {
            switch(edge){
                case DsControlSet.Edge.LeftOuter:
                case DsControlSet.Edge.LeftInner:
                case DsControlSet.Edge.RightOuter:
                case DsControlSet.Edge.RightInner:
                    return (parent.height - 16)/2;
                    break;
                case DsControlSet.Edge.TopOuter:
                case DsControlSet.Edge.BottomInner:
                    return parent.height - 16 - 6;
                    break;
                case DsControlSet.Edge.BottomOuter:
                case DsControlSet.Edge.TopInner:
                    return 6;
                    break;
            }
        }
        width:16
        height:16

        VectorImage {
            id:icon
            anchors.fill: parent
            source: Ds.env.expandUrl(iconSource)
            preferredRendererType: VectorImage.CurveRenderer
            layer.enabled: false
            layer.live: true
            layer.effect: MultiEffect  {
                colorization: 1.0
                colorizationColor: palette.text
            }
        }



    }

}
