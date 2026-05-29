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
    // Also re-establish the opacity binding: the stage's createObject(...) passes
    // {"opacity": 1, ...} when instantiating the launcher, which replaces the declarative
    // `opacity: shown ? 1 : 0` binding above with a constant. Without this re-bind the close
    // button's `shown = false` does nothing visible.
    Component.onCompleted: {
        if (parent) {
            x = Math.round(parent.width / 2 - width / 2);
            y = 100;
        }
        opacity = Qt.binding(() => root.shown ? 1 : 0);
    }

    // Glass config — defaults from the stage (which falls back to DsTheme). The launcher panel
    // uses a stronger tint than the media chrome (0.9 vs 0.8 per Figma).
    property bool  glassEnabled:     stage ? stage.glassEnabled : true
    property color glassTint:        stage ? stage.glassTint : DsTheme.surface
    property real  glassTintOpacity: 0.9
    property real  glassBlur:        stage ? stage.glassBlur : DsTheme.glassBlur
    property int   glassBlurMax:     stage ? stage.glassBlurMax : DsTheme.glassBlurMax
    property real  glassBlurMultiplier: stage ? stage.glassBlurMultiplier : DsTheme.glassBlurMultiplier
    property real  glassRadius:      20
    property color glassBorderColor: stage ? stage.glassBorderColor : DsTheme.stroke
    property real  glassBorderWidth: stage ? stage.glassBorderWidth : DsTheme.glassBorderWidth

    // Glass context — bundles `source: backdropSource` (the stage's slot, assigned by
    // DsWaffleStage.rebuildGlass) + the viewer's own coords so DsGlassBackground can self-sample
    // its slice via mapToItem. `refresh` re-triggers the slice computation when the launcher is
    // dragged.
    QtObject {
        id: glassContext
        property Item  source: root.backdropSource
        property Item  viewerItem: root
        property real  viewerX: root.x
        property real  viewerY: root.y
        property bool  enabled: root.glassEnabled
        property color tint: root.glassTint
        property real  tintOpacity: root.glassTintOpacity
        property real  blur: root.glassBlur
        property int   blurMax: root.glassBlurMax
        property real  blurMultiplier: root.glassBlurMultiplier
        property color borderColor: root.glassBorderColor
        property real  borderWidth: root.glassBorderWidth
        property real  radius: root.glassRadius
        property real  refresh: root.x + root.y + root.width + root.height
    }

    // Resolve an icon URL by short name (no extension).
    function _icon(name) {
        return Ds.env.expandUrl("%APP%/data/images/waffles/content_launcher/" + name + ".svg");
    }

    // Map an item's kind to the leading icon name. Used when a drillable item has no thumbnail.
    // Falls back to "folder" so unmapped drillable kinds still render a sensible icon.
    function _leadingIconFor(kind) {
        if (kind === "folder")   return "folder";
        if (kind === "playlist") return "playlist";
        return "folder";
    }

    // Map an item's kind to the trailing type icon (for media leaves).
    function _typeIconFor(kind) {
        if (kind === "image")   return "image";
        if (kind === "video")   return "video";
        if (kind === "pdf")     return "pdf";
        if (kind === "weblink") return "weblink";
        return "";
    }

    // Order-insensitive equality for the search-filter kind arrays (drives chip active-state).
    function _kindsEqual(a, b) {
        const aa = (a || []).slice().sort();
        const bb = (b || []).slice().sort();
        if (aa.length !== bb.length) return false;
        for (let i = 0; i < aa.length; ++i) if (aa[i] !== bb[i]) return false;
        return true;
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

    // A search type-filter chip. Single-select: tapping sets the model's searchKinds to this
    // chip's `kinds` ([] for "All"); active when the model's current filter matches.
    component FilterChip: Rectangle {
        id: chip
        property string label
        property var kinds: []
        property var targetModel: null
        readonly property bool active: chip.targetModel ? root._kindsEqual(chip.targetModel.searchKinds, chip.kinds) : false
        implicitHeight: 32
        implicitWidth: chipText.implicitWidth + 28
        radius: 16
        color: chip.active ? DsTheme.selectedSurface : DsTheme.surfaceVariant
        border.color: DsTheme.stroke
        border.width: chip.active ? 0 : 1
        Text {
            id: chipText
            anchors.centerIn: parent
            text: chip.label
            color: chip.active ? DsTheme.selectedSurfaceText : DsTheme.surfaceText
            font.family: "Roboto"
            font.pixelSize: 13
        }
        TapHandler { onTapped: if (chip.targetModel) chip.targetModel.searchKinds = chip.kinds }
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

            // Leading 34x34. Three states:
            //   - thumbnail present   → rounded thumbnail
            //   - drillable, no thumb → Tonal-10 rounded square + kind icon (folder / playlist)
            //   - leaf, no thumb      → empty (rare; could add per-kind leaf icons later)
            Item {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34

                // Kind-icon container for drillable items without a thumbnail.
                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: DsTheme.surfaceVariant
                    visible: lrow.item && lrow.item.hasChildren && !lrow.item.thumbnail
                }
                DsLauncherIcon {
                    anchors.centerIn: parent
                    visible: lrow.item && lrow.item.hasChildren && !lrow.item.thumbnail
                    sourceSize: 20
                    source: lrow.item ? root._icon(root._leadingIconFor(lrow.item.kind)) : ""
                    color: lrow.selected ? DsTheme.selectedSurfaceText : DsTheme.surfaceText
                }

                // Thumbnail — rounded via MultiEffect mask. Always shown when present, even on
                // drillable items (a folder/Asset/playlist with a cover image looks nicer than
                // the icon fallback). The trailing chevron still indicates drillability.
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
        context: glassContext
        topLeftRadius: 20
        topRightRadius: 20
        bottomLeftRadius: 20
        bottomRightRadius: 20
        fallbackColor: Qt.rgba(DsTheme.surface.r, DsTheme.surface.g, DsTheme.surface.b, 0.9)
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
            font.pixelSize: 32
            font.weight: 100   // Roboto Variable's lightest weight (= Font.Thin)
            font.letterSpacing: 0.5
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
    }

    // Drag from anywhere on the panel — PointerHandlers cooperate with the more-specific
    // TapHandlers (rows, tabs, close button) and the ListView's internal Flickable, so:
    //   - tap on a row / tab / close → that handler activates;
    //   - touch-drag inside the list → list scrolls;
    //   - touch-drag on any other empty space (header, section labels, breadcrumb, padding,
    //     gutters, around the close button) → the launcher itself drags.
    // Disabled while the docked keyboard is up: this root handler would otherwise steal the
    // press-hold-drag the virtual keyboard uses to pick alternate (long-press) keys, moving the
    // launcher instead of selecting the character.
    DragHandler {
        target: root
        enabled: !(keyboardLoader.item && keyboardLoader.item.panelActive)
    }

    // --- Body: Loader between Content and Search panes. ----------------------------------------
    Item {
        id: bodyArea
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: keyboardArea.top
        anchors.topMargin: 16
        anchors.bottomMargin: 12

        Loader {
            anchors.fill: parent
            sourceComponent: (root.viewMode === "content") ? contentPane : searchPane
        }
    }

    // --- Docked virtual keyboard (search mode). -------------------------------------------------
    // Sits just above the close button; bodyArea's bottom anchors to its top so the results list
    // gives up space while the keyboard is up. Only loaded in search mode — all InputPanels in a
    // window share one InputContext, so we keep at most one active (see DsVirtualKeyboard note).
    Item {
        id: keyboardArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: closeBtn.top
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 6
        clip: true
        height: (root.viewMode === "search" && keyboardLoader.item && keyboardLoader.item.panelActive)
                ? keyboardLoader.item.panelHeight : 0
        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        Loader {
            id: keyboardLoader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            // Gated off while the stage's floating keyboard is shown — one InputPanel at a time.
            // Also gated on the launcher being on-screen, so closing the launcher while search is
            // open tears the InputPanel down (the keyboard capture ends with it).
            active: root.visible && root.viewMode === "search"
                    && !(root.stage && root.stage.floatingKeyboardShown)
            sourceComponent: keyboardComponent
        }
    }

    Component {
        id: keyboardComponent
        DsVirtualKeyboard {}
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

    // Search pane (Phase 2c): search field + type filter chips + results list. The field grabs
    // focus on open so the docked virtual keyboard (see keyboardLoader) shows automatically.
    Component {
        id: searchPane
        ColumnLayout {
            spacing: 0

            // Type filter chips (single-select; "All" clears the filter).
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 12
                spacing: 8

                FilterChip { label: "All";     kinds: [];                     targetModel: root.model }
                FilterChip { label: "Images";  kinds: ["image"];              targetModel: root.model }
                FilterChip { label: "Video";   kinds: ["video"];              targetModel: root.model }
                FilterChip { label: "PDF";     kinds: ["pdf"];                targetModel: root.model }
                FilterChip { label: "Web";     kinds: ["weblink"];            targetModel: root.model }
                FilterChip { label: "Folders"; kinds: ["folder", "playlist"]; targetModel: root.model }
            }

            // Results, with prompt / no-results empty states.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 48
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: resultsList.count === 0
                    text: {
                        if (!root.model) return "";
                        const hasQuery  = ("" + root.model.searchQuery).trim().length > 0;
                        const hasFilter = (root.model.searchKinds || []).length > 0;
                        return (!hasQuery && !hasFilter) ? "Type to search the content library"
                                                         : "No matching content";
                    }
                    color: Qt.rgba(DsTheme.surfaceText.r, DsTheme.surfaceText.g, DsTheme.surfaceText.b, 0.55)
                    font.family: "Roboto"
                    font.pixelSize: 14
                }

                ListView {
                    id: resultsList
                    anchors.fill: parent
                    clip: true
                    spacing: 0
                    model: root.model ? root.model.searchResults : []
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: LauncherRow {
                        id: resRow
                        required property var modelData
                        width: resultsList.width
                        item: modelData
                        selected: false
                        onActivated: {
                            if (!root.model || !resRow.modelData) return;
                            if (resRow.modelData.hasChildren) {
                                root.viewMode = "content";
                                root.model.enter(resRow.modelData);
                            } else {
                                root.openRequested(resRow.modelData);
                            }
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

            // Search field — docked at the bottom, just above the keyboard (per Figma). Grabs
            // focus on open so the docked virtual keyboard shows automatically.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 12
                Layout.bottomMargin: 12

                Rectangle {
                    anchors.fill: parent
                    radius: 23
                    color: DsTheme.surfaceVariant
                    border.color: DsTheme.stroke
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 10

                        DsLauncherIcon {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            sourceSize: 18
                            source: root._icon("search")
                            color: Qt.rgba(DsTheme.surfaceText.r, DsTheme.surfaceText.g, DsTheme.surfaceText.b, 0.6)
                        }

                        TextField {
                            id: searchField
                            objectName: "launcherSearchField"
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            placeholderText: "Search content"
                            color: DsTheme.surfaceText
                            placeholderTextColor: Qt.rgba(DsTheme.surfaceText.r, DsTheme.surfaceText.g, DsTheme.surfaceText.b, 0.5)
                            font.family: "Roboto"
                            font.pixelSize: 15
                            background: null
                            leftPadding: 0
                            rightPadding: 0
                            verticalAlignment: TextInput.AlignVCenter
                            inputMethodHints: Qt.ImhNoPredictiveText
                            text: root.model ? root.model.searchQuery : ""
                            onTextEdited: if (root.model) root.model.searchQuery = text
                            Component.onCompleted: if (root.visible) searchField.forceActiveFocus()
                            // Tie input focus to the launcher being on-screen: when the launcher
                            // hides (e.g. closed while search is open) drop focus + dismiss the IM
                            // so the keyboard/capture ends; regrab focus when it's shown again.
                            Connections {
                                target: root
                                function onVisibleChanged() {
                                    if (root.visible) {
                                        searchField.forceActiveFocus();
                                    } else {
                                        searchField.focus = false;
                                        Qt.inputMethod.hide();
                                    }
                                }
                            }
                        }

                        DsLauncherIcon {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            sourceSize: 16
                            visible: searchField.text.length > 0
                            source: root._icon("close")
                            color: DsTheme.surfaceText
                            // Reset the model too: searchField.clear() is a programmatic change so
                            // it doesn't fire onTextEdited, which is what syncs model.searchQuery.
                            TapHandler {
                                onTapped: {
                                    if (root.model) root.model.searchQuery = "";
                                    searchField.clear();
                                    searchField.forceActiveFocus();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // --- Close button. -------------------------------------------------------------------------
    // Straddles the launcher's bottom border: its centre sits on the bottom edge (half inside, half
    // out), splitting the border the way the fullscreen controller's collapse tab straddles the bar.
    Rectangle {
        id: closeBtn
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.bottom
        width: 44
        height: 44
        radius: 8
        color: DsTheme.surfaceVariant
        border.color: DsTheme.stroke
        border.width: 1

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
