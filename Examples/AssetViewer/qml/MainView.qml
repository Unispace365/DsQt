pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Dsqt.Core
import Dsqt.Waffles
import AssetViewer

Item {
    id: mainView
    anchors.fill: parent

    property int nextSeed: 1

    // Local media sets downloaded into data/ (see data/images/gallery and data/videos).
    property var localImages: ["img1.jpg", "img2.jpg", "img3.jpg", "img4.jpg", "img5.jpg", "img6.jpg"]
    property var localVideos: [
        { "file": "big_buck_bunny.mp4", "title": "Big Buck Bunny", "w": 640, "h": 360 },
        { "file": "jellyfish.mp4",      "title": "Jellyfish",      "w": 640, "h": 360 },
        { "file": "sintel_trailer.mp4", "title": "Sintel Trailer", "w": 854, "h": 480 }
    ]

    // Opens a local image (1200x800) sized to its aspect ratio at the configured image width.
    function openLocalImage(file) {
        stage.createViewer({
            "model": { "title": file, "media": { "filepath": "%APP%/data/images/gallery/" + file, "type": "image", "width": 1200, "height": 800 } },
            "matchAspectRatio": true,
            "mediaFillMode": Image.PreserveAspectCrop,
            "glassFallbackColor": Qt.hsva(Math.random(), 0.7, 0.95, 1.0),
            "stage": stage
        })
    }

    // Opens a local video sized to its aspect ratio.
    function openLocalVideo(v) {
        stage.createViewer({
            "model": { "title": v.title, "media": { "filepath": "%APP%/data/videos/" + v.file, "type": "video", "width": v.w, "height": v.h } },
            "matchAspectRatio": true,
            "mediaFillMode": Image.PreserveAspectFit,
            "glassFallbackColor": Qt.hsva(Math.random(), 0.7, 0.95, 1.0),
            "stage": stage
        })
    }

    // --- Reusable themed toolbar controls (token colours, 40px hit target, hover +
    //     keyboard-focus affordance). ---
    component ActionButton: Button {
        id: ab
        implicitHeight: 72
        leftPadding: 28
        rightPadding: 28
        focusPolicy: Qt.StrongFocus
        background: Rectangle {
            radius: 12
            color: ab.down ? Qt.darker(DsTheme.surfaceVariant, 1.25)
                 : ab.hovered ? Qt.lighter(DsTheme.surfaceVariant, 1.15)
                 : DsTheme.surfaceVariant
            border.width: ab.visualFocus ? 4 : 2
            border.color: ab.visualFocus ? DsTheme.accent : DsTheme.stroke
        }
        contentItem: Text {
            text: ab.text
            font.pixelSize: 28
            color: DsTheme.surfaceText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // On/off toggle. State is shown by BOTH fill colour and a ●/○ glyph (never colour alone).
    component ToggleButton: Button {
        id: tb
        property bool on: false
        implicitHeight: 72
        leftPadding: 28
        rightPadding: 28
        focusPolicy: Qt.StrongFocus
        background: Rectangle {
            radius: 12
            color: tb.on ? (tb.down ? Qt.darker(DsTheme.accent, 1.2) : DsTheme.accent)
                         : (tb.hovered ? Qt.lighter(DsTheme.surfaceVariant, 1.15) : DsTheme.surfaceVariant)
            border.width: tb.visualFocus ? 4 : 2
            border.color: tb.visualFocus ? DsTheme.accent : (tb.on ? DsTheme.accent : DsTheme.stroke)
        }
        contentItem: Text {
            text: (tb.on ? "● " : "○ ") + tb.text
            font.pixelSize: 28
            color: tb.on ? DsTheme.accentText : DsTheme.surfaceText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    DsWaffleStage {
        id: stage
        anchors.fill: parent

        // Everything below the viewers and captured by the glass: animated background + gallery UI.
        backgroundContent: [
            AnimatedBackground { id: animBg; anchors.fill: parent },

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 48
                spacing: 32

                // --- Header (wayfinding) ---
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        id: title
                        text: "Asset Viewer"
                        font.family: "Roboto"
                        font.pixelSize: 56
                        font.weight: Font.Medium
                        color: DsTheme.surfaceText
                    }
                    Text {
                        text: "Open media into glass viewers — drag to move, ✕ to close."
                        font.pixelSize: 24
                        color: Qt.rgba(DsTheme.surfaceText.r, DsTheme.surfaceText.g, DsTheme.surfaceText.b, 0.6)
                    }
                }

                // --- Toolbar. A Flow wraps the controls onto more rows when space is tight, so
                //     they never run off-screen regardless of window width. ---
                Rectangle {
                    Layout.fillWidth: true
                    radius: 20
                    color: Qt.rgba(DsTheme.surface.r, DsTheme.surface.g, DsTheme.surface.b, 0.55)
                    border.color: DsTheme.stroke
                    border.width: 2
                    implicitHeight: toolbar.implicitHeight + 48

                    Flow {
                        id: toolbar
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 20

                        ActionButton {
                            text: "Add Image"
                            onClicked: {
                                let seed = nextSeed++
                                stage.createViewer({
                                    "model": { "title": "Image " + seed, "media": { "filepath": "https://picsum.photos/seed/" + seed + "/800/600", "type": "image", "width": 800, "height": 600 } },
                                    "matchAspectRatio": true,
                                    "mediaFillMode": Image.PreserveAspectCrop,
                                    "glassFallbackColor": Qt.hsva(Math.random(), 0.7, 0.95, 1.0),
                                    "stage": stage
                                })
                            }
                        }

                        ActionButton {
                            text: "Add Web"
                            onClicked: {
                                stage.createViewer({
                                    "model": { "title": "Web Viewer", "media": { "filepath": "https://example.com", "type": "web" } },
                                    "viewerWidth": 1100,
                                    "viewerHeight": 800,
                                    "enterAnimation": DsViewer.Anim.Fade,
                                    "exitAnimation": DsViewer.Anim.Fade,
                                    "glassFallbackColor": Qt.hsva(Math.random(), 0.7, 0.95, 1.0),
                                    "stage": stage
                                })
                            }
                        }

                        ActionButton {
                            text: "Add Local Image"
                            onClicked: {
                                // Local file via %APP% expansion (resolved to a file:// URL by the viewer).
                                stage.createViewer({
                                    "model": { "title": "Local Image", "media": { "filepath": "%APP%/data/images/landscape.jpeg", "type": "image" } },
                                    "viewerWidth": 1000,
                                    "viewerHeight": 680,
                                    "mediaFillMode": Image.PreserveAspectFit,
                                    "glassFallbackColor": Qt.hsva(Math.random(), 0.7, 0.95, 1.0),
                                    "stage": stage
                                })
                            }
                        }

                        ActionButton {
                            text: "Add Network Video"
                            onClicked: {
                                // Network video (mp4). Local videos work the same way via a %APP% path.
                                stage.createViewer({
                                    "model": { "title": "Network Video", "media": { "filepath": "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/360/Big_Buck_Bunny_360_10s_1MB.mp4", "type": "video", "width": 640, "height": 360 } },
                                    "matchAspectRatio": true,
                                    "mediaFillMode": Image.PreserveAspectFit,
                                    "enterAnimation": DsViewer.Anim.FadeRise,
                                    "glassFallbackColor": Qt.hsva(Math.random(), 0.7, 0.95, 1.0),
                                    "stage": stage
                                })
                            }
                        }

                        ActionButton {
                            text: "Add Glass"
                            onClicked: {
                                // No media + media-region glass on = a pure glass panel to compare to Figma.
                                stage.createViewer({
                                    "model": { "title": "Glass", "media": {} },
                                    "viewerWidth": 900,
                                    "viewerHeight": 560,
                                    "mediaGlassEnabled": true,
                                    "glassFallbackColor": Qt.hsva(Math.random(), 0.7, 0.95, 1.0),
                                    "stage": stage
                                })
                            }
                        }

                        // Group separator between content actions and view toggles.
                        Rectangle { width: 2; height: 72; color: DsTheme.stroke; opacity: 0.6 }

                        ToggleButton {
                            text: "Glass"
                            on: stage.glassEnabled
                            onClicked: stage.glassEnabled = !stage.glassEnabled
                        }
                        ToggleButton {
                            text: "Background"
                            on: animBg.animated
                            onClicked: animBg.animated = !animBg.animated
                        }
                    }
                }

                // --- Local image gallery ---
                Text {
                    text: "Local Images"
                    font.pixelSize: 36
                    color: DsTheme.surfaceText
                }

                Flickable {
                    id: imageGallery
                    contentWidth: imageRow.width
                    contentHeight: imageRow.height
                    clip: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 230

                    Row {
                        id: imageRow
                        spacing: 24
                        height: 220

                        Repeater {
                            model: mainView.localImages

                            Button {
                                id: imgThumb
                                required property string modelData
                                width: 300
                                height: 220
                                focusPolicy: Qt.StrongFocus
                                background: Rectangle {
                                    radius: 12
                                    color: DsTheme.surfaceVariant
                                    border.width: imgThumb.visualFocus || imgThumb.hovered ? 4 : 2
                                    border.color: imgThumb.visualFocus || imgThumb.hovered ? DsTheme.accent : DsTheme.stroke
                                }

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    source: Ds.env.expandUrl("%APP%/data/images/gallery/" + imgThumb.modelData)
                                    fillMode: Image.PreserveAspectCrop
                                    sourceSize.width: 600
                                    clip: true
                                }

                                onClicked: mainView.openLocalImage(modelData)
                            }
                        }
                    }
                }

                // --- Local video gallery ---
                Text {
                    text: "Local Videos"
                    font.pixelSize: 36
                    color: DsTheme.surfaceText
                }

                Flickable {
                    id: videoGallery
                    contentWidth: videoRow.width
                    contentHeight: videoRow.height
                    clip: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 230

                    Row {
                        id: videoRow
                        spacing: 24
                        height: 220

                        Repeater {
                            model: mainView.localVideos

                            Button {
                                id: vidThumb
                                required property var modelData
                                width: 380
                                height: 220
                                focusPolicy: Qt.StrongFocus
                                background: Rectangle {
                                    radius: 12
                                    color: DsTheme.surfaceVariant
                                    border.width: vidThumb.visualFocus || vidThumb.hovered ? 4 : 2
                                    border.color: vidThumb.visualFocus || vidThumb.hovered ? DsTheme.accent : DsTheme.stroke
                                }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 12

                                    Text {
                                        text: "▶"
                                        color: DsTheme.accent
                                        font.pixelSize: 56
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }
                                    Text {
                                        text: vidThumb.modelData.title
                                        color: DsTheme.surfaceText
                                        font.pixelSize: 28
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }
                                }

                                onClicked: mainView.openLocalVideo(modelData)
                            }
                        }
                    }
                }

                // Absorbs remaining vertical space so the galleries sit just under the toolbar.
                Item { Layout.fillWidth: true; Layout.fillHeight: true }
            }
        ]
    }
}
