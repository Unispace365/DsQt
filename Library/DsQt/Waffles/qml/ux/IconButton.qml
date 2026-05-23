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

    // Offset of this button's background within the viewer; combined with the viewer's stage
    // position to sample the right slice of the glass. Recomputed (via refresh) on viewer move.
    readonly property point _bgOff: {
        if (!glass || !glass.viewerItem)
            return Qt.point(0, 0)
        let r = glass.refresh
        return bg.mapToItem(glass.viewerItem, 0, 0)
    }

    background: DsGlassBackground {
        id: bg
        source: root.glass ? root.glass.source : null
        sampleX: root.glass && root.glass.viewerItem ? root.glass.viewerX + root._bgOff.x : 0
        sampleY: root.glass && root.glass.viewerItem ? root.glass.viewerY + root._bgOff.y : 0
        blurEnabled: root.glass ? root.glass.enabled : false
        tint: root.glass ? root.glass.tint : palette.button
        tintOpacity: root.glass ? root.glass.tintOpacity : 1.0
        blur: root.glass ? root.glass.blur : 0.5
        blurMax: root.glass ? root.glass.blurMax : 32
        // Controls keep their own off-state colour rather than the viewer's fallback.
        fallbackColor: palette.button
        topLeftRadius: root._tlr
        topRightRadius: root._trr
        bottomLeftRadius: root._blr
        bottomRightRadius: root._brr

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1
            border.color: palette.light
            topLeftRadius: root._tlr
            topRightRadius: root._trr
            bottomLeftRadius: root._blr
            bottomRightRadius: root._brr
        }
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
