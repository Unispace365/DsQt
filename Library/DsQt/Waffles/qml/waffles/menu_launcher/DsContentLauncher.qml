pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.VectorImage
import Dsqt.Core
import Dsqt.Waffles

// Reusable Content Launcher panel (Phase-2b.2 visuals).
//
// Bridge-agnostic: the launcher is driven entirely by a generic `model` object that exposes the
// adapter contract documented below. Apps provide their own adapter (e.g. `DsContentLauncherModel`
// from `Dsqt.Bridge`).
//
// Required `model` shape:
//   - displayedItems  : array of items { uid, title, kind, thumbnail, media, hasChildren, record }
//   - currentPlaylist : item or null (rendered at the top when present)
//   - path            : array of items (breadcrumb)
//   - atRoot          : bool
//   - enter(item)     : drill into a folder/playlist
//   - back()          : pop one level
//
// On a leaf-row tap the launcher emits `openRequested(item)` so the app can map it to the right
// viewer (image/video/pdf/web etc.) and call `stage.openViewer`. On a hasChildren-row tap it
// calls `model.enter(item)`, and the list re-binds to `model.displayedItems`.
//
// Icons are resolved from `%APP%/data/images/waffles/content_launcher/<name>.svg` via the
// app's environment expansion — apps drop their own icon set there (the demo set is in
// `ECPresenter/data/images/waffles/content_launcher/`).
DsViewer {
    id: root
    viewerType: DsViewer.ViewerType.Launcher

    // Stage reference, populated by the stage when it instantiates the launcher.
    required property var stage

    // The bridge-agnostic data adapter (see contract above).
    property var model: null

    // Emitted when a leaf (non-folder/non-playlist) row is tapped.
    signal openRequested(var item)

    // Show / hide. The close button hides; the stage's double-tap re-shows + repositions.
    property bool shown: true
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

    // Top-of-panel toggle. "search" is a placeholder for 2c; for now it just renders an empty pane.
    property string viewMode: "content"

    viewerWidth: 453
    viewerHeight: 762
    width: viewerWidth
    height: viewerHeight

    // Initial position — centred horizontally near the top of the stage.
    Component.onCompleted: {
        if (parent) {
            x = Math.round(parent.width / 2 - width / 2);
            y = 100;
        }
    }

    // Resolve an icon URL by short name (no extension).
    function _icon(name) {
        return Ds.env.expandUrl("%APP%/data/images/waffles/content_launcher/" + name + ".svg");
    }

    // Map an item's kind to the leading icon name (for folders / playlists).
    function _leadingIconFor(kind) {
        if (kind === "folder")   return "folder";
        if (kind === "playlist") return "playlist";
        return "";
    }

    // Map an item's kind to the trailing type icon (for media leaves).
    function _typeIconFor(kind) {
        if (kind === "image")   return "image";
        if (kind === "video")   return "video";
        if (kind === "pdf")     return "pdf";
        if (kind === "weblink") return "weblink";
        return "";
    }

    // -----------------------------------------------------------------------------------------
    // Reusable inline components.
    // -----------------------------------------------------------------------------------------

    // Colourised vector icon. Retints the SVG to `color` via MultiEffect — keeps SVGs theme-aware.
    component DsLauncherIcon: Item {
        id: dlIcon
        property url source
        property color color: DsTheme.surfaceText
        property int sourceSize: 20
        width: sourceSize
        height: sourceSize
        VectorImage {
            anchors.fill: parent
            source: dlIcon.source
            preferredRendererType: VectorImage.CurveRenderer
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: dlIcon.color
            }
        }
    }

    // Content / Search tab pill.
    component TabPill: Rectangle {
        id: pill
        property string label
        property string iconName
        property bool active: false
        signal clicked()
        implicitWidth: 130
        implicitHeight: 46
        radius: 23
        color: pill.active ? DsTheme.selectedSurface : DsTheme.surfaceVariant
        border.color: DsTheme.stroke
        border.width: pill.active ? 0 : 1
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 16
            spacing: 8
            DsLauncherIcon {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                sourceSize: 22
                source: root._icon(pill.iconName)
                color: pill.active ? DsTheme.selectedSurfaceText : DsTheme.surfaceText
            }
            Text {
                Layout.fillWidth: true
                text: pill.label
                color: pill.active ? DsTheme.selectedSurfaceText : DsTheme.surfaceText
                font.family: "Roboto"
                font.pixelSize: 16
                verticalAlignment: Text.AlignVCenter
            }
        }
        TapHandler { onTapped: pill.clicked() }
    }

    // A single launcher row — 34px leading slot (thumbnail or kind-icon), title, trailing icon.
    // Highlights in selectedSurface on `selected`. Used by both the current-playlist row and the
    // ListView delegate.
    component LauncherRow: Item {
        id: lrow
        property var item: null
        property bool selected: false
        signal activated()

        implicitHeight: 52

        // Selected-row background (Primary-50 across the full row).
        Rectangle {
            anchors.fill: parent
            color: lrow.selected ? DsTheme.selectedSurface : "transparent"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 12

            // Leading 34x34 — thumbnail for media leaves, kind-icon-on-Tonal-10 for folders / playlists.
            Item {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34

                // Filled background for folders / playlists (no thumbnail).
                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: DsTheme.surfaceVariant
                    visible: lrow.item && (lrow.item.kind === "folder" || lrow.item.kind === "playlist")
                }
                // Kind icon for folders / playlists.
                DsLauncherIcon {
                    anchors.centerIn: parent
                    visible: lrow.item && (lrow.item.kind === "folder" || lrow.item.kind === "playlist")
                    sourceSize: 20
                    source: lrow.item ? root._icon(root._leadingIconFor(lrow.item.kind)) : ""
                    color: lrow.selected ? DsTheme.selectedSurfaceText : DsTheme.surfaceText
                }

                // Thumbnail for media leaves — rounded via MultiEffect mask.
                Image {
                    id: thumbImg
                    anchors.fill: parent
                    source: (lrow.item && lrow.item.thumbnail) ? lrow.item.thumbnail : ""
                    visible: false
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    smooth: true
                    sourceSize.width: 68
                    sourceSize.height: 68
                }
                Rectangle {
                    id: thumbMask
                    anchors.fill: parent
                    radius: 10
                    visible: false
                    layer.enabled: true
                }
                MultiEffect {
                    anchors.fill: parent
                    visible: lrow.item && lrow.item.thumbnail
                          && lrow.item.kind !== "folder" && lrow.item.kind !== "playlist"
                    source: thumbImg
                    maskEnabled: true
                    maskSource: thumbMask
                }
            }

            // Title.
            Text {
                Layout.fillWidth: true
                text: lrow.item ? lrow.item.title : ""
                color: lrow.selected ? DsTheme.selectedSurfaceText : DsTheme.surfaceText
                font.family: "Roboto"
                font.pixelSize: 14
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            // Trailing — type icon (16x16) for media, chevron for hasChildren, empty otherwise.
            DsLauncherIcon {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                sourceSize: 16
                visible: lrow.item && (lrow.item.hasChildren || root._typeIconFor(lrow.item.kind) !== "")
                source: {
                    if (!lrow.item) return "";
                    if (lrow.item.hasChildren) return root._icon("next");
                    const t = root._typeIconFor(lrow.item.kind);
                    return t ? root._icon(t) : "";
                }
                color: lrow.selected ? DsTheme.selectedSurfaceText
                                     : Qt.rgba(DsTheme.surfaceText.r, DsTheme.surfaceText.g, DsTheme.surfaceText.b, 0.75)
            }
        }

        // Thin Tonal-10 divider at the bottom of every row (Figma look).
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: DsTheme.stroke
            opacity: lrow.selected ? 0 : 0.6
        }

        TapHandler { onTapped: lrow.activated() }
    }

    // -----------------------------------------------------------------------------------------
    // The panel.
    // -----------------------------------------------------------------------------------------

    DsGlassBackground {
        anchors.fill: parent
        tintOpacity: 0.9
        topLeftRadius: 20
        topRightRadius: 20
        bottomLeftRadius: 20
        bottomRightRadius: 20
        fallbackColor: Qt.rgba(DsTheme.surface.r, DsTheme.surface.g, DsTheme.surface.b, 0.9)
        borderColor: DsTheme.stroke
        borderWidth: 1
    }

    // --- Header: title + tab pills + drag handle. -----------------------------------------------
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 28
        height: titleText.height + 14 + tabRow.height

        Text {
            id: titleText
            text: "Content Launcher"
            color: DsTheme.surfaceText
            font.family: "Roboto"
            font.pixelSize: 36
            font.weight: Font.Thin
        }

        Row {
            id: tabRow
            anchors.top: titleText.bottom
            anchors.topMargin: 14
            spacing: 14
            TabPill {
                label: "Content"
                iconName: (root.viewMode === "content") ? "content_selected" : "content"
                active: (root.viewMode === "content")
                onClicked: root.viewMode = "content"
            }
            TabPill {
                label: "Search"
                iconName: (root.viewMode === "search") ? "search_selected" : "search"
                active: (root.viewMode === "search")
                onClicked: root.viewMode = "search"
            }
        }

        // Drag-by-header — the rest of the panel (list, close button) is free to handle taps.
        DragHandler { target: root }
    }

    // --- Body: Loader between Content and Search panes. ----------------------------------------
    Item {
        id: bodyArea
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: closeBtn.top
        anchors.topMargin: 16
        anchors.bottomMargin: 12

        Loader {
            anchors.fill: parent
            sourceComponent: (root.viewMode === "content") ? contentPane : searchPane
        }
    }

    // Content pane: breadcrumb (drilled-in) OR CURRENT PLAYLIST + CONTENT LIBRARY (at root) + list.
    Component {
        id: contentPane
        ColumnLayout {
            spacing: 0

            // Breadcrumb (drilled-in only).
            Item {
                visible: root.model && !root.model.atRoot
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                RowLayout {
                    anchors.fill: parent
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 8
                        color: DsTheme.surfaceVariant
                        DsLauncherIcon {
                            anchors.centerIn: parent
                            sourceSize: 18
                            source: root._icon("back")
                            color: DsTheme.surfaceText
                        }
                        TapHandler { onTapped: if (root.model) root.model.back() }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (!root.model) return "";
                            return root.model.path.map(it => it.title).join("  ›  ");
                        }
                        color: DsTheme.surfaceText
                        font.family: "Roboto"
                        font.pixelSize: 14
                        elide: Text.ElideLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // CURRENT PLAYLIST header (root only, when there is a playlist).
            Text {
                visible: root.model && root.model.atRoot && root.model.currentPlaylist
                text: "CURRENT PLAYLIST"
                color: Qt.rgba(DsTheme.surfaceText.r, DsTheme.surfaceText.g, DsTheme.surfaceText.b, 0.55)
                font.family: "Roboto"
                font.pixelSize: 10
                font.letterSpacing: 0.5
                font.weight: Font.Light
                Layout.leftMargin: 24
                Layout.topMargin: 4
                Layout.bottomMargin: 6
            }

            // The current-playlist row (root only).
            LauncherRow {
                visible: root.model && root.model.atRoot && root.model.currentPlaylist
                Layout.fillWidth: true
                item: root.model ? root.model.currentPlaylist : null
                selected: false
                onActivated: if (root.model && root.model.currentPlaylist) root.model.enter(root.model.currentPlaylist)
            }

            // CONTENT LIBRARY header (root only).
            Text {
                visible: root.model && root.model.atRoot
                text: "CONTENT LIBRARY"
                color: Qt.rgba(DsTheme.surfaceText.r, DsTheme.surfaceText.g, DsTheme.surfaceText.b, 0.55)
                font.family: "Roboto"
                font.pixelSize: 10
                font.letterSpacing: 0.5
                font.weight: Font.Light
                Layout.leftMargin: 24
                Layout.topMargin: 14
                Layout.bottomMargin: 6
            }

            // The list.
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 0
                model: root.model ? root.model.displayedItems : []
                boundsBehavior: Flickable.StopAtBounds

                delegate: LauncherRow {
                    id: rowItem
                    required property var modelData
                    width: ListView.view ? ListView.view.width : 0
                    item: modelData
                    selected: false
                    onActivated: {
                        if (!root.model || !rowItem.modelData) return;
                        if (rowItem.modelData.hasChildren) root.model.enter(rowItem.modelData);
                        else root.openRequested(rowItem.modelData);
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    width: 4
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 4
                        radius: 2
                        color: DsTheme.track
                    }
                    background: null
                }
            }
        }
    }

    // Search pane — placeholder for 2c.
    Component {
        id: searchPane
        Item {
            Text {
                anchors.centerIn: parent
                text: "Search — coming in 2c"
                color: DsTheme.surfaceText
                font.family: "Roboto"
                font.pixelSize: 14
                opacity: 0.55
            }
        }
    }

    // --- Bottom-centre close button. -----------------------------------------------------------
    Rectangle {
        id: closeBtn
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        width: 50
        height: 50
        radius: 8
        color: DsTheme.surfaceVariant

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.25)
            shadowVerticalOffset: 4
            shadowBlur: 0.6
            shadowOpacity: 1.0
        }

        DsLauncherIcon {
            anchors.centerIn: parent
            sourceSize: 24
            source: root._icon("close")
            color: DsTheme.surfaceText
        }

        TapHandler { onTapped: root.shown = false }
    }
}
