import QtQuick

Item {
    id: dsViewer
    enum ViewerType {
        TitledMediaViewer,
        Launcher,
        PresentationController,
        FullscreenController,
        Unknown
    }

    // Enter/exit animation styles (open/close).
    enum Anim {
        None,        // no animation (snap)
        Fade,        // fade in / out
        Grow,        // scale up on enter, shrink on exit
        GrowBounce,  // scale with overshoot (bounce) on enter, anticipate on exit
        FadeRise     // fade + rise up on enter, fade + sink down on exit
    }

    property var signalObject: null
    property int viewerType: DsViewer.ViewerType.Unknown
    property int viewerWidth: 400
    property int viewerHeight: 300

    // The composite "slot" the stage assigns: a stage-sized item holding everything
    // below this viewer in z-order. Subclasses sample their own rect from it for the glass.
    property Item backdropSource: null

    // The item the stage captures into other viewers' slots. Must sit at (0,0) within the
    // viewer and must NOT contain any glass (so captures don't recurse). Subclasses that have
    // a glass layer override this to point at their glass-free content layer.
    property Item captureItem: dsViewer

    // --- Enter/exit animation config (defaults supplied by the stage; per-viewer overridable) ---
    property int  enterAnimation:    DsViewer.Anim.Fade
    property int  exitAnimation:     DsViewer.Anim.Fade
    property int  animationDuration: 300
    property real animationRise:     40   // px of travel for the rise/sink styles

    // Emitted (e.g. by the close button) to ask the owner (the stage) to close this viewer.
    signal closeRequested()

    // Visual offset used by the rise/sink styles. Kept separate from x/y so the animation
    // does not fight viewer dragging (which sets x/y directly).
    transform: Translate { id: _animTranslate }

    // One reusable player drives all the styles; channels not used by a style are no-ops.
    ParallelAnimation {
        id: _animPlayer
        property var pendingDone: null
        NumberAnimation { id: _aOpacity; target: dsViewer;       property: "opacity"; duration: dsViewer.animationDuration }
        NumberAnimation { id: _aScale;   target: dsViewer;       property: "scale";   duration: dsViewer.animationDuration }
        NumberAnimation { id: _aRise;    target: _animTranslate; property: "y";       duration: dsViewer.animationDuration }
        onFinished: { let cb = pendingDone; pendingDone = null; if (cb) cb(); }
    }

    // Play the configured enter animation (called by the stage right after creation).
    function playEnter() {
        if (enterAnimation === DsViewer.Anim.None) {
            opacity = 1; scale = 1; _animTranslate.y = 0;
            return;
        }
        _setupAnim(enterAnimation, true);
        _animPlayer.pendingDone = null;
        _animPlayer.start();
    }

    // Play the configured exit animation, then invoke onComplete (the stage destroys the viewer).
    function playExit(onComplete) {
        if (exitAnimation === DsViewer.Anim.None) {
            if (onComplete) onComplete();
            return;
        }
        _setupAnim(exitAnimation, false);
        _animPlayer.pendingDone = onComplete;
        _animPlayer.start();
    }

    // Configures the three channels for a style + direction, then snaps to the start state so the
    // first rendered frame isn't the end state.
    function _setupAnim(type, entering) {
        _animPlayer.stop();
        // Baseline: every channel is a no-op (from == to == current state).
        _aOpacity.from = dsViewer.opacity;   _aOpacity.to = dsViewer.opacity;   _aOpacity.easing.type = Easing.Linear;
        _aScale.from   = dsViewer.scale;     _aScale.to   = dsViewer.scale;     _aScale.easing.type   = Easing.OutQuad;
        _aRise.from    = _animTranslate.y;   _aRise.to    = _animTranslate.y;   _aRise.easing.type    = Easing.OutQuad;

        switch (type) {
        case DsViewer.Anim.Fade:
            _aOpacity.from = entering ? 0 : 1; _aOpacity.to = entering ? 1 : 0;
            break;
        case DsViewer.Anim.Grow:
            _aScale.from = entering ? 0 : 1; _aScale.to = entering ? 1 : 0;
            _aScale.easing.type = entering ? Easing.OutQuad : Easing.InQuad;
            break;
        case DsViewer.Anim.GrowBounce:
            _aScale.from = entering ? 0 : 1; _aScale.to = entering ? 1 : 0;
            _aScale.easing.type = entering ? Easing.OutBack : Easing.InBack;
            break;
        case DsViewer.Anim.FadeRise:
            _aOpacity.from = entering ? 0 : 1; _aOpacity.to = entering ? 1 : 0;
            _aRise.from = entering ? dsViewer.animationRise : 0;
            _aRise.to   = entering ? 0 : dsViewer.animationRise;
            break;
        }

        // Snap to the start state immediately.
        dsViewer.opacity = _aOpacity.from;
        dsViewer.scale   = _aScale.from;
        _animTranslate.y = _aRise.from;
    }
}
