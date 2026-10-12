pragma ComponentBehavior: Bound
import QtQuick
import Dsqt.Core
import Dsqt.Waffles

Item {
    id: root
    property var slide: null
    property bool active: true
    // Explicit application frame selection is strict. Empty selects the closest authored aspect.
    property string frameUid: root.layout ? root.layout.frameUid || "" : ""
    readonly property var layout: slide ? slide.layout : null
    readonly property var frames: layout && layout.frames ? layout.frames : []
    readonly property var frame: {
        let best = null, distance = Infinity;
        const aspect = width / Math.max(1, height);
        for (const candidate of root.frames) {
            if (root.frameUid && candidate.uid === root.frameUid) return candidate;
            const delta = Math.abs(Math.log(aspect / (candidate.width / candidate.height)));
            if (delta < distance) { best = candidate; distance = delta; }
        }
        return root.frameUid ? null : best;
    }
    readonly property bool missingFrame: !!root.layout && !!root.frameUid && !root.frame
    property string _reportedMissingFrame: ""
    function _reportMissingFrame() {
        const key = root.missingFrame ? ((root.slide ? root.slide.uid : "") + ":" + root.frameUid) : "";
        if (key && key !== root._reportedMissingFrame)
            console.warn("[DsCustomLayoutTemplate] No authored content for frame", root.frameUid,
                         "on slide", root.slide ? root.slide.uid : "");
        root._reportedMissingFrame = key;
    }
    onMissingFrameChanged: Qt.callLater(root._reportMissingFrame)
    onFrameUidChanged: Qt.callLater(root._reportMissingFrame)
    onSlideChanged: Qt.callLater(root._reportMissingFrame)
    readonly property bool hasBackground: !!(layout && layout.background && layout.background.filepath)
    signal contentRequested(var target)

    Item {
        id: canvas
        objectName: "customLayoutCanvas"
        visible: !root.missingFrame
        anchors.centerIn: parent
        width: root.frame ? Math.min(root.width, root.height * root.frame.width / root.frame.height) : root.width
        height: root.frame ? width * root.frame.height / root.frame.width : root.height
        clip: true

        DsMediaViewer {
            id: background
            anchors.fill: parent
            media: root.hasBackground ? root.layout.background : null
            visible: root.hasBackground
            fillMode: Image.PreserveAspectCrop
            autoPlay: root.active
            loops: -1
            onMediaLoaded: if (!root.active && videoItem) videoItem.pause()
        }
        Binding {
            target: background.videoItem
            property: "volume"
            value: 0
            when: background.videoItem !== null
        }
        Connections {
            target: root
            function onActiveChanged() {
                if (!background.videoItem) return;
                if (root.active) background.videoItem.play();
                else background.videoItem.pause();
            }
        }

        Column {
            objectName: "customLayoutHeader"
            visible: !root.hasBackground
            x: canvas.width * 0.045
            y: canvas.height * 0.05
            width: canvas.width * 0.85
            spacing: canvas.height * 0.008
            Text {
                width: parent.width
                text: root.layout ? root.layout.tagline : ""
                visible: text.length > 0
                font.family: "Roboto"
                font.pixelSize: Math.max(12, canvas.height * 0.021)
                font.letterSpacing: 2
                color: DsTheme.accent
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            Text {
                width: parent.width
                text: root.layout ? root.layout.headline : ""
                font.family: "Roboto"
                font.pixelSize: Math.max(22, canvas.height * 0.057)
                font.weight: Font.Medium
                color: DsTheme.surfaceText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            Text {
                width: parent.width
                text: root.layout ? root.layout.subHeadline : ""
                visible: text.length > 0
                font.family: "Roboto"
                font.pixelSize: Math.max(14, canvas.height * 0.025)
                color: DsTheme.surfaceText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
        }

        Repeater {
            model: root.frame ? root.frame.items : []
            DsCustomLayoutMedia {
                required property var modelData
                item: modelData
                active: root.active
                x: modelData.x * canvas.width
                y: modelData.y * canvas.height
                width: modelData.w * canvas.width
                height: modelData.h * canvas.height
                onContentRequested: target => root.contentRequested(target)
            }
        }
    }
}
