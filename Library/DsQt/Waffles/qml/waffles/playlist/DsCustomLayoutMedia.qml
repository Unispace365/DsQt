pragma ComponentBehavior: Bound
import QtQuick
import Dsqt.Core
import Dsqt.Waffles

Item {
    id: root
    required property var item
    property bool active: true
    readonly property var media: item ? item.media : null
    readonly property var hotspots: item && item.hotspots ? item.hotspots : []
    readonly property bool movable: !!(item && item.touchEvents)
    readonly property real cropX: media && media.crop ? media.crop[0] : 0
    readonly property real cropY: media && media.crop ? media.crop[1] : 0
    readonly property real cropW: media && media.crop ? Math.max(0.0001, media.crop[2]) : 1
    readonly property real cropH: media && media.crop ? Math.max(0.0001, media.crop[3]) : 1
    readonly property real mediaAspect: media && media.width > 0 && media.height > 0
                                        ? media.width * cropW / (media.height * cropH) : width / Math.max(1, height)
    readonly property real paintedWidth: Math.min(width, height * mediaAspect)
    readonly property real paintedHeight: paintedWidth / Math.max(0.0001, mediaAspect)
    signal contentRequested(var target)
    objectName: item ? "customMedia:" + item.uid : "customMedia"

    DsMediaViewer {
        id: viewer
        anchors.fill: parent
        media: root.media
        fillMode: Image.PreserveAspectFit
        autoPlay: root.active && !!root.item.autoplay
        loops: root.item.loop ? -1 : 1
        page: root.item.pageNumber || 1
        onMediaLoaded: if (!root.active && videoItem) videoItem.pause()
    }
    Binding {
        target: viewer.videoItem
        property: "volume"
        value: root.item.volume === undefined ? 0.5 : root.item.volume
        when: viewer.videoItem !== null
    }
    onActiveChanged: {
        if (!viewer.videoItem) return;
        if (active && item.autoplay) viewer.videoItem.play();
        else viewer.videoItem.pause();
    }

    DragHandler {
        id: drag
        objectName: "customMediaDrag"
        target: root
        enabled: root.active && root.movable
        maximumPointCount: 1
        xAxis.minimum: -root.width / 2
        xAxis.maximum: root.parent ? root.parent.width - root.width / 2 : 0
        yAxis.minimum: -root.height / 2
        yAxis.maximum: root.parent ? root.parent.height - root.height / 2 : 0
    }
    PinchHandler {
        objectName: "customMediaPinch"
        target: root
        enabled: root.active && root.movable
        minimumScale: 0.25
        maximumScale: 4
        minimumRotation: 0
        maximumRotation: 0
    }

    Item {
        id: imageArea
        objectName: "customMediaImageArea"
        anchors.centerIn: parent
        width: root.paintedWidth
        height: root.paintedHeight
        clip: true
        Repeater {
            model: root.hotspots
            Item {
                id: hotspot
                required property var modelData
                objectName: "hotspot:" + modelData.uid
                x: (modelData.x - root.cropX) / root.cropW * imageArea.width
                y: (modelData.y - root.cropY) / root.cropH * imageArea.height
                width: modelData.w / root.cropW * imageArea.width
                height: modelData.h / root.cropH * imageArea.height
                enabled: root.active
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: modelData.title || qsTr("Open content")
                Accessible.onPressAction: root.contentRequested(hotspot.modelData.target)
                Keys.onReturnPressed: root.contentRequested(hotspot.modelData.target)
                Keys.onSpacePressed: root.contentRequested(hotspot.modelData.target)
                Rectangle {
                    anchors.fill: parent
                    color: tap.pressed ? "#33ffffff" : "transparent"
                    border.width: hover.hovered || hotspot.activeFocus ? 2 : 0
                    border.color: DsTheme.accent
                }
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    id: tap
                    gesturePolicy: TapHandler.DragThreshold
                    onTapped: root.contentRequested(hotspot.modelData.target)
                }
            }
        }
    }
}
