import QtQuick
import Dsqt.Core
import Dsqt.Waffles



Item {
    id: wafflesRoot
    property Component launcher: Component { DsTestLauncher {} }
    property Component viewer: Component { TitledMediaViewer {} }
    property Component fullscreenController: Qt.createComponent("FullscreenController.qml")
    property Component presentationController: Qt.createComponent("PresentationController.qml")
    property bool menuShown: false

    // --- Glass / blurred-backdrop config (defaults from DsTheme tokens; per-viewer overridable) ---
    property bool  glassEnabled: true
    property color glassTint:        DsTheme.surface          // Tonal 0
    property real  glassTintOpacity: DsTheme.glassTintOpacity // 80%
    property real  glassBlur:        DsTheme.glassBlur
    property int   glassBlurMax:     DsTheme.glassBlurMax
    property real  glassRadius:      DsTheme.glassRadius
    property color glassBorderColor: DsTheme.stroke           // Tonal 10
    property real  glassBorderWidth: DsTheme.glassBorderWidth

    // Control palette derived from the theme roles, so it propagates to every viewer/control
    // (dark surfaces, white text/icons, Tonal-10 strokes). See Docs/articles/color_system.md.
    palette.window:          DsTheme.surface
    palette.base:            DsTheme.surface
    palette.button:          DsTheme.surface
    palette.windowText:      DsTheme.onSurface
    palette.text:            DsTheme.onSurface
    palette.buttonText:      DsTheme.onSurface
    palette.brightText:      DsTheme.onSurface
    palette.dark:            DsTheme.stroke
    palette.mid:             DsTheme.stroke
    palette.light:           DsTheme.stroke
    palette.highlight:       DsTheme.accent
    palette.highlightedText: DsTheme.onAccent

    // Content captured as the bottom of the glass stack (e.g. an animated background).
    property alias backgroundContent: backdrop.data
    // The backdrop leaf, exposed so control glass can sample it without recursing through slots.
    readonly property Item glassBackdrop: backdrop

    // Viewer size limits (from app_settings.toml [viewer]; per-viewer overridable on creation).
    DsSettingsProxy { id: appSettings; target: "app_settings" }
    property real viewerMinWidth:  appSettings.getFloat("viewer.minWidth", 200)
    property real viewerMaxWidth:  appSettings.getFloat("viewer.maxWidth", 1600)
    property real viewerMinHeight: appSettings.getFloat("viewer.minHeight", 150)
    property real viewerMaxHeight: appSettings.getFloat("viewer.maxHeight", 1200)

    // Live composite "slots" (stage-sized textures), one per viewer that has something above it.
    property var _glassSlots: []
    // Incrementing z handed to the most recently selected viewer so it sits in front.
    property int _topZ: 1
    // Currently selected viewer, to skip redundant reselects (avoids glass chain churn).
    property var _selectedViewer: null

    onGlassEnabledChanged: rebuildGlass()

    Component.onCompleted: {
        createMenu()
    }

    TapHandler {

    }

    // Holder for the cumulative composite chain. Each slot is a stage-sized texture of
    // everything beneath one viewer. It must actually be in the render tree for its nested live
    // ShaderEffectSources to keep updating, so it stays visible but sits at the very back, fully
    // covered by the opaque backdrop above it — the user never sees it directly.
    Item {
        id: glassSlotHolder
        anchors.fill: wafflesRoot
    }

    // C[0] of the glass chain: everything below the viewer layer (animated background + gallery).
    // Its opaque base also occludes glassSlotHolder beneath it.
    Item {
        id: backdrop
        anchors.fill: wafflesRoot
    }

    Item {
        id: topLayer
        anchors.fill: wafflesRoot
        // Keep viewers/launcher above any consumer content placed in the stage.
        z: 1
        onChildrenChanged: wafflesRoot.rebuildGlass()
    }

    // One slot = the backdrop plus every viewer below its owner, composited FLAT: each source is
    // a leaf item (the backdrop, or a viewer's glass-free captureItem), never another slot. With
    // no slot-to-slot nesting the chain has no depth, so the front (deepest) viewer can't blank
    // out and there's no propagation lag. Each captured viewer is positioned at its stage rect
    // (tracks dragging) using its captureItem's childrenRect to include overflowing control sets.
    Component {
        id: glassSlotComponent
        Item {
            id: slot
            property var lowerViewers: []
            anchors.fill: parent
            ShaderEffectSource {
                anchors.fill: parent
                sourceItem: backdrop
                live: true
                hideSource: false
            }
            Repeater {
                model: slot.lowerViewers
                ShaderEffectSource {
                    required property var modelData
                    readonly property Item cap: modelData ? modelData.captureItem : null
                    x: cap ? modelData.x + cap.childrenRect.x : 0
                    y: cap ? modelData.y + cap.childrenRect.y : 0
                    width: cap ? cap.childrenRect.width : 0
                    height: cap ? cap.childrenRect.height : 0
                    sourceItem: cap
                    sourceRect: cap ? Qt.rect(cap.childrenRect.x, cap.childrenRect.y,
                                              cap.childrenRect.width, cap.childrenRect.height)
                                    : Qt.rect(0, 0, 0, 0)
                    live: true
                    hideSource: false
                }
            }
        }
    }

    //functions && _privates
    QtObject {
        id: _private
        property var launcher: null
    }

    function createMenu() {
        if (launcher.status == Component.Ready)
            completeMenu()
        else
            launcher.statusChanged.connect(completeMenu)
    }

    function completeMenu() {
        if(launcher.status == Component.Ready) {
            _private.launcher = launcher.createObject(topLayer,{"opacity":1, "stage":wafflesRoot});
            if(_private.launcher == null)
            {
                console.log("Error creating menu");
            }
        } else if (launcher.status === Component.Error) {
            console.log("Error loading component:", launcher.errorString());
        }
    }

    function createViewer(viewerProps: var) {
        if (viewer.status == Component.Ready)
            completeViewer(viewerProps)
        else
            viewer.statusChanged.connect(()=>{completeViewer(viewerProps);})
    }

    function completeViewer(viewerProps: var) {
        if(viewer.status == Component.Ready) {
            let viewerInstance = viewer.createObject(topLayer,viewerProps);
            if(viewerInstance == null)
            {
                console.log("Error creating viewer");
            } else {
                selectViewer(viewerInstance);
            }
        } else if (viewer.status === Component.Error) {
            console.log("Error loading component:", viewer.errorString());
        }
    }

    function openViewer(viewerProps: var){
        createViewer(viewerProps);
    }

    // Brings a viewer to the front and marks it selected (deselecting the rest). Selected
    // viewers show their controls/title; the z bump reorders the glass chain so the front
    // viewer samples everything beneath it. Pass null to deselect all (e.g. tap on empty space).
    function selectViewer(v) {
        if (v === _selectedViewer)
            return;
        _selectedViewer = v;
        let kids = topLayer.children;
        for (let i = 0; i < kids.length; i++) {
            let p = kids[i];
            if (p && ("selected" in p))
                p.selected = (p === v);
        }
        if (v)
            v.z = ++_topZ;
        rebuildGlass();
    }

    // Rebuilds the cumulative glass chain. Each topLayer child is assigned a backdropSource
    // (a slot holding everything painted below it); the next link composites that slot with
    // the child itself. Children paint in array order, which is our z-order. Movement and
    // animated content update for free via the live ShaderEffectSources; only add/remove/
    // reorder needs a rebuild.
    function rebuildGlass() {
        for (let i = 0; i < _glassSlots.length; i++) {
            if (_glassSlots[i])
                _glassSlots[i].destroy();
        }
        _glassSlots = [];

        // Participants in paint order: sort topLayer children by z, then document order.
        let kids = [];
        for (let k = 0; k < topLayer.children.length; k++)
            kids.push({ item: topLayer.children[k], idx: k });
        kids.sort((a, b) => {
            let d = (a.item.z || 0) - (b.item.z || 0);
            return d !== 0 ? d : (a.idx - b.idx);
        });
        let parts = kids.map(k => k.item);

        // Glass disabled: drop every viewer's source so the live chain stops entirely.
        if (!glassEnabled) {
            for (let j = 0; j < parts.length; j++) {
                let q = parts[j];
                if (q && ("backdropSource" in q))
                    q.backdropSource = null;
            }
            return;
        }

        for (let i = 0; i < parts.length; i++) {
            let p = parts[i];
            if (!(p && ("backdropSource" in p)))
                continue;
            if (i === 0) {
                // Bottom-most viewer samples the backdrop leaf directly.
                p.backdropSource = backdrop;
            } else {
                let slot = glassSlotComponent.createObject(glassSlotHolder, {
                    "lowerViewers": parts.slice(0, i)
                });
                _glassSlots.push(slot);
                p.backdropSource = slot;
            }
        }
    }

    TapHandler {
        target: null
        onTapped: (point,button)=>{
            if(tapCount == 2){
                _private.launcher.x = point.position.x
                _private.launcher.y = point.position.y
            }
            let inView = null;
            for(let i=0;i<topLayer.children.length;i++){
              let child = topLayer.children[i];
              if(child.contains(child.mapFromGlobal(point.globalPosition))){
                  inView = child;
              }

            }
            wafflesRoot.selectViewer(inView);

        }
    }
}
