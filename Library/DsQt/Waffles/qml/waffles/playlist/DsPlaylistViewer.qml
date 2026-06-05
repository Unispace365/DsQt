pragma ComponentBehavior: Bound

import QtQuick
import Dsqt.Waffles

// Full-stage viewer that plays a playlist of templated slides. Created by DsWaffleStage.openPlaylist
// into the stage's `presentation` layer (z=1), behind the media-viewer layer.
//
// Rendering uses TWO stacked slots so slides can transition (crossfade / slide) rather than cut:
// `_front` shows the current slide; on advance the incoming slide loads into `_back`, the chosen
// transition animates, then `_back` is promoted to front. Templates are chosen from
// `templateByTypeUid` (app-supplied; unmapped slides fall back to DsMediaTemplate).
//
// Transitions:
//   - Built-in: "none" (cut), "fade" (crossfade), "slideLeft", "slideRight".
//   - Per-slide: slide.transition overrides the playlist `defaultTransition`; slide.disableTransition
//     forces "none".
//   - Custom: register customTransitions[name] = function(front, back, durationMs, done) — it
//     animates the two slot Items and MUST call done() when finished. Lets apps add transitions in
//     QML/JS or drive a C++ implementation.
Item {
    id: pv
    anchors.fill: parent

    // Ordered slide descriptors (from DsContentLauncherModel.slidesFor):
    //   { uid, title, kind, typeUid, media, thumbnail, holdTime, disableTransition, transition, record }
    property var slides: []
    // ambient = auto-advance on a timer; interactive = manual next/prev.
    property bool ambient: false
    // Pauses ambient auto-advance (toggled by the presentation controller's play/pause).
    property bool paused: false
    // typeUid → Component map for slide templates (app-supplied; uids are schema-specific).
    property var templateByTypeUid: ({})
    property Component fallbackTemplate: Component { DsMediaTemplate {} }

    // --- Transitions ---
    property string defaultTransition: "fade"
    property int    transitionDuration: 500
    // name → function(front, back, durationMs, done). Built-ins handled internally.
    property var    customTransitions: ({})

    property int index: 0
    readonly property int count: pv.slides ? pv.slides.length : 0
    function _slideAt(i) { return (pv.slides && i >= 0 && i < pv.slides.length) ? pv.slides[i] : null; }
    readonly property var currentSlide: pv._slideAt(pv.index)

    signal closeRequested()
    focus: true

    // --- Enter/exit crossfade (the WHOLE viewer's opacity, independent of the per-slide
    // transitions). The stage starts a new playlist fading in OVER the still-opaque outgoing one for
    // a clean ambient↔interactive crossfade with no background flash; dismiss() fades it back out. ---
    property int  fadeMs: 400
    property bool _dismissing: false
    opacity: 0
    Behavior on opacity { NumberAnimation { duration: pv.fadeMs; easing.type: Easing.InOutQuad } }
    function dismiss() {
        pv._dismissing = true;
        advanceTimer.stop();
        pv.opacity = 0;                // Behavior fades out
        pv.destroy(pv.fadeMs + 50);    // …then self-destroys
    }

    // --- Navigation (loops by wrapping) ---
    function goTo(i) {
        if (pv.count === 0) return;
        const ni = ((i % pv.count) + pv.count) % pv.count;
        if (ni !== pv.index) pv.index = ni;   // → onIndexChanged → _advanceTo
    }
    function next() { pv.goTo(pv.index + 1); }
    function prev() { pv.goTo(pv.index - 1); }

    function _templateFor(slide) {
        if (slide && slide.typeUid && pv.templateByTypeUid[slide.typeUid])
            return pv.templateByTypeUid[slide.typeUid];
        return pv.fallbackTemplate;
    }
    function _transitionName(slide) {
        if (slide && slide.disableTransition) return "none";
        if (slide && slide.transition) return "" + slide.transition;
        return pv.defaultTransition;
    }

    // --- Two slots (front shows current; back receives the incoming slide during a transition). ---
    property Item _front: slotA
    property Item _back:  slotB
    property bool _transitioning: false

    component Slot: Item {
        id: slot
        width: pv.width
        height: pv.height
        property var slideData: null
        property bool slotActive: false
        Loader {
            anchors.fill: parent
            sourceComponent: slot.slideData ? pv._templateFor(slot.slideData) : null
            onLoaded: {
                if (!item) return;
                item.slide = Qt.binding(() => slot.slideData);
                if ("active" in item) item.active = Qt.binding(() => pv.visible && slot.slotActive);
            }
        }
    }
    Slot { id: slotA }
    Slot { id: slotB }

    // Built-in transition animation (x + opacity channels for both slots).
    ParallelAnimation {
        id: transAnim
        property var pendingDone: null
        NumberAnimation { id: aBackX;  target: null; property: "x";       duration: pv.transitionDuration; easing.type: Easing.InOutQuad }
        NumberAnimation { id: aBackO;  target: null; property: "opacity"; duration: pv.transitionDuration }
        NumberAnimation { id: aFrontX; target: null; property: "x";       duration: pv.transitionDuration; easing.type: Easing.InOutQuad }
        NumberAnimation { id: aFrontO; target: null; property: "opacity"; duration: pv.transitionDuration }
        onFinished: { let cb = transAnim.pendingDone; transAnim.pendingDone = null; if (cb) cb(); }
    }

    Component.onCompleted: {
        pv._front.slideData = pv._slideAt(pv.index);
        pv._front.slotActive = true;
        pv._front.opacity = 1; pv._front.x = 0; pv._front.z = 0;
        pv._back.opacity = 0;  pv._back.slotActive = false; pv._back.z = 1;
        pv.forceActiveFocus();
        pv._restartAdvance();
        pv.opacity = 1;                // fade the whole viewer in
    }

    // Advance to slides[index]. NOTE: read the slide via _slideAt(pv.index) directly, NOT via the
    // derived `currentSlide` binding — inside onIndexChanged that binding can still hold the previous
    // value (handler runs before the binding re-evaluates), which made every advance lag one slide.
    onIndexChanged: pv._advanceTo()

    function _advanceTo() {
        const slide = pv._slideAt(pv.index);
        if (!slide) return;
        if (pv._transitioning) { transAnim.stop(); pv._finishTransition(); }   // snap to finish, then go

        const name = pv._transitionName(slide);
        pv._back.slideData = slide;
        pv._back.slotActive = true;

        if (name === "none" || !pv.visible || pv.transitionDuration <= 0) {
            pv._back.z = 1; pv._front.z = 0;   // incoming on top, swap instantly
            pv._back.opacity = 1; pv._back.x = 0;
            pv._finishTransition();
            pv._restartAdvance();
            return;
        }
        pv._transitioning = true;
        pv._runTransition(name, pv._front, pv._back,
                          function () { pv._finishTransition(); pv._restartAdvance(); });
    }

    function _runTransition(name, front, back, done) {
        if (name && pv.customTransitions[name]) {
            back.z = 1; front.z = 0;
            pv.customTransitions[name](front, back, pv.transitionDuration, done);
            return;
        }
        transAnim.stop();
        const w = pv.width;
        aBackX.target = back;  aBackO.target = back;
        aFrontX.target = front; aFrontO.target = front;

        switch (name) {
        case "slideLeft":   // incoming enters from the right, outgoing exits left
            back.z = 1; front.z = 0;
            back.opacity = 1; front.opacity = 1; back.x = w; front.x = 0;
            aBackO.from = 1; aBackO.to = 1;  aFrontO.from = 1; aFrontO.to = 1;
            aBackX.from = w; aBackX.to = 0;  aFrontX.from = 0; aFrontX.to = -w;
            break;
        case "slideRight":  // incoming enters from the left, outgoing exits right
            back.z = 1; front.z = 0;
            back.opacity = 1; front.opacity = 1; back.x = -w; front.x = 0;
            aBackO.from = 1; aBackO.to = 1;  aFrontO.from = 1; aFrontO.to = 1;
            aBackX.from = -w; aBackX.to = 0; aFrontX.from = 0; aFrontX.to = w;
            break;
        case "fade":
        default:
            // Incoming sits UNDER at full opacity — a solid base, so no background bleeds through
            // where the two slides overlap. The outgoing fades out ON TOP, so it also dissolves away
            // cleanly over the incoming's letterbox (fit) gaps instead of hard-cutting to the
            // background. (Unknown transition names fall through to this.)
            back.z = 0; front.z = 1;
            back.opacity = 1; back.x = 0; front.x = 0;
            aBackO.from = 1; aBackO.to = 1;  aBackX.from = 0; aBackX.to = 0;
            aFrontO.from = 1; aFrontO.to = 0; aFrontX.from = 0; aFrontX.to = 0;
            break;
        }
        transAnim.pendingDone = done;
        transAnim.start();
    }

    // Promote _back → _front and reset the old front (now back) off-screen/hidden.
    function _finishTransition() {
        pv._front.slotActive = false;
        pv._front.opacity = 0; pv._front.x = 0; pv._front.slideData = null;
        const oldFront = pv._front;
        pv._front = pv._back;
        pv._back = oldFront;
        pv._front.opacity = 1; pv._front.x = 0; pv._front.slotActive = true;
        pv._transitioning = false;
    }

    // --- Advance engine ---
    property real fallbackHold: 8
    Timer {
        id: advanceTimer
        repeat: false
        interval: Math.max(1, (pv.currentSlide && pv.currentSlide.holdTime > 0
                                ? pv.currentSlide.holdTime : pv.fallbackHold)) * 1000
        onTriggered: pv.next()
    }
    function _restartAdvance() {
        advanceTimer.stop();
        if (!pv._dismissing && pv.ambient && pv.visible && !pv.paused && pv.count > 1 && pv.currentSlide)
            advanceTimer.start();
    }
    onAmbientChanged: pv._restartAdvance()
    onVisibleChanged: pv._restartAdvance()
    onPausedChanged:  pv._restartAdvance()

    // Interactive keyboard navigation (← / →, PageUp/Down). Ambient ignores manual keys.
    Keys.onPressed: (e) => {
        if (pv.ambient) return;
        if (e.key === Qt.Key_Right || e.key === Qt.Key_PageDown) { pv.next(); e.accepted = true; }
        else if (e.key === Qt.Key_Left || e.key === Qt.Key_PageUp) { pv.prev(); e.accepted = true; }
    }
}
