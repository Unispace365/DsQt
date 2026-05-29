import QtQuick
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt.Core
import Dsqt.Waffles



Item {
    id: wafflesRoot
    property Component launcher: Component { DsTestLauncher {} }
    // Data adapter for the launcher (e.g. ContentLauncherModel). Passed through to the launcher
    // instance as `model` when the stage creates it. Bridge-agnostic — apps wire their own
    // adapter; the launcher just consumes the shape it defines.
    property var launcherModel: null
    // Named layer the launcher is parented into. Apps can set this to "modal3" to keep the
    // launcher above the fullscreen scrim, or to a custom layer added via addLayer() (deferred).
    property string launcherLayer: "modal1"
    property Component viewer: Component { TitledMediaViewer {} }
    // Per-type fullscreen controllers (shown while a viewer of that type is fullscreen).
    property Component fullscreenController: Component { FullscreenController {} }
    property Component presentationController: Qt.createComponent("PresentationController.qml")
    // The live controller instance (created lazily, reused across fullscreen sessions).
    property var _fsController: null
    property bool menuShown: false
    // Show a small floating button on the stage to summon the launcher when it has been
    // hidden via its close button. Apps that don't want it can set this to false.
    property bool launcherButtonVisible: true

    // Floating virtual keyboard (for text entry over non-launcher viewers, e.g. web). Toggled by
    // the web controls' keyboard button (DsMediaControls). Hosted on modal3 so it floats above
    // everything; the launcher gates its own docked keyboard off while this is shown so only one
    // InputPanel is active at a time.
    property bool floatingKeyboardShown: false

    // --- Glass / blurred-backdrop config (defaults from DsTheme tokens; per-viewer overridable) ---
    property bool  glassEnabled: true
    property color glassTint:        DsTheme.surface          // Tonal 0
    property real  glassTintOpacity: DsTheme.glassTintOpacity // 80%
    property real  glassBlur:        DsTheme.glassBlur
    property int   glassBlurMax:     DsTheme.glassBlurMax
    property real  glassBlurMultiplier: DsTheme.glassBlurMultiplier
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
    property alias backgroundContent: background.data
    // The backdrop leaf, exposed so control glass can sample it without recursing through slots.
    // (Name retained for backwards compatibility; now points at the renamed `background` layer.)
    readonly property Item glassBackdrop: background

    // Viewer size limits (from [waffles.viewer] in waffles_settings.toml, merged into the
    // app_settings collection; per-viewer overridable on creation).
    DsSettingsProxy { id: appSettings; target: "app_settings"; prefix: "waffles" }
    property real viewerMinWidth:  appSettings.getFloat("viewer.minWidth", 200)
    property real viewerMaxWidth:  appSettings.getFloat("viewer.maxWidth", 1600)
    property real viewerMinHeight: appSettings.getFloat("viewer.minHeight", 150)
    property real viewerMaxHeight: appSettings.getFloat("viewer.maxHeight", 1200)
    // Default width an image viewer uses when it fits itself to the media aspect ratio.
    property real viewerImageWidth: appSettings.getFloat("viewer.imageWidth", 800)
    // Default size a web viewer opens at — web pages have no natural aspect to fit to, so they
    // open at this fixed size (clamped to the min/max bounds) unless the caller overrides sizing.
    property real viewerWebWidth:  appSettings.getFloat("viewer.webWidth", 1280)
    property real viewerWebHeight: appSettings.getFloat("viewer.webHeight", 800)
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
    // scrim is shown, and the geometry to restore on exit. Scrim sits at _scrimZ inside modal2;
    // the fullscreen viewer one above it (reparented into modal2 while fullscreen).
    property var    _fullscreenViewer: null
    property string _scrimMode: "tint"     // blur | tint | none
    property bool   _scrimShown: false
    property rect   _fsRestore: Qt.rect(0, 0, 0, 0)
    // Original parent layer of the fullscreen viewer, restored when leaving fullscreen.
    property var    _fsOriginalParent: null
    readonly property int _scrimZ: 9000

    // Live composite "slots" (stage-sized textures), one per participant that has something
    // beneath it in the global cross-layer chain. Rebuilt on layer-children / glass-enabled
    // changes; live ShaderEffectSources keep them updating for movement / animated content.
    property var _glassSlots: []
    // Incrementing z handed to the most recently selected viewer so it sits in front (within
    // the viewer layer). Layer ordering enforces "viewers always below modals" regardless.
    property int _topZ: 1
    // Currently selected viewer, to skip redundant reselects (avoids glass chain churn).
    property var _selectedViewer: null
    // Counter used to cascade successive centred opens so they don't perfectly overlap.
    property int _openCount: 0

    // --- Layer registry. Six predefined named layers, z-ordered back→front. Apps look layers
    // up by name via getLayer() and may opt their viewers into any of them via the `layer`
    // field on openViewer() (default: "viewer"). The launcher's layer is `launcherLayer`. ---
    property var _layers: ({})

    onGlassEnabledChanged: rebuildGlass()

    Component.onCompleted: {
        _layers = {
            "background":   background,
            "presentation": presentation,
            "viewer":       viewerLayer,
            "modal1":       modal1,
            "modal2":       modal2,
            "modal3":       modal3
        };
        createMenu()
    }

    TapHandler {

    }

    // Holder for the cumulative composite chain. Each slot is a stage-sized texture of
    // everything beneath one participant. It must actually be in the render tree for its
    // nested live ShaderEffectSources to keep updating, so it stays visible but sits at the
    // very back, fully covered by the opaque background layer above it.
    Item {
        id: glassSlotHolder
        anchors.fill: wafflesRoot
    }

    // --- Layer 0: background. Animated bg + consumer backdrop content via `backgroundContent`.
    // Replaces the previously-named `backdrop` Item; same role, captured as the bottom of every
    // slot. Opaque base also occludes glassSlotHolder beneath it.
    Item {
        id: background
        anchors.fill: wafflesRoot
        z: 0
    }

    // --- Layer 1: presentation. Reserved for presentation-style content (empty for now). Treated
    // as a glass-stack leaf, so anything dropped here participates in viewers' backdrops.
    Item {
        id: presentation
        anchors.fill: wafflesRoot
        z: 1
    }

    // --- Layer 2: viewer. Normal viewer instances live here (renamed from `topLayer`). ---
    Item {
        id: viewerLayer
        anchors.fill: wafflesRoot
        z: 2
        onChildrenChanged: wafflesRoot.rebuildGlass()
    }

    // --- Layer 3: modal1. Default home of the content launcher (below the fullscreen scrim).
    // Apps that want the launcher above fullscreen instead set wafflesRoot.launcherLayer = "modal3".
    Item {
        id: modal1
        anchors.fill: wafflesRoot
        z: 3
        onChildrenChanged: wafflesRoot.rebuildGlass()
    }

    // --- Layer 4: modal2. Houses the fullscreen scrim and (transiently, while fullscreen is
    // active) the fullscreen viewer + its FS controller. Reparenting happens in setFullscreen.
    Item {
        id: modal2
        anchors.fill: wafflesRoot
        z: 4
        onChildrenChanged: wafflesRoot.rebuildGlass()

        // Fullscreen scrim. Blocks the rest of the UI behind the fullscreen viewer (which is
        // reparented into this same layer while fullscreen). Sits at z=_scrimZ within modal2;
        // the fullscreen viewer gets _scrimZ+1, the FS controller _scrimZ+2.
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

    // --- Layer 5: modal3. Always-on-top placement (above fullscreen scrim). Empty by default.
    Item {
        id: modal3
        anchors.fill: wafflesRoot
        z: 5
        onChildrenChanged: wafflesRoot.rebuildGlass()

        // Floating virtual keyboard (web text entry). Loaded only while shown; closing it hides.
        // No explicit Loader size, so it sizes to the keyboard's implicit size; anchors place it
        // bottom-centre of the stage (the keyboard can then be dragged from there).
        Loader {
            id: floatingKbLoader
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 80
            active: wafflesRoot.floatingKeyboardShown
            sourceComponent: Component {
                DsFloatingKeyboard {
                    onCloseRequested: wafflesRoot.floatingKeyboardShown = false
                }
            }
        }
    }

    // Floating "open launcher" button. Visible when the launcher exists, exposes a `shown`
    // property, has been closed, and no fullscreen viewer is active (so the scrim doesn't
    // clash). Sits at the stage's bottom-left. Tap → re-show the launcher.
    Rectangle {
        id: launcherFab
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 48
        anchors.bottomMargin: 48
        width: 80
        height: 80
        radius: 14
        z: 10  // above every predefined layer (max z=5)
        color: DsTheme.surfaceVariant
        border.color: DsTheme.stroke
        border.width: 1

        readonly property bool _eligible:
            wafflesRoot.launcherButtonVisible
            && _private.launcher
            && ("shown" in _private.launcher)
            && _private.launcher.shown === false
            && wafflesRoot._fullscreenViewer === null

        visible: opacity > 0
        opacity: _eligible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
            shadowVerticalOffset: 6
            shadowBlur: 0.7
            shadowOpacity: 1.0
        }

        VectorImage {
            anchors.centerIn: parent
            width: 36
            height: 36
            source: Ds.env.expandUrl("%APP%/data/images/waffles/content_launcher/content.svg")
            preferredRendererType: VectorImage.CurveRenderer
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: DsTheme.surfaceText
            }
        }

        TapHandler {
            onTapped: {
                if (_private.launcher && ("shown" in _private.launcher))
                    _private.launcher.shown = true
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

    // One slot = the cumulative composite of everything beneath its owner in the global glass
    // chain, composited FLAT (no nesting). Two kinds of leaves:
    //   - baseLeaves   : whole Items captured at the slot's full size (the background +
    //                    presentation layers).
    //   - lowerViewers : viewer-like items captured via their `captureItem` (their glass-free
    //                    contentLayer) at their on-stage rect — so an upper viewer's glass shows
    //                    lower viewers WITH their chrome ("compound" look) without recursion.
    Component {
        id: glassSlotComponent
        Item {
            id: slot
            property var baseLeaves: []
            property var lowerViewers: []
            anchors.fill: parent
            Repeater {
                model: slot.baseLeaves
                ShaderEffectSource {
                    required property var modelData
                    anchors.fill: parent
                    sourceItem: modelData
                    live: true
                    hideSource: false
                }
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

    // Look up a named layer Item. Returns null for unknown names.
    function getLayer(name) {
        return _layers[name] || null;
    }

    function createMenu() {
        if (launcher.status == Component.Ready)
            completeMenu()
        else
            launcher.statusChanged.connect(completeMenu)
    }

    function completeMenu() {
        if(launcher.status == Component.Ready) {
            // Parented into the launcher's configured layer (default "modal1"). Glass is wired
            // through the cross-layer chain built by rebuildGlass.
            let parentLayer = getLayer(launcherLayer) || modal1;
            _private.launcher = launcher.createObject(parentLayer, {
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
            // Resolve layer (default "viewer"). The `layer` field is consumed by the stage and
            // stripped before forwarding to createObject so it doesn't try to set a non-existent
            // property on the viewer instance.
            let layerName = viewerProps.layer || "viewer";
            let parentLayer = getLayer(layerName) || viewerLayer;
            let propsForViewer = Object.assign({}, viewerProps);
            delete propsForViewer.layer;

            let viewerInstance = viewer.createObject(parentLayer, propsForViewer);
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
    // destroys it. Destruction triggers its layer's onChildrenChanged → rebuildGlass.
    function closeViewer(v) {
        if (!v) return;
        // If the viewer being closed is fullscreen, tear down the scrim + controller first.
        if (v === _fullscreenViewer) {
            _hideController();
            _scrimShown = false;
            _fullscreenViewer = null;
            _fsOriginalParent = null;
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
    // another is fullscreen snaps the previous one back first. Entering reparents v into modal2
    // and animates it to a margin-fit of the stage over the scrim; leaving animates it back to
    // its pre-fullscreen geometry and reparents back to its original layer.
    function setFullscreen(v, on) {
        if (!v) return;
        if (on) {
            if (wafflesRoot._fullscreenViewer === v) return;
            if (wafflesRoot._fullscreenViewer) {
                let prev = wafflesRoot._fullscreenViewer;
                let pr = wafflesRoot._fsRestore;
                prev.fullscreen = false;
                prev.x = pr.x; prev.y = pr.y; prev.viewerWidth = pr.width; prev.viewerHeight = pr.height;
                if (wafflesRoot._fsOriginalParent)
                    prev.parent = wafflesRoot._fsOriginalParent;
                prev.z = ++wafflesRoot._topZ;
                wafflesRoot._fullscreenViewer = null;
                wafflesRoot._fsOriginalParent = null;
            }
            wafflesRoot._fsRestore = Qt.rect(v.x, v.y, v.viewerWidth, v.viewerHeight);
            wafflesRoot._fsOriginalParent = v.parent;
            v.parent = modal2;
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
            let originalParent = wafflesRoot._fsOriginalParent || viewerLayer;
            v.fullscreen = false;
            wafflesRoot._scrimShown = false;
            wafflesRoot._hideController();
            wafflesRoot._animateViewer(v, rr.x, rr.y, rr.width, rr.height, function() {
                v.parent = originalParent;
                wafflesRoot._fsOriginalParent = null;
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

    // Shows the per-type fullscreen controller, bound to v, above the fullscreen viewer.
    // Lives in modal2 alongside the scrim and the fullscreen viewer.
    function _showController(v) {
        let comp = wafflesRoot.fullscreenController;   // (could vary by v.viewerType later)
        if (!comp || comp.status !== Component.Ready) return;
        if (!wafflesRoot._fsController) {
            wafflesRoot._fsController = comp.createObject(modal2, { "z": wafflesRoot._scrimZ + 2 });
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

    // Brings a viewer to the front and marks it selected (deselecting the rest within the
    // viewer layer). Selected viewers show their controls/title; the z bump reorders the glass
    // chain so the front viewer samples everything beneath it. Pass null to deselect all.
    function selectViewer(v) {
        if (v === _selectedViewer)
            return;
        _selectedViewer = v;
        let kids = viewerLayer.children;
        for (let i = 0; i < kids.length; i++) {
            let p = kids[i];
            if (p && ("selected" in p))
                p.selected = (p === v);
        }
        if (v)
            v.z = ++_topZ;
        rebuildGlass();
    }

    // Rebuilds the cross-layer glass chain. Every item with a `backdropSource` across every
    // participant layer (viewer / modal1 / modal2 / modal3) is sorted globally by (layer.z,
    // item.z, doc-index), and each gets a flat-composite slot containing every leaf below it:
    //   - background + presentation as full-Item base leaves
    //   - the captureItem of every earlier participant
    // Movement and animated content update for free via live ShaderEffectSources; only
    // add/remove/reorder and glassEnabled toggling triggers a rebuild.
    function rebuildGlass() {
        for (let i = 0; i < _glassSlots.length; i++) {
            if (_glassSlots[i])
                _glassSlots[i].destroy();
        }
        _glassSlots = [];

        let participantLayers = [viewerLayer, modal1, modal2, modal3];

        // Glass disabled: drop every participant's source so the live chain stops entirely.
        if (!glassEnabled) {
            for (let li = 0; li < participantLayers.length; li++) {
                let layer = participantLayers[li];
                for (let k = 0; k < layer.children.length; k++) {
                    let q = layer.children[k];
                    if (q && ("backdropSource" in q))
                        q.backdropSource = null;
                }
            }
            return;
        }

        // Collect participants in global z order (layer.z, then item.z, then document order).
        let participants = [];
        for (let li = 0; li < participantLayers.length; li++) {
            let layer = participantLayers[li];
            let kids = [];
            for (let k = 0; k < layer.children.length; k++) {
                let kid = layer.children[k];
                if (!kid || !("backdropSource" in kid)) continue;
                kids.push({ item: kid, idx: k });
            }
            kids.sort((a, b) => {
                let d = (a.item.z || 0) - (b.item.z || 0);
                return d !== 0 ? d : (a.idx - b.idx);
            });
            for (let kd of kids) participants.push(kd.item);
        }

        // One slot per participant. Base leaves are shared (the two leaf layers); lowerViewers
        // grows as we walk up the chain.
        let baseLeaves = [background, presentation];
        for (let i = 0; i < participants.length; i++) {
            let p = participants[i];
            let slot = glassSlotComponent.createObject(glassSlotHolder, {
                "baseLeaves": baseLeaves,
                "lowerViewers": participants.slice(0, i)
            });
            _glassSlots.push(slot);
            p.backdropSource = slot;
        }
    }

    TapHandler {
        target: null
        onTapped: (point,button)=>{
            // While fullscreen the scrim handles taps (margin tap exits); ignore stage taps.
            if (wafflesRoot._fullscreenViewer) return;
            // A tap that lands on the launcher is the launcher's to handle (e.g. opening an item).
            // This stage TapHandler also fires for that same tap (drag-threshold TapHandlers don't
            // grab exclusively), so without this guard the select-by-location below would run with
            // the tap point on the launcher — not over the freshly-centred viewer — and call
            // selectViewer(null), deselecting the viewer the launcher just opened (its controls
            // then never show). Skip selection changes for taps over the visible launcher.
            if (_private.launcher && _private.launcher.visible
                && _private.launcher.contains(_private.launcher.mapFromGlobal(point.globalPosition)))
                return;
            if(tapCount == 2 && _private.launcher){
                // Reposition centred on the tap point, and re-show if the launcher was hidden
                // via its close button. The `shown` property is optional on the launcher type;
                // launchers that don't expose it (e.g. DsTestLauncher) simply ignore the bump.
                _private.launcher.x = Math.round(point.position.x - _private.launcher.width / 2)
                _private.launcher.y = Math.round(point.position.y)
                if ("shown" in _private.launcher) _private.launcher.shown = true
            }
            let inView = null;
            for(let i=0;i<viewerLayer.children.length;i++){
              let child = viewerLayer.children[i];
              if(child.contains(child.mapFromGlobal(point.globalPosition))){
                  inView = child;
              }

            }
            wafflesRoot.selectViewer(inView);

        }
    }
}
