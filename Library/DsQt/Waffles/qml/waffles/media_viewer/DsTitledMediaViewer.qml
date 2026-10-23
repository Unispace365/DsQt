import QtQuick
import QtMultimedia
import Dsqt.Core
import Dsqt.Waffles

DsViewer {
    id: root
    viewerType: DsViewer.ViewerType.TitledMediaViewer
    property list<DsControlSet> controls

    property var model: null
    readonly property var config: configObj

    property DsWaffleStage stage: null
    property bool playbackSuspended: stage ? stage.foregroundPlaybackSuspended : false
    //@brief this defines this objects default config.
    //the keys of theconfig map to the property of the TitledMediaViewers config object which is passed to controlSets.
    //that config object is filled with the values from your model. If you are using custom contolSets that
    //know about your model, then you can ignore this and just get data directly from the model.
    //if you setup your model's keys to match these values you won't need to set modelConfig.
    //if your model's data for the given property is different than the default you can override
    //this by copying the object and changing the values to match your model's keys.
    property var modelConfig: {
        "media": "media",
        "title": "title",
        "subtitle" : "subtitle"
    }
    property alias mediaViewer: mediaView.viewer

    // Fill mode for the media. PreserveAspectFit letterboxes, revealing the glass behind.
    property int mediaFillMode: Image.PreserveAspectCrop

    // --- Glass config: defaults to the stage globals, override per-instance if needed ---
    property bool  glassEnabled:     stage ? stage.glassEnabled : true
    property color glassTint:        stage ? stage.glassTint : DsTheme.surface
    property real  glassTintOpacity: stage ? stage.glassTintOpacity : DsTheme.glassTintOpacity
    property real  glassBlur:        stage ? stage.glassBlur : DsTheme.glassBlur
    property int   glassBlurMax:     stage ? stage.glassBlurMax : DsTheme.glassBlurMax
    property real  glassBlurMultiplier: stage ? stage.glassBlurMultiplier : DsTheme.glassBlurMultiplier
    property real  glassRadius:      stage ? stage.glassRadius : DsTheme.glassRadius
    property color glassBorderColor: stage ? stage.glassBorderColor : DsTheme.stroke
    property real  glassBorderWidth: stage ? stage.glassBorderWidth : DsTheme.glassBorderWidth
    // Background shown behind the media when glass is off (developer configurable).
    property color glassFallbackColor: glassTint
    // Whether the MEDIA region paints its own glass. Off by default: the media is opaque (like
    // the design) and only the control sets (title bar, buttons) show glass. Turn on for glass
    // behind letterbox bars / transparent media.
    property bool  mediaGlassEnabled: false

    // --- Sizing config ---
    // sizeToMedia: size the viewer to the media's natural dimensions. matchAspectRatio: preserve
    // the media's aspect ratio while clamping. imageWidth: the base width used when fitting to
    // aspect (defaults to the app_settings value via the stage). Bounds default from settings
    // ([waffles.viewer] in waffles_settings.toml, via the stage); all are overridable on creation.
    property bool sizeToMedia: false
    property bool matchAspectRatio: false
    property real imageWidth: stage ? stage.viewerImageWidth : 800
    // Default size for web content (no natural aspect to fit). Used when the media is web and the
    // caller didn't request sizeToMedia / matchAspectRatio. Defaults from [waffles.viewer] webWidth
    // / webHeight via the stage.
    property real webWidth:  stage ? stage.viewerWebWidth : 1280
    property real webHeight: stage ? stage.viewerWebHeight : 800
    property real minWidth:  stage ? stage.viewerMinWidth : 200
    property real maxWidth:  stage ? stage.viewerMaxWidth : 1600
    property real minHeight: stage ? stage.viewerMinHeight : 150
    property real maxHeight: stage ? stage.viewerMaxHeight : 1200
    onSizeToMediaChanged: applySizing()
    onMatchAspectRatioChanged: applySizing()
    onImageWidthChanged: applySizing()
    onWebWidthChanged: applySizing()
    onWebHeightChanged: applySizing()

    // --- Pinch-zoom config ---
    // Opt-in (default off). When true, viewers can be resized by:
    //   - touch multi-touch pinch
    //   - Alt + click-and-drag (mouse): vertical drag distance maps to scale (up = in, down = out)
    //   - Alt + scroll wheel (mouse)
    // The resize is uniform (preserves aspect) and clamped to [minWidth, maxWidth] / [minHeight,
    // maxHeight] — i.e. honours the same bounds applySizing() does — and stays centred on the
    // viewer's centre point so it doesn't snap to a corner.
    property bool pinchEnabled: false
    readonly property bool viewerZoomEnabled: root.pinchEnabled && root.viewerDragEnabled
                                               && (!root.contentInteractive || !root.contentInputEnabled)

    // --- Enter/exit animation: defaults from the stage, overridable per viewer on creation ---
    enterAnimation:    stage ? stage.viewerEnterAnimation : DsViewer.Anim.Fade
    exitAnimation:     stage ? stage.viewerExitAnimation : DsViewer.Anim.Fade
    animationDuration: stage ? stage.viewerAnimationDuration : 300

    // Default the signal target to ourselves so the built-in controls (e.g. the close button)
    // drive this viewer unless a consumer supplies its own controller object.
    signalObject: root
    // Called by the close button (via signalObject); asks the stage to close us with the exit
    // animation. The stage destroys the viewer once the animation finishes.
    function close() { root.closeRequested() }

    // Selected viewers show their title/controls; unselected ones hide them and the glass
    // shrinks to just the media area. The stage sets this and raises the selected viewer.
    property bool selected: false
    // Auto-hide: controls fade out after controlsIdleMs of no interaction (so they stop covering
    // the content) and return when the viewer is touched/hovered/dragged. 0 disables auto-hide.
    property int  controlsIdleMs: stage ? stage.controlsIdleMs : 4000
    property bool controlsAwake: true
    signal interacted()
    property point _lastHoverPosition: Qt.point(-1, -1)
    function hoverAt(position) {
        if (position.x === _lastHoverPosition.x && position.y === _lastHoverPosition.y) return;
        _lastHoverPosition = position;
        if (root.contains(root.mapFromItem(null, position.x, position.y)))
            root.wake();
    }
    readonly property bool interactionActive: contentPress.active
        || root.controls.some(control => control.interactionActive)
    readonly property bool controlsHidden: root.fullscreen
        ? !!(root.stage && root.stage._fsController && ("controlsHidden" in root.stage._fsController)
             && root.stage._fsController.controlsHidden)
        : root.selected && root.controlsIdleMs > 0 && !root.controlsAwake
    Timer { id: idleTimer; interval: Math.max(1, root.controlsIdleMs); onTriggered: root.controlsAwake = false }
    onControlsIdleMsChanged: wake()
    onInteractionActiveChanged: if (!root.controlsHidden) wake()

    readonly property bool contentInteractive: root.mediaViewer
        ? (root.mediaViewer.mediaType === "web" || root.mediaViewer.mediaType === "pdf") : false

    // All windowed controls share one timeout. Fullscreen uses the stage's controller instead.
    onSelectedChanged: { wake(); if (selected) refreshGlass(); }
    onFullscreenChanged: { wake(); if (!fullscreen && selected) refreshGlass(); }
    onControlsAwakeChanged: _applyControls()
    // TEMP diagnostic — flip on to log control state on each _applyControls.
    property bool _debugControls: false
    function _applyControls() {
        let base = selected && !fullscreen;
        let controlsOn = base && (controlsAwake || controlsIdleMs <= 0);
        if (root._debugControls) {
            let mt = root.mediaViewer ? root.mediaViewer.mediaType : "(none)";
            console.log("[DsTitledMediaViewer] mediaType=" + mt + " selected=" + selected
                        + " fullscreen=" + fullscreen + " controls=" + root.controls.length
                        + " base=" + base + " controlsOn=" + controlsOn
                        + " viewer=" + Math.round(viewerWidth) + "x" + Math.round(viewerHeight)
                        + " pos=" + Math.round(root.x) + "," + Math.round(root.y) + " z=" + root.z);
        }
        for (var i = 0; i < root.controls.length; i++) {
            let c = root.controls[i];
            let want = controlsOn ? 1 : 0;
            c.opacity = want;
            if (root._debugControls)
                console.log("    ctrl[" + i + "] edge=" + c.edge + " wantOpacity=" + want
                            + " size=" + Math.round(c.width) + "x" + Math.round(c.height)
                            + " parent=" + (c.parent ? c.parent : "(none)"));
        }
    }
    // Activity resets only this viewer's controls; it never changes application idle state.
    function wake() {
        controlsAwake = true;
        idleTimer.stop();
        if (controlsIdleMs > 0 && selected && !fullscreen && !interactionActive)
            idleTimer.restart();
        _applyControls();
        root.interacted();
    }
    function containsInput(globalPosition) {
        if (root.contains(root.mapFromGlobal(globalPosition)))
            return true;
        for (let i = 0; i < root.controls.length; i++) {
            const control = root.controls[i];
            if (control.visible && control.enabled && control.opacity > 0
                    && control.contains(control.mapFromGlobal(globalPosition)))
                return true;
        }
        return false;
    }

    // Passive content handlers may also observe taps delivered to an overlapping viewer.
    function selectAt(globalPosition) {
        if (root.stage) {
            if (root.stage._fullscreenViewer && root.stage._fullscreenViewer !== root)
                return;
            if (root.stage.viewerAt(globalPosition, root.parent) !== root)
                return;
            // Fullscreen taps wake controls without changing selection or scrim stacking.
            if (!root.fullscreen && !root.stage._fullscreenViewer)
                root.stage.selectViewer(root);
        }
        root.wake();
    }

    // Dragging (including the title bar) keeps all controls awake.
    onXChanged: wake()
    onYChanged: wake()

    // For PDFs, the lock fixes the viewer in place and enables document interaction.
    // Unlocked PDFs move/resize as a window. Web content retains its content-lock behavior.
    // Disable the media subtree when moving the window so PDF handlers cannot grab gestures.
    property bool contentLocked: false
    readonly property bool pdfContent: root.mediaViewer && root.mediaViewer.mediaType === "pdf"
    readonly property bool contentInputEnabled: root.pdfContent ? root.contentLocked : !root.contentLocked
    readonly property bool viewerDragEnabled: !root.fullscreen && !(root.pdfContent && root.contentLocked)

    // Bumped (over a few frames, after layout) to force the glass to re-sample its real on-screen
    // position. The glass origin is computed via mapToItem, which is NOT reactive to the control
    // sets being reparented into their edge holders on creation — so without this nudge the
    // controls keep sampling their initial (wrong, often blue) location until the viewer is moved.
    property int glassRefreshTick: 0
    Timer {
        id: glassSettleTimer
        interval: 16
        repeat: true
        property int ticks: 0
        onTriggered: { root.glassRefreshTick++; if (++ticks >= 5) { ticks = 0; stop(); } }
    }
    function refreshGlass() { glassSettleTimer.ticks = 0; glassSettleTimer.restart(); }

    // The stage captures this (glass + content) into the slots viewers above sample, so an
    // upper viewer's glass shows the lower viewers WITH their glass — the compound look. Slots
    // are flat (leaf sources only), so this nesting has no depth chain that could blank out.
    captureItem: contentLayer


    QtObject {
        id: configObj
        property var media: model[root.modelConfig.media]
        property var title: model[root.modelConfig.title]
        property var subtitle: model[root.modelConfig.subtitle]
    }

    // Glass settings handed to the control sets so their backgrounds can match (or diverge
    // from) the media area. viewerX/Y are the viewer's stage position; nested controls add
    // their own offset within the viewer to sample the right region. refresh recomputes that
    // offset when the viewer moves/resizes.
    QtObject {
        id: glassContext
        // Controls sample THIS viewer's slot so their glass shows the compound (lower viewers +
        // backdrop), the same source the media region uses. Each panel self-samples its own slice.
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
        property real  refresh: root.x + root.y + root.width + root.height + root.glassRefreshTick
    }

    width: mediaView.width;
    height: mediaView.height;

    Component.onCompleted: { setControls(); applySizing(); _applyControls(); refreshGlass(); _connectInteractions(); }
    onControlsChanged: setControls()

    // Connects each control set's `interacted` signal to wake() so using a control (e.g. the
    // scrubber) resets the idle auto-hide. Run once after creation.
    function _connectInteractions() {
        for (var i = 0; i < root.controls.length; i++)
            root.controls[i].interacted.connect(root.wake)
    }

    // Sets viewerWidth/viewerHeight from the sizing config: optionally to the media's natural
    // size, optionally preserving the media aspect ratio, always clamped to [min,max]. When
    // fitting to aspect (and not sizing to the media), the base width is `imageWidth` — the
    // app_settings-driven default — so an image viewer becomes imageWidth-wide at the media aspect.
    function applySizing() {
        let media = (config && config.media) ? config.media : null;
        let mw = (media && media.width)  ? media.width  : viewerWidth;
        let mh = (media && media.height) ? media.height : viewerHeight;
        // Web content has no natural aspect to fit — default it to the configured web size
        // (unless the caller asked to size to the media or match its aspect).
        let isWeb = media && ("" + (media.type || "")).toLowerCase() === "web";
        let w = sizeToMedia ? mw : (matchAspectRatio ? imageWidth : (isWeb ? webWidth : viewerWidth));
        let h = sizeToMedia ? mh : (isWeb ? webHeight : viewerHeight);
        if (matchAspectRatio && mw > 0 && mh > 0) {
            let aspect = mw / mh;
            w = Math.max(minWidth, Math.min(maxWidth, w));
            h = w / aspect;
            if (h < minHeight) { h = minHeight; w = h * aspect; }
            if (h > maxHeight) { h = maxHeight; w = h * aspect; }
            w = Math.max(minWidth, Math.min(maxWidth, w));
        } else {
            w = Math.max(minWidth, Math.min(maxWidth, w));
            h = Math.max(minHeight, Math.min(maxHeight, h));
        }
        viewerWidth = Math.round(w);
        viewerHeight = Math.round(h);
    }

    // Resize uniformly to newW x newH, preserving the requested aspect ratio, clamping to
    // [minWidth, maxWidth] / [minHeight, maxHeight], and re-centring the viewer on its own
    // centre point (so the zoom doesn't shift the panel toward 0,0). Used by the pinch /
    // Alt-drag / Alt-wheel handlers below.
    function _zoomTo(newW, newH) {
        if (newW <= 0 || newH <= 0) return;
        const aspect = newW / newH;
        let w = Math.max(minWidth, Math.min(maxWidth, newW));
        let h = w / aspect;
        if (h < minHeight) { h = minHeight; w = h * aspect; }
        if (h > maxHeight) { h = maxHeight; w = h * aspect; }
        w = Math.max(minWidth, Math.min(maxWidth, w));
        h = w / aspect;
        const dx = (root.viewerWidth - w) / 2;
        const dy = (root.viewerHeight - h) / 2;
        root.x = Math.round(root.x + dx);
        root.y = Math.round(root.y + dy);
        root.viewerWidth = Math.round(w);
        root.viewerHeight = Math.round(h);
    }


    //this function positions the control in their respective edges
    function setControls() {
        for(var i=0; i < controls.length; i++){
            let ctrl = controls[i];
            ctrl.config = config
            ctrl.model = model
            ctrl.glass = glassContext
            let edge = ctrl.edge;
            switch(edge) {
            case DsControlSet.Edge.TopOuter:
                ctrl.parent = Qt.binding(()=>{return topOuter;})
                ctrl.width = Qt.binding(()=>{return topOuter.width;})
                topOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                topOuter.enabled = true;
                break;
            case DsControlSet.Edge.TopInner:
                ctrl.parent = Qt.binding(()=>{return topInner;})
                ctrl.width = Qt.binding(()=>{return topInner.width;})
                topInner.offset = Qt.binding(()=>{return ctrl.offset;})
                topInner.enabled = true;
                break;
            case DsControlSet.Edge.BottomOuter:
                ctrl.parent = Qt.binding(()=>{return bottomOuter;})
                ctrl.width = Qt.binding(()=>{return bottomOuter.width;})
                bottomOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                bottomOuter.enabled = true;
                break;
            case DsControlSet.Edge.BottomInner:
                ctrl.parent = Qt.binding(()=>{return bottomInner;})
                ctrl.width = Qt.binding(()=>{return bottomInner.width;})
                bottomInner.offset = Qt.binding(()=>{return ctrl.offset;})
                bottomInner.enabled = true;
                break;
            case DsControlSet.Edge.LeftOuter:
                ctrl.parent = Qt.binding(()=>{return leftOuter;})
                ctrl.height = Qt.binding(()=>{return leftOuter.height;})
                leftOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                leftOuter.enabled = true;
                break;
            case DsControlSet.Edge.LeftInner:
                ctrl.parent = Qt.binding(()=>{return leftInner;})
                ctrl.height = Qt.binding(()=>{return leftInner.height;})
                leftInner.offset = Qt.binding(()=>{return ctrl.offset;})
                leftInner.enabled = true;
                break;
            case DsControlSet.Edge.RightOuter:
                ctrl.parent = Qt.binding(()=>{return rightOuter;})
                ctrl.height = Qt.binding(()=>{return rightOuter.height;})
                rightOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                rightOuter.enabled = true;
                break;
            case DsControlSet.Edge.RightInner:
                ctrl.parent = Qt.binding(()=>{return rightInner;})
                ctrl.height = Qt.binding(()=>{return rightInner.height;})
                rightInner.offset = Qt.binding(()=>{return ctrl.offset;})
                rightInner.enabled = true;
                break;
            case DsControlSet.Edge.Center:
                ctrl.parent = Qt.binding(()=>{return mediaView;})
                ctrl.width = Qt.binding(()=>{return mediaView.width;})
                ctrl.height = Qt.binding(()=>{return mediaView.height;})
                break;

            case DsControlSet.Edge.CenterBack:
                ctrl.parent = Qt.binding(()=>{return background;})
                background.horizontalOffset = Qt.binding(()=>{return ctrl.horizontalOffset;})
                background.verticalOffset = Qt.binding(()=>{return ctrl.verticalOffset;})
                break;
            }
        }
    }
    // The viewer's captured appearance: its glass panel + media + controls. The stage captures
    // THIS into the slots that viewers above sample, so an upper viewer's glass blurs the lower
    // viewers together with their own glass (compound). childrenRect spans the glass + controls.
    Item {
        id: contentLayer
        anchors.fill: parent
        Item {
            anchors.fill: parent
            z: 100
            PointHandler {
                id: contentPress
                acceptedButtons: Qt.AllButtons
                onPointChanged: if (active && !root.controlsHidden) root.wake()
            }
            // Remember movement outside the media too, so returning to the same pixel wakes
            // controls. This observer never blocks input; the media's hover shield does that.
            HoverHandler {
                blocking: false
                margin: root.stage ? root.stage.width + root.stage.height : root.width + root.height
                onPointChanged: if (hovered && (root.selected || root.fullscreen)) root.hoverAt(point.scenePosition)
            }
        }

        // The media region's OWN glass — off by default (media is opaque, like the design). It
        // uses the same DsGlassBackground element + glass context as the control sets do. Turn on
        // (mediaGlassEnabled) for glass behind letterbox bars / transparent media.
        DsGlassBackground {
            id: mediaGlass
            z: -1
            anchors.fill: mediaView
            visible: root.mediaGlassEnabled
            context: glassContext
            fallbackColor: root.glassFallbackColor
            borderWidth: 0   // media region has no border in the design
            topLeftRadius: 0
            topRightRadius: 0
            bottomLeftRadius: 0
            bottomRightRadius: 0
        }

        // Consume body input below the media/controls, which get first chance to handle it.
        // MouseArea provides mouse dragging; the touch surface below handles every finger.
        //
        // When pinchEnabled is on and the user presses with Alt held, body-drag is suppressed
        // so the Alt-drag zoom handler gets to run uncontested.
        MouseArea {
            id: bodyDrag
            anchors.fill: mediaView
            acceptedButtons: Qt.AllButtons
            property int _pressMods: 0
            drag.target: (!root.viewerDragEnabled || (root.pinchEnabled && (bodyDrag._pressMods & Qt.AltModifier))) ? null : root
            onPressed: (mouse) => {
                bodyDrag._pressMods = mouse.modifiers;
                if (root.stage) root.stage.selectViewer(root);
                root.wake();
            }
            onReleased: bodyDrag._pressMods = 0
            onCanceled: bodyDrag._pressMods = 0
            onWheel: event => { event.accepted = true }
        }
        MultiPointTouchArea {
            anchors.fill: mediaView
            mouseEnabled: false
            onGestureStarted: gesture => {
                if (!root.viewerZoomEnabled) gesture.grab()
            }
            onPressed: {
                if (root.stage) root.stage.selectViewer(root);
                root.wake();
            }
            DragHandler {
                target: root
                enabled: root.viewerDragEnabled
                acceptedDevices: PointerDevice.TouchScreen
                maximumPointCount: 1
            }
        }

        // Mouse movement anywhere over the viewer (incl. over the controls) keeps controls awake.

        Item {
            id: background
            property real horizontalOffset:0
            property real verticalOffset:0
            anchors.horizontalCenter: mediaView.horizontalCenter
            anchors.verticalCenter: mediaView.verticalCenter
            anchors.horizontalCenterOffset: horizontalOffset
            anchors.verticalCenterOffset: verticalOffset
        }

        //this is the media view. It has several children that are designed to only show one based on the media
        //type. We should make this explicit so the we can use VectorImage and Web element as well.
        Item {
            id: mediaView
            width: root.viewerWidth;
            height: root.viewerHeight
            property alias viewer: viewer
            // Qt's PDF tap handlers are passive. Own taps at the viewer boundary so
            // selecting text or following a link cannot activate a hotspot underneath.
            TapHandler {
                enabled: root.pdfContent && root.contentInputEnabled
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: point => root.selectAt(point.globalPosition)
            }
            DsMediaViewer {
                id: viewer
                anchors.fill: parent
                enabled: root.contentInputEnabled
                media: root.config.media
                fillMode: root.mediaFillMode
                autoPlay: true
                playbackSuspended: root.playbackSuspended
                // Honour `media.loop` from the bridge media descriptor: when truthy, loop
                // forever; otherwise play once (including Qt 6.12 AnimatedImage, with the
                // viewer's KeepLastFrame policy holding the final frame on stop).
                loops: (root.config.media && root.config.media.loop) ? MediaPlayer.Infinite : 1
                visible: true
            }
        }

        // Window gestures sit above the media; disabled for an interactive PDF.
        Item {
            anchors.fill: mediaView
            // --- Pinch / Alt-drag / Alt-wheel zoom handlers (gated on pinchEnabled) ---
            // All three apply via the same _zoomTo() helper, so the clamp + recentre logic is shared.
            // PinchArea retains its touch grab, preventing a PDF underneath from
            // taking the same fingers. Mouse zoom uses Alt to avoid normal interaction.
            PinchArea {
                id: pinchZoom
                objectName: "viewerPinch"
                anchors.fill: parent
                enabled: root.viewerZoomEnabled
                // PinchArea owns touch presses even when only one finger is used.
                // Select on a single tap without giving up the pinch grab to a PDF below.
                TapHandler {
                    acceptedDevices: PointerDevice.TouchScreen
                    gesturePolicy: TapHandler.DragThreshold
                    onTapped: point => root.selectAt(point.globalPosition)
                }
                property real baseW: 0
                property real baseH: 0
                onPinchStarted: {
                    pinchZoom.baseW = root.viewerWidth;
                    pinchZoom.baseH = root.viewerHeight;
                    if (root.stage) root.stage.selectViewer(root);
                    root.wake();
                }
                onPinchUpdated: pinch => {
                    root._zoomTo(pinchZoom.baseW * pinch.scale,
                                 pinchZoom.baseH * pinch.scale);
                }
            }

            DragHandler {
                id: altZoomDrag
                enabled: root.viewerZoomEnabled
                target: null
                acceptedButtons: Qt.LeftButton
                acceptedDevices: PointerDevice.Mouse
                acceptedModifiers: Qt.AltModifier
                grabPermissions: PointerHandler.CanTakeOverFromAnything | PointerHandler.ApprovesTakeOverByAnything
                property real baseW: 0
                property real baseH: 0
                onActiveChanged: {
                    if (active) { altZoomDrag.baseW = root.viewerWidth; altZoomDrag.baseH = root.viewerHeight; }
                }
                onActiveTranslationChanged: {
                    if (!altZoomDrag.active) return;
                    // Vertical drag → exponential scale: up = zoom in, down = zoom out.
                    // 200 px of drag ≈ 2× change.
                    const factor = Math.exp(-altZoomDrag.activeTranslation.y / 200 * Math.LN2);
                    root._zoomTo(altZoomDrag.baseW * factor, altZoomDrag.baseH * factor);
                }
            }

            WheelHandler {
                id: altZoomWheel
                enabled: root.viewerZoomEnabled
                acceptedDevices: PointerDevice.Mouse
                acceptedModifiers: Qt.AltModifier
                onWheel: (event) => {
                    // 1 standard wheel tick (120 angleDelta) ≈ 10% zoom.
                    const factor = Math.pow(1.1, event.angleDelta.y / 120);
                    root._zoomTo(root.viewerWidth * factor, root.viewerHeight * factor);
                    event.accepted = true;
                }
            }
        }

        DsContentState {
            anchors.fill: mediaView
            viewer: root.mediaViewer
            contentState: root.model && root.model.errorMessage ? "error"
                : viewer ? viewer.loadState : "empty"
            title: root.model && root.model.errorTitle ? root.model.errorTitle
                : contentState === "loading" ? qsTr("Loading content") : qsTr("Missing content")
            message: root.model && root.model.errorMessage ? root.model.errorMessage
                : contentState === "loading" ? qsTr("Please wait…")
                : contentState === "empty" ? qsTr("There is no content to display here.")
                : qsTr("Sorry! We couldn’t load this piece of media. Please try again.")
            actionText: root.model && root.model.errorMessage ? ""
                : contentState === "error" ? qsTr("Try again") : ""
        }
        // Stay above media (including PDF/web input) and below the controls. Hover is always
        // observed so enabling a hidden-controls surface under a stationary cursor cannot wake it.
        Item {
            id: revealLayer
            anchors.fill: mediaView
            enabled: root.selected || root.fullscreen || (root.contentInteractive && root.contentInputEnabled)
            HoverHandler { blocking: true }
            TapHandler {
                enabled: !root.fullscreen && !root.selected && root.contentInteractive && root.contentInputEnabled
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: point => root.selectAt(point.globalPosition)
            }
            Item {
                anchors.fill: parent
                visible: root.controlsHidden || revealMouse.pressed || revealTouch.touchCount > 0
                MouseArea {
                    id: revealMouse
                    anchors.fill: parent
                    acceptedButtons: Qt.AllButtons
                    preventStealing: true
                    onClicked: mouse => { if (mouse.button === Qt.LeftButton) root.wake(); }
                    onWheel: event => { root.wake(); event.accepted = true; }
                }
                MultiPointTouchArea {
                    id: revealTouch
                    anchors.fill: parent
                    mouseEnabled: false
                    property int touchCount: 0
                    onPressed: points => { touchCount += points.length; }
                    onGestureStarted: gesture => gesture.grab()
                    onReleased: points => {
                        touchCount = Math.max(0, touchCount - points.length);
                        if (touchCount === 0) root.wake();
                    }
                    onCanceled: points => { touchCount = Math.max(0, touchCount - points.length); }
                }
            }
        }

        //holders for the controlSets.
        Item {
            id:topOuter
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.bottom: mediaView.top
            anchors.bottomMargin: offset
        }

        Item {
            id:bottomOuter
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.top: mediaView.bottom
            anchors.topMargin: offset
        }

        Item {
            id:leftOuter
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.right: mediaView.left
            anchors.rightMargin: offset
        }

        Item {
            id:rightOuter
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.left: mediaView.right
            anchors.leftMargin: offset
        }

        Item {
            id:topInner
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.top: mediaView.top
            anchors.topMargin: offset
        }

        Item {
            id:bottomInner
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.bottom: mediaView.bottom
            anchors.bottomMargin: offset
        }

        Item {
            id:leftInner
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.right: mediaView.left
            anchors.rightMargin: offset
        }

        Item {
            id:rightInner
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.left: mediaView.right
            anchors.leftMargin: offset
        }
    }

}
