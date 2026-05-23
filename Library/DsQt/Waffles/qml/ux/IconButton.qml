import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt.Core
import Dsqt.Waffles

Button {
    id: root
    height: 37
    width: 37
    padding: 0
    property var signalObject: null
    property string iconSource: ""
    property int edge: DsControlSet.Edge.RightOuter
    // Glass context from the owning control set (set by the viewer). When present and enabled
    // the button background is glass; otherwise it keeps its own solid look.
    property var glass: null

    readonly property real _tlr: edge === DsControlSet.Edge.LeftOuter ||
                                 edge === DsControlSet.Edge.TopOuter ||
                                 edge === DsControlSet.Edge.BottomInner ||
                                 edge === DsControlSet.Edge.RightInner ? 20 : 0
    readonly property real _trr: edge === DsControlSet.Edge.RightOuter ||
                                 edge === DsControlSet.Edge.TopOuter ||
                                 edge === DsControlSet.Edge.BottomInner ||
                                 edge === DsControlSet.Edge.LeftInner ? 20 : 0
    readonly property real _blr: edge === DsControlSet.Edge.LeftOuter ||
                                 edge === DsControlSet.Edge.BottomOuter ||
                                 edge === DsControlSet.Edge.TopInner ||
                                 edge === DsControlSet.Edge.RightInner ? 20 : 0
    readonly property real _brr: edge === DsControlSet.Edge.RightOuter ||
                                 edge === DsControlSet.Edge.BottomOuter ||
                                 edge === DsControlSet.Edge.TopInner ||
                                 edge === DsControlSet.Edge.LeftInner ? 20 : 0

    // Button background uses the same glass element + context as the viewer/control sets; it
    // self-samples its own slice from the context's slot. Controls keep their own off-state colour.
    background: DsGlassBackground {
        id: bg
        context: root.glass
        fallbackColor: palette.button
        topLeftRadius: root._tlr
        topRightRadius: root._trr
        bottomLeftRadius: root._blr
        bottomRightRadius: root._brr
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
