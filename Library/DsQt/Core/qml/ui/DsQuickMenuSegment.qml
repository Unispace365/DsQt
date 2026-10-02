pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtCanvas2D

Item {
    id: root

    required property var entry
    property var config
    property url iconSource
    property url arrowSource
    property real innerRadius: 125
    property real outerRadius: 230
    property real startAngle: 0
    property real sweepAngle: 40
    property real revealProgress: 1
    property bool selected: false
    property bool interactive: true
    property color highlightColor: "#00ADF7"
    readonly property bool available: root.entry?.available !== false
    readonly property bool pointerHighlighted: root.enabled && root.available
        && (segmentHover.hovered || segmentTap.pressed)

    readonly property real middleAngle: (startAngle + sweepAngle * 0.5 - 90) * Math.PI / 180
    readonly property real labelRadius: (innerRadius + outerRadius) * 0.5
    readonly property real labelX: width * 0.5 + Math.cos(middleAngle) * labelRadius
    readonly property real labelY: height * 0.5 + Math.sin(middleAngle) * labelRadius
    readonly property real revealScale: 1 - Math.pow(1 - revealProgress, 3)
    property real highlightOpacity: root.available && selected ? 1 : 0

    signal highlighted(highlight: bool)
    signal selectedAt(position: point)
    onPointerHighlightedChanged: root.highlighted(root.pointerHighlighted)

    visible: revealProgress > 0 && outerRadius > innerRadius
    enabled: interactive && visible
    transform: Scale {
        origin.x: root.labelX
        origin.y: root.labelY
        xScale: root.revealScale
        yScale: root.revealScale
    }

    containmentMask: QtObject {
        function contains(point: point): bool {
            const dx = point.x - root.width * 0.5
            const dy = point.y - root.height * 0.5
            const distance = Math.sqrt(dx * dx + dy * dy)
            const angle = ((Math.atan2(dy, dx) * 180 / Math.PI + 90
                            - root.startAngle) % 360 + 360) % 360
            return distance > root.innerRadius && distance < root.outerRadius
                && angle < root.sweepAngle
        }
    }

    // Qt 6.12's Canvas2D technology preview renders through Qt Canvas Painter / QRhi.
    Canvas2D {
        id: canvas
        readonly property real padding: 24
        x: -padding
        y: -padding
        width: root.width + padding * 2
        height: root.height + padding * 2
        fillColor: "transparent"
        alphaBlending: true

        onPaint: {
            const ctx = canvas.getContext("2d")
            if (!ctx || root.outerRadius <= root.innerRadius)
                return
            ctx.reset()
            const cx = root.width * 0.5 + canvas.padding
            const cy = root.height * 0.5 + canvas.padding
            const start = (root.startAngle - 90) * Math.PI / 180
            const end = start + root.sweepAngle * Math.PI / 180

            ctx.beginPath()
            if (root.sweepAngle >= 360) {
                ctx.circle(cx, cy, root.outerRadius)
                ctx.beginHoleSubPath()
                ctx.circle(cx, cy, root.innerRadius)
            } else {
                ctx.arc(cx, cy, root.outerRadius, start, end, false)
                ctx.lineTo(cx + Math.cos(end) * root.innerRadius,
                           cy + Math.sin(end) * root.innerRadius)
                ctx.arc(cx, cy, root.innerRadius, end, start, true)
                ctx.closePath()
            }
            ctx.fillStyle = "#cc191f25"
            ctx.fill()
            ctx.strokeStyle = "#2E3439"
            ctx.lineWidth = 4
            ctx.stroke()

            if (root.highlightOpacity > 0) {
                ctx.beginPath()
                ctx.arc(cx, cy, Math.max(0, root.outerRadius - 2), start, end, false)
                ctx.moveTo(cx + Math.cos(start) * (root.innerRadius + 2),
                           cy + Math.sin(start) * (root.innerRadius + 2))
                ctx.arc(cx, cy, root.innerRadius + 2, start, end, false)
                ctx.strokeStyle = root.highlightColor
                ctx.globalAlpha = root.highlightOpacity * 0.45
                ctx.antialias = 10
                ctx.lineWidth = 8
                ctx.stroke()
                ctx.globalAlpha = root.highlightOpacity
                ctx.antialias = 1
                ctx.lineWidth = 4
                ctx.stroke()
            }
        }
    }

    onInnerRadiusChanged: canvas.requestPaint()
    onOuterRadiusChanged: canvas.requestPaint()
    onStartAngleChanged: canvas.requestPaint()
    onSweepAngleChanged: canvas.requestPaint()
    onHighlightOpacityChanged: canvas.requestPaint()
    onHighlightColorChanged: canvas.requestPaint()
    onVisibleChanged: {
        if (visible)
            canvas.requestPaint()
    }

    Behavior on highlightOpacity {
        NumberAnimation { duration: 200 }
    }

    Item {
        id: label
        x: root.labelX
        y: root.labelY
        opacity: root.revealProgress

        Image {
            id: icon
            source: root.iconSource
            x: -width * 0.5
            y: -height * 0.5
            width: root.entry?.iconWidth ?? implicitWidth
            height: root.entry?.iconHeight ?? implicitHeight
            sourceSize: Qt.size(root.entry?.iconWidth ?? 0, root.entry?.iconHeight ?? 0)
            fillMode: Image.PreserveAspectFit
        }

        Text {
            color: "#bbbbbb"
            font.pixelSize: root.config?.fontSize ?? 8
            font.family: root.config?.fontFamily ?? "Helvetica Nueue"
            font.weight: root.config?.fontWeight ?? 400
            horizontalAlignment: Text.AlignHCenter
            text: root.entry?.iconText ?? ""
            x: -width * 0.5
            y: root.iconSource.toString() ? icon.y + icon.height : -height * 0.5
        }
    }

    Item {
        width: 24
        height: 24
        x: root.width * 0.5 + Math.cos(root.middleAngle) * root.innerRadius * 0.9 - width * 0.5
        y: root.height * 0.5 + Math.sin(root.middleAngle) * root.innerRadius * 0.9 - height * 0.5
        rotation: root.startAngle + root.sweepAngle * 0.5
        opacity: root.highlightOpacity
        visible: opacity > 0

        Image {
            id: arrow
            anchors.fill: parent
            source: root.arrowSource
            sourceSize: Qt.size(24, 24)
            visible: false
        }
        MultiEffect {
            anchors.fill: arrow
            source: arrow
            colorization: 1
            colorizationColor: root.highlightColor
        }
    }

    HoverHandler {
        id: segmentHover
        enabled: root.available
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
    }

    TapHandler {
        id: segmentTap
        enabled: root.available
        onTapped: eventPoint => root.selectedAt(root.mapToGlobal(eventPoint.position))
    }
}
