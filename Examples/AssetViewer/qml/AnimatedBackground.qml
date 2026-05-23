import QtQuick

Item {
    id: root

    // Dark base so the saturated blobs read as vivid colour through the glass blur.
    Rectangle {
        anchors.fill: parent
        color: "#05060a"
    }

    // Vibrant drifting blobs. Each is a soft radial glow at high opacity so the glass picks up
    // strong colour. Component keeps the per-blob animation/gradient in one place.
    component Blob: Rectangle {
        id: blob
        property color glow: "#ff00ff"
        property real fromX: 0
        property real toX: 0.2
        property real fromY: 0
        property real toY: 0.2
        property int durX: 11000
        property int durY: 13000
        radius: width / 2
        color: "transparent"
        gradient: Gradient {
            GradientStop { position: 0.0; color: blob.glow }
            GradientStop { position: 1.0; color: "transparent" }
        }
        // radial-ish look via scaled gradient is approximate; keep simple soft circles.
        opacity: 0.75
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * blob.fromX; to: root.width * blob.toX; duration: blob.durX; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * blob.fromX; duration: blob.durX; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * blob.fromY; to: root.height * blob.toY; duration: blob.durY; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * blob.fromY; duration: blob.durY; easing.type: Easing.InOutSine }
        }
    }

    Blob { width: 520; height: 520; glow: "#ff1fa2"; opacity: 0.80; fromX: 0.02; toX: 0.20; fromY: 0.05; toY: 0.32; durX: 11000; durY: 14000 }
    Blob { width: 460; height: 460; glow: "#00e5ff"; opacity: 0.75; fromX: 0.62; toX: 0.80; fromY: 0.50; toY: 0.70; durX: 13000; durY: 10000 }
    Blob { width: 420; height: 420; glow: "#7c3aff"; opacity: 0.78; fromX: 0.28; toX: 0.46; fromY: 0.70; toY: 0.86; durX: 9000;  durY: 12000 }
    Blob { width: 480; height: 480; glow: "#ff7a00"; opacity: 0.72; fromX: 0.74; toX: 0.90; fromY: 0.04; toY: 0.22; durX: 10000; durY: 8000 }
    Blob { width: 380; height: 380; glow: "#22ff88"; opacity: 0.70; fromX: 0.46; toX: 0.60; fromY: 0.34; toY: 0.52; durX: 15000; durY: 11000 }
    Blob { width: 440; height: 440; glow: "#ff3b3b"; opacity: 0.70; fromX: 0.12; toX: 0.28; fromY: 0.58; toY: 0.78; durX: 12000; durY: 9500 }
    Blob { width: 400; height: 400; glow: "#3b6bff"; opacity: 0.78; fromX: 0.66; toX: 0.82; fromY: 0.36; toY: 0.54; durX: 8500;  durY: 13500 }
    Blob { width: 360; height: 360; glow: "#ffe14a"; opacity: 0.66; fromX: 0.40; toX: 0.55; fromY: 0.08; toY: 0.24; durX: 14000; durY: 10500 }
}
