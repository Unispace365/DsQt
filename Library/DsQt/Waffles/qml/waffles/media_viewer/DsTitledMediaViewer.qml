import QtQuick 2.15
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
    property real minWidth:  stage ? stage.viewerMinWidth : 200
    property real maxWidth:  stage ? stage.viewerMaxWidth : 1600
    property real minHeight: stage ? stage.viewerMinHeight : 150
    property real maxHeight: stage ? stage.viewerMaxHeight : 1200
    onSizeToMediaChanged: applySizing()
    onMatchAspectRatioChanged: applySizing()
    onImageWidthChanged: applySizing()

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
    Timer { id: idleTimer; interval: root.controlsIdleMs; onTriggered: root.controlsAwake = false }

    // Outer controls (title, corner buttons) show whenever the viewer is selected and not
    // fullscreen (fullscreen hands controls to the stage's fullscreen controller). INNER controls
    // (over the content, e.g. the media transport bar) additionally auto-hide when idle and return
    // on interaction, so they don't cover the content.
    onSelectedChanged: { if (selected) { wake(); refreshGlass(); } else _applyControls() }
    onFullscreenChanged: { if (!fullscreen && selected) { wake(); refreshGlass(); } else _applyControls() }
    onControlsAwakeChanged: _applyControls()
    function _applyControls() {
        let base = selected && !fullscreen;
        let innerOn = base && (controlsAwake || controlsIdleMs <= 0);
        for (var i = 0; i < root.controls.length; i++) {
            let c = root.controls[i];
            c.opacity = ((_edgeIsInner(c.edge) ? innerOn : base) ? 1 : 0);
        }
    }
    function _edgeIsInner(e) {
        return e === DsControlSet.Edge.TopInner || e === DsControlSet.Edge.BottomInner
            || e === DsControlSet.Edge.LeftInner || e === DsControlSet.Edge.RightInner
            || e === DsControlSet.Edge.Center;
    }
    // Resets the inner-controls idle timer and re-applies visibility; called on any interaction.
    function wake() {
        controlsAwake = true;
        if (controlsIdleMs > 0 && selected && !fullscreen) idleTimer.restart();
        _applyControls();
    }
    // Dragging (incl. the title-bar DragHandler, which moves the viewer) keeps inner controls awake.
    onXChanged: wake()
    onYChanged: wake()

    // When true the content (web page / pdf) ignores input so the viewer can be dragged instead;
    // the media controls' lock button toggles this. Implemented as an input-swallowing overlay.
    property bool contentLocked: false

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
        let w = sizeToMedia ? mw : (matchAspectRatio ? imageWidth : viewerWidth);
        let h = sizeToMedia ? mh : viewerHeight;
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

        // Consumes presses over the viewer body so a viewer on top blocks touches/clicks from
        // reaching viewers or the gallery beneath it (a MouseArea accepts the press, unlike a
        // PointerHandler). Also drags the viewer by its body and raises it on press. Sits below
        // the media/controls so they (e.g. a web viewer or buttons) still get input first.
        MouseArea {
            anchors.fill: mediaView
            acceptedButtons: Qt.AllButtons
            drag.target: root.fullscreen ? null : root
            onPressed: (mouse) => { if (root.stage) root.stage.selectViewer(root); root.wake(); }
        }

        // Mouse movement anywhere over the viewer (incl. over the controls) keeps controls awake.
        HoverHandler { onPointChanged: root.wake() }

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
            DsMediaViewer {
                id: viewer
                anchors.fill: parent
                media: root.config.media
                fillMode: root.mediaFillMode
                autoPlay: true
                visible: true // TEMP: media hidden to inspect the glass background
            }
        }

        // Lock overlay: while contentLocked, swallow input over the media so interactive content
        // (web / pdf) doesn't receive it and the viewer can be dragged instead.
        MouseArea {
            anchors.fill: mediaView
            visible: root.contentLocked
            enabled: root.contentLocked
            acceptedButtons: Qt.AllButtons
            drag.target: root.fullscreen ? null : root
            onPressed: (mouse) => { if (root.stage) root.stage.selectViewer(root) }
        }

        // Solid surface-colour fill shown while the media is loading, so the spinner sits on a
        // solid background rather than the glass / empty (or flashing) media region.
        Rectangle {
            anchors.fill: mediaView
            color: DsTheme.surface
            z: 0
            visible: root.mediaViewer ? root.mediaViewer.loading : false
        }

        // Loading indicator: a themed (accent) rotating arc shown while the media is still
        // loading (web pages, network images/videos). Cleared when the media reports it loaded.
        Item {
            id: loadingSpinner
            width: 48
            height: 48
            z: 1
            anchors.centerIn: mediaView
            visible: root.mediaViewer ? root.mediaViewer.loading : false

            Canvas {
                id: spinnerCanvas
                anchors.fill: parent
                Component.onCompleted: requestPaint()
                onPaint: {
                    let ctx = getContext("2d");
                    ctx.reset();
                    let c = width / 2;
                    let r = c - 4;
                    ctx.lineWidth = 4;
                    ctx.lineCap = "round";
                    ctx.strokeStyle = DsTheme.accent;
                    ctx.beginPath();
                    ctx.arc(c, c, r, 0, Math.PI * 1.5);
                    ctx.stroke();
                }
            }

            RotationAnimator {
                target: spinnerCanvas
                from: 0
                to: 360
                duration: 900
                loops: Animation.Infinite
                running: loadingSpinner.visible
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
