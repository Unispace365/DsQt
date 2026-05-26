import QtQuick
import QtQuick.Effects
import Dsqt.Core
import Dsqt.Waffles



Item {
    id: wafflesRoot
    property Component launcher: Component { DsTestLauncher {} }
    // Data adapter for the launcher (e.g. ContentLauncherModel). Passed through to the launcher
    // instance as `model` when the stage creates it. Bridge-agnostic — apps wire their own
    // adapter; the launcher just consumes the shape it defines.
    property var launcherModel: null
    property Component viewer: Component { TitledMediaViewer {} }
    // Per-type fullscreen controllers (shown while a viewer of that type is fullscreen).
    property Component fullscreenController: Component { FullscreenController {} }
    property Component presentationController: Qt.createComponent("PresentationController.qml")
    // The live controller instance (created lazily, reused across fullscreen sessions).
    property var _fsController: null
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
    palette.windowText:      DsTheme.surfaceText
    palette.text:            DsTheme.surfaceText
    palette.buttonText:      DsTheme.surfaceText
    palette.brightText:      DsTheme.surfaceText
    palette.dark:            DsTheme.stroke
    palette.mid:             DsTheme.stroke
    palette.light:           DsTheme.stroke
    palette.highlight:       DsTheme.accent
    palette.highlightedText: DsTheme.accentText

    // Content captured as the bottom of the glass stack (e.g. an animated background).
    property alias backgroundContent: backdrop.data
    // The backdrop leaf, exposed so control glass can sample it without recursing through slots.
    readonly property Item glassBackdrop: backdrop

    // Viewer size limits (from [waffles.viewer] in waffles_settings.toml, merged into the
    // app_settings collection; per-viewer overridable on creation).
    DsSettingsProxy { id: appSettings; target: "app_settings"; prefix: "waffles" }
    property real viewerMinWidth:  appSettings.getFloat("viewer.minWidth", 200)
    property real viewerMaxWidth:  appSettings.getFloat("viewer.maxWidth", 1600)
    property real viewerMinHeight: appSettings.getFloat("viewer.minHeight", 150)
    property real viewerMaxHeight: appSettings.getFloat("viewer.maxHeight", 1200)
    // Default width an image viewer uses when it fits itself to the media aspect ratio.
    property real viewerImageWidth: appSettings.getFloat("viewer.imageWidth", 800)
    // Controls auto-hide after this idle time (ms). 0 disables auto-hide. Per-viewer overridable.
    property int controlsIdleMs: appSettings.getInt("controlsIdleSeconds", 4) * 1000
    // Enter/exit animation defaults (per-viewer overridable on creation). Stored as strings in
    // settings (fade | grow | growBounce | fadeRise | none); mapped to the DsViewer.Anim enum.
    property int viewerEnterAnimation:    _animFromString(appSettings.getString("viewer.enterAnimation", "growBounce"))
    property int viewerExitAnimation:     _animFromString(appSettings.getString("viewer.exitAnimation", "grow"))
    property int viewerAnimationDuration: appSettings.getInt("viewer.animationDuration", 300)

    // --- Fullscreen config. A fullscreen viewer fits the stage (minus margin), over a scrim that
    //     blocks the rest of the UI. The scrim style follows the viewer's resolved glass state
    //     (blur on -> blurred scrim; off -> tint+alpha), or the viewer's fullscreenScrim override. ---
    property real  fullscreenMargin:              appSettings.getFloat("fullscreen.margin", 80)
    property color fullscreenScrimColor:          DsTheme.surface
    property real  fullscreenScrimOpacity:        appSettings.getFloat("fullscreen.scrimOpacity", 0.6)
    property real  fullscreenScrimBlurTintOpacity: appSettings.getFloat("fullscreen.scrimBlurTintOpacity", 0.3)
    // Blur for the blur-mode scrim. Defaults to the glass tokens so the scrim matches the viewer
    // glass; set fullscreen.scrimBlur / scrimBlurMax in settings to give the scrim its own blur.
    property real  fullscreenScrimBlur:           appSettings.getFloat("fullscreen.scrimBlur", DsTheme.glassBlur)
    property int   fullscreenScrimBlurMax:        appSettings.getInt("fullscreen.scrimBlurMax", DsTheme.glassBlurMax)
    property int   fullscreenDuration:            viewerAnimationDuration

    // The single fullscreen viewer (only one at a time), the resolved scrim mode, whether the
    // scrim is shown, and the geometry to restore on exit. Scrim sits at _scrimZ; the fullscreen
    // viewer one above it.
    property var    _fullscreenViewer: null
    property string _scrimMode: "tint"     // blur | tint | none
    property bool   _scrimShown: false
    property rect   _fsRestore: Qt.rect(0, 0, 0, 0)
    readonly property int _scrimZ: 9000

    // Live composite "slots" (stage-sized textures), one per viewer that has something above it.
    property var _glassSlots: []
    // Incrementing z handed to the most recently selected viewer so it sits in front.
    property int _topZ: 1
    // Currently selected viewer, to skip redundant reselects (avoids glass chain churn).
    property var _selectedViewer: null
    // Counter used to cascade successive centred opens so they don't perfectly overlap.
    property int _openCount: 0

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

        // Fullscreen scrim: blocks the rest of the UI behind the fullscreen viewer. Sits just
        // below it (z 9000 < 9001). Blur mode samples the fullscreen viewer's backdrop slot
        // (everything behind it) and blurs it; tint mode is a flat colour+alpha. Always input-
        // blocking while shown; tapping a margin exits fullscreen.
        Item {
            id: scrimLayer
            anchors.fill: parent
            z: wafflesRoot._scrimZ
            visible: opacity > 0
            opacity: wafflesRoot._scrimShown ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: wafflesRoot.fullscreenDuration; easing.type: Easing.OutCubic } }

            ShaderEffectSource {
                id: scrimGrab
                anchors.fill: parent
                visible: false
                live: wafflesRoot._scrimMode === "blur" && wafflesRoot._fullscreenViewer !== null
                hideSource: false
                sourceItem: wafflesRoot._fullscreenViewer ? wafflesRoot._fullscreenViewer.backdropSource : null
            }
            MultiEffect {
                anchors.fill: parent
                visible: wafflesRoot._scrimMode === "blur"
                source: scrimGrab
                autoPaddingEnabled: false
                blurEnabled: true
                blur: wafflesRoot.fullscreenScrimBlur
                blurMax: wafflesRoot.fullscreenScrimBlurMax
            }
            Rectangle {
                anchors.fill: parent
                color: wafflesRoot.fullscreenScrimColor
                opacity: wafflesRoot._scrimMode === "tint" ? wafflesRoot.fullscreenScrimOpacity
                       : wafflesRoot._scrimMode === "blur" ? wafflesRoot.fullscreenScrimBlurTintOpacity
                       : 0
            }
            MouseArea {
                anchors.fill: parent
                enabled: wafflesRoot._scrimShown
                acceptedButtons: Qt.AllButtons
                onClicked: if (wafflesRoot._fullscreenViewer) wafflesRoot.setFullscreen(wafflesRoot._fullscreenViewer, false)
            }
        }
    }

    // Drives the fullscreen enter/exit geometry transition for whichever viewer is targeted.
    ParallelAnimation {
        id: _fsAnim
        property var _done: null
        NumberAnimation { id: _fsX; property: "x";            duration: wafflesRoot.fullscreenDuration; easing.type: Easing.OutCubic }
        NumberAnimation { id: _fsY; property: "y";            duration: wafflesRoot.fullscreenDuration; easing.type: Easing.OutCubic }
        NumberAnimation { id: _fsW; property: "viewerWidth";  duration: wafflesRoot.fullscreenDuration; easing.type: Easing.OutCubic }
        NumberAnimation { id: _fsH; property: "viewerHeight"; duration: wafflesRoot.fullscreenDuration; easing.type: Easing.OutCubic }
        onFinished: { let cb = _fsAnim._done; _fsAnim._done = null; if (cb) cb(); }
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
            _private.launcher = launcher.createObject(topLayer, {
                "opacity": 1,
                "stage": wafflesRoot,
                "model": wafflesRoot.launcherModel
            });
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
                // Open centred in the stage unless the caller positioned the viewer itself.
                if (!("x" in viewerProps) && !("y" in viewerProps))
                    wafflesRoot.centerViewer(viewerInstance);
                if (viewerInstance.closeRequested)
                    viewerInstance.closeRequested.connect(()=>{ wafflesRoot.closeViewer(viewerInstance); });
                if (viewerInstance.fullscreenRequested)
                    viewerInstance.fullscreenRequested.connect((want)=>{ wafflesRoot.setFullscreen(viewerInstance, want); });
                selectViewer(viewerInstance);
                if (viewerInstance.playEnter)
                    viewerInstance.playEnter();
            }
        } else if (viewer.status === Component.Error) {
            console.log("Error loading component:", viewer.errorString());
        }
    }

    // Closes a viewer: clears selection if it was selected, plays its exit animation, then
    // destroys it. Destruction triggers topLayer.onChildrenChanged → rebuildGlass.
    function closeViewer(v) {
        if (!v) return;
        // If the viewer being closed is fullscreen, tear down the scrim + controller first.
        if (v === _fullscreenViewer) {
            _hideController();
            _scrimShown = false;
            _fullscreenViewer = null;
        }
        if (_selectedViewer === v) _selectedViewer = null;
        if (v.playExit)
            v.playExit(()=>{ v.destroy(); });
        else
            v.destroy();
    }

    // Places a freshly created viewer centred in the stage, with a gentle cascade so successive
    // opens don't perfectly overlap. The viewer is already sized (applySizing ran on completion).
    function centerViewer(v) {
        let off = ((wafflesRoot._openCount % 5) - 2) * 60;
        wafflesRoot._openCount++;
        v.x = Math.round((wafflesRoot.width  - v.width)  / 2 + off);
        v.y = Math.round((wafflesRoot.height - v.height) / 2 + off);
    }

    // Enters/leaves fullscreen for v. Only one viewer is fullscreen at a time; entering while
    // another is fullscreen snaps the previous one back first. Entering animates v to a margin-fit
    // of the stage over the scrim; leaving animates it back to its pre-fullscreen geometry.
    function setFullscreen(v, on) {
        if (!v) return;
        if (on) {
            if (wafflesRoot._fullscreenViewer === v) return;
            if (wafflesRoot._fullscreenViewer) {
                let prev = wafflesRoot._fullscreenViewer;
                let pr = wafflesRoot._fsRestore;
                prev.fullscreen = false;
                prev.x = pr.x; prev.y = pr.y; prev.viewerWidth = pr.width; prev.viewerHeight = pr.height;
                prev.z = ++wafflesRoot._topZ;
                wafflesRoot._fullscreenViewer = null;
            }
            wafflesRoot._fsRestore = Qt.rect(v.x, v.y, v.viewerWidth, v.viewerHeight);
            wafflesRoot._scrimMode = wafflesRoot._scrimModeFor(v);
            v.fullscreen = true;
            v.z = wafflesRoot._scrimZ + 1;
            wafflesRoot._fullscreenViewer = v;
            wafflesRoot._scrimShown = true;
            rebuildGlass();
            let r = wafflesRoot._fullscreenFit(v);
            wafflesRoot._animateViewer(v, r.x, r.y, r.width, r.height, null);
            wafflesRoot._showController(v);
        } else {
            if (wafflesRoot._fullscreenViewer !== v) return;
            let rr = wafflesRoot._fsRestore;
            v.fullscreen = false;
            wafflesRoot._scrimShown = false;
            wafflesRoot._hideController();
            wafflesRoot._animateViewer(v, rr.x, rr.y, rr.width, rr.height, function() {
                v.z = ++wafflesRoot._topZ;
                if (wafflesRoot._fullscreenViewer === v) wafflesRoot._fullscreenViewer = null;
                wafflesRoot.rebuildGlass();
            });
        }
    }

    // Resolves the scrim style for a viewer: its fullscreenScrim override, or "auto" -> blur when
    // both the viewer and the stage have glass on, else tint.
    function _scrimModeFor(v) {
        let m = (v && ("fullscreenScrim" in v)) ? v.fullscreenScrim : "auto";
        if (m === "auto")
            m = (v && ("glassEnabled" in v) && v.glassEnabled && wafflesRoot.glassEnabled) ? "blur" : "tint";
        return m;
    }

    // Largest rect inside the stage (minus margin) that preserves the viewer's current aspect.
    function _fullscreenFit(v) {
        let availW = wafflesRoot.width  - 2 * wafflesRoot.fullscreenMargin;
        let availH = wafflesRoot.height - 2 * wafflesRoot.fullscreenMargin;
        let aspect = (v.viewerHeight > 0) ? (v.viewerWidth / v.viewerHeight) : (availW / availH);
        let w = availW, h = availW / aspect;
        if (h > availH) { h = availH; w = h * aspect; }
        return Qt.rect(Math.round((wafflesRoot.width - w) / 2), Math.round((wafflesRoot.height - h) / 2),
                       Math.round(w), Math.round(h));
    }

    function _animateViewer(v, x, y, w, h, done) {
        _fsAnim.stop();
        _fsX.target = v; _fsX.to = x;
        _fsY.target = v; _fsY.to = y;
        _fsW.target = v; _fsW.to = w;
        _fsH.target = v; _fsH.to = h;
        _fsAnim._done = done || null;
        _fsAnim.start();
    }

    // Shows the per-type fullscreen controller, bound to v, above the fullscreen viewer. The
    // viewer hides its own edge controls while fullscreen, so the controller carries them.
    function _showController(v) {
        let comp = wafflesRoot.fullscreenController;   // (could vary by v.viewerType later)
        if (!comp || comp.status !== Component.Ready) return;
        if (!wafflesRoot._fsController) {
            wafflesRoot._fsController = comp.createObject(topLayer, { "z": wafflesRoot._scrimZ + 2 });
        }
        if (wafflesRoot._fsController) {
            wafflesRoot._fsController.viewer = v;
            wafflesRoot._fsController.collapsed = false;
            wafflesRoot._fsController.shown = true;
        }
    }

    function _hideController() {
        if (wafflesRoot._fsController) {
            wafflesRoot._fsController.shown = false;
            wafflesRoot._fsController.viewer = null;
        }
    }

    // Maps a settings string to a DsViewer.Anim enum value.
    function _animFromString(s) {
        switch (String(s).toLowerCase()) {
        case "none":       return DsViewer.Anim.None;
        case "fade":       return DsViewer.Anim.Fade;
        case "grow":       return DsViewer.Anim.Grow;
        case "growbounce": return DsViewer.Anim.GrowBounce;
        case "faderise":   return DsViewer.Anim.FadeRise;
        default:           return DsViewer.Anim.Fade;
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
            // While fullscreen the scrim handles taps (margin tap exits); ignore stage taps.
            if (wafflesRoot._fullscreenViewer) return;
            if(tapCount == 2 && _private.launcher){
                // Reposition centred on the tap point, and re-show if the launcher was hidden
                // via its close button. The `shown` property is optional on the launcher type;
                // launchers that don't expose it (e.g. DsTestLauncher) simply ignore the bump.
                _private.launcher.x = Math.round(point.position.x - _private.launcher.width / 2)
                _private.launcher.y = Math.round(point.position.y)
                if ("shown" in _private.launcher) _private.launcher.shown = true
            }
            let inView = null;
            for(let i=0;i<topLayer.children.length;i++){
              let child = topLayer.children[i];
              if (child === scrimLayer) continue;
              if(child.contains(child.mapFromGlobal(point.globalPosition))){
                  inView = child;
              }

            }
            wafflesRoot.selectViewer(inView);

        }
    }
}
