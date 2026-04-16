import QtQuick
import QtQuick.Controls

Item {
    default property alias content: container.children

    property real zoom: 7.07
    property real panX: -5053.14
    property real panY: -1250.14
    property real delta: 1.25
    property real defaultZoom: 1.0
    property real minZoom: 0.5
    property real maxZoom: 200
    property Item viewport: root
    property var easing: Easing.OutQuad

    signal tapped(var point, int button, int count)
    signal singleTapped(var point, int button)
    signal doubleTapped(var point, int button)
    signal zoomStarted
    signal panXStarted
    signal panYStarted
    signal zoomStopped
    signal panXStopped
    signal panYStopped

    id: root

    Shortcut {
        sequence: "z"
        autoRepeat: false
        context: Qt.ApplicationShortcut
        onActivated: console.log("PanAndZoom", root.panX, root.panY, root.zoom)
    }

    Shortcut {
        sequence: "Shift+Z"
        autoRepeat: false
        context: Qt.ApplicationShortcut
        onActivated: {
            root.animate = false
            root.animatedZoom = 7.07
            root.animatedPanX = -5053.14
            root.animatedPanY = -1250.14
        }
    }

    Item {
        id: container

        // Let the container size itself to its children
        width: childrenRect.width
        height: childrenRect.height
        x: childrenRect.x  // usually 0
        y: childrenRect.y  // usually 0

        transform: [
            Scale { xScale: root.animatedZoom; yScale: root.animatedZoom },
            Translate { x: root.animatedPanX; y: root.animatedPanY }
        ]
    }

    function computeGlobalScale() {
        return root.mapFromGlobal(1, 0).x - root.mapFromGlobal(0, 0).x
    }

    function stopAnimation() {
        if(root.animate){
            root.animate = false
            root.zoom = root.animatedZoom
            root.panX = root.animatedPanX
            root.panY = root.animatedPanY
        }
    }

    function startAnimation() {
        root.animate = true
    }

    function fitToView(animated) {
        if(root.viewport && container.width > 0 && container.height > 0) {
            const bounds = Qt.rect(container.x, container.y, container.width, container.height)
            fitToViewport(bounds, root.defaultZoom, animated)
        }
        else if(root.width > 0 && root.height > 0 && container.width > 0 && container.height > 0) {
            root.stopAnimation()
            const scale = 0.85 * Math.min( root.width / container.width, root.height / container.height )
            root.animate = animated
            root.zoom = Math.max(root.minZoom, Math.min(root.maxZoom, scale))
            root.panX = (root.width  - container.width * root.zoom) * 0.5
            root.panY = (root.height - container.height * root.zoom) * 0.5
        }
    }

    function fitToViewport(bounds, zoom, animated = true) {
        if(root.viewport && bounds) {
            root.stopAnimation()
            const tl = mapFromItem(root.viewport, 0, 0)
            const br = mapFromItem(root.viewport, root.viewport.width, root.viewport.height)
            const dst = Qt.rect(tl.x, tl.y, br.x - tl.x, br.y - tl.y)
            if(dst.width > 0 && dst.height > 0 && bounds.width > 0 && bounds.height > 0) {
                const scale = zoom * Math.min(dst.width / bounds.width, dst.height / bounds.height)
                const clampedScale = Math.max(root.minZoom, Math.min(root.maxZoom, scale))
                const targetX = dst.x + 0.5 * (dst.width - clampedScale * bounds.width)
                const targetY = dst.y + 0.5 * (dst.height - clampedScale * bounds.height)
                root.animate = animated
                root.zoom = clampedScale
                root.panX = targetX - root.zoom * bounds.x
                root.panY = targetY - root.zoom * bounds.y
            }
        }
    }

    Connections {
        target: container
        function onChildrenRectChanged() { root.fitToView(false) }
    }

    onWidthChanged: fitToView(false)
    onHeightChanged: fitToView(false)
    onViewportChanged: fitToView(false)

    property bool animate: true
    property int animatedDuration: 500
    property real animatedZoom: 1.0
    property real animatedPanX: 0.0
    property real animatedPanY: 0.0
    Behavior on animatedZoom {
        enabled: root.animate
        SequentialAnimation {
            ScriptAction{ script:root.zoomStarted() }
            NumberAnimation { duration: root.animatedDuration; easing.type: root.easing; }
            ScriptAction{ script:root.zoomStopped() }
        }
    }
    Behavior on animatedPanX {
        enabled: root.animate
        SequentialAnimation {
            ScriptAction{ script:root.panXStarted() }
            NumberAnimation { duration: root.animatedDuration; easing.type: root.easing; }
            ScriptAction{ script:root.panXStopped() }
        }
    }
    Behavior on animatedPanY {
        enabled: root.animate
        SequentialAnimation {
            ScriptAction{ script:root.panYStarted() }
            NumberAnimation { duration: root.animatedDuration; easing.type: root.easing; }
            ScriptAction{ script:root.panYStopped() }
        }
    }
    onZoomChanged: animatedZoom = zoom
    onPanXChanged: animatedPanX = panX
    onPanYChanged: animatedPanY = panY

    // ----- Cursor handling ---------------------------------------
    HoverHandler {
        id: hoverHandler
        parent: root.viewport
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        cursorShape: dragHandler.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
    }

    // ----- Zoom with wheel (centered on mouse) -------------------
    WheelHandler {
        id: wheelHandler
        enabled: hoverHandler.hovered

        onWheel: (wheel) => {
                     root.startAnimation()
                     var dx = (wheel.x - root.panX) / root.zoom
                     var dy = (wheel.y - root.panY) / root.zoom
                     var factor = wheel.angleDelta.y > 0 ? root.delta : 1.0 / root.delta
                     root.zoom = Math.max(root.minZoom, Math.min(root.maxZoom, root.zoom * factor))
                     root.panX = wheel.x - dx * root.zoom
                     root.panY = wheel.y - dy * root.zoom
                 }
    }

    // ----- Zoom by pinching (for touch screens) -------------------
    PinchHandler {
        id: pinchHandler
        target: null // Don't let it transform the item automatically
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        property bool skipFirst: false
        onActiveChanged: {
            if(active){
                skipFirst = true
            }
        }

        onScaleChanged: (delta) => {
                            if (active) {
                                root.stopAnimation()

                                // Calculate the zoom center point
                                var centerX = centroid.position.x
                                var centerY = centroid.position.y

                                // Calculate deltas from the initial state
                                var dx = (centerX - root.panX) / root.zoom
                                var dy = (centerY - root.panY) / root.zoom

                                // Calculate the new zoom level
                                var newZoom = root.zoom * delta
                                newZoom = Math.max(root.minZoom, Math.min(root.maxZoom, newZoom))

                                // Apply zoom and adjust pan to keep the pinch center fixed
                                root.zoom = newZoom
                                root.panX = centerX - dx * root.zoom
                                root.panY = centerY - dy * root.zoom
                            }
                        }

        onActiveTranslationChanged: (t) => {
                                  if (active) {
                                      if(skipFirst == true) {
                                        console.log("skipping pinch first")
                                        skipFirst = false
                                        return
                                      }
                                      root.stopAnimation()

                                      let s = root.computeGlobalScale()
                                      root.panX += t.x * s
                                      root.panY += t.y * s
                                  }
                              }
    }

    // ----- Panning with left-drag --------------------------------
    DragHandler {
        id: dragHandler
        target: null // Don't let it transform the item automatically
        enabled: hoverHandler.hovered || dragHandler.active
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedButtons: Qt.LeftButton
        grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
        maximumPointCount: 1

        onTranslationChanged: (translation) => {

                                  if (active) {
                                      root.stopAnimation()

                                      let s = root.computeGlobalScale()
                                      console.log("translate 1 "+s)
                                      root.panX += translation.x * s
                                      root.panY += translation.y * s
                                  }

                              }
    }

    DragHandler {
        id: touchDragHandler
        target: null // Don't let it transform the item automatically
        acceptedDevices: PointerDevice.TouchScreen
        acceptedButtons: Qt.NoButton
        //grabPermissions: PointerHandler.ApprovesTakeOverByHandlersOfSameType
         grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
        maximumPointCount: 1
        property bool skipFirst: false;
        onActiveChanged: {
            if(active){
                 skipFirst = true;
           }
        }

        onActiveTranslationChanged: (t) => {
              if (active) {
                  if(skipFirst == true) {
                    console.log("skipping drag first")
                    skipFirst = false
                    return
                  }

                  root.stopAnimation()

                  let s = root.computeGlobalScale()
                  console.log("translate 2 "+t.x+","+t.y)
                  root.panX += t.x * s
                  root.panY += t.y * s
              }

          }
    }

    // ----- Handle taps --------------------------------------------
    TapHandler {
        id: tapHandler
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        enabled: hoverHandler.hovered
        exclusiveSignals: TapHandler.SingleTap | TapHandler.DoubleTap
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        grabPermissions: PointerHandler.ApprovesTakeOverByHandlersOfSameType

        onTapped: (point, button) => {
                      point.accepted = true
                      root.tapped(container.mapFromGlobal(point.globalPosition), button, tapCount)
                  }

        onSingleTapped: (point, button) => {
                            root.singleTapped(container.mapFromGlobal(point.globalPosition), button)
                        }

        onDoubleTapped: (point, button) => {
                            root.doubleTapped(container.mapFromGlobal(point.globalPosition), button)
                        }
    }

    TapHandler {
        id: touchTapHandler
        parent: root.viewport
        acceptedDevices: PointerDevice.TouchScreen
        acceptedButtons: Qt.NoButton
        exclusiveSignals: TapHandler.SingleTap | TapHandler.DoubleTap
        gesturePolicy: TapHandler.DragThreshold
        grabPermissions: PointerHandler.ApprovesTakeOverByHandlersOfSameType
        dragThreshold: 10

        onTapped: (point, button) => {
                      point.accepted = true
                      root.tapped(container.mapFromGlobal(point.globalPosition), button, tapCount)
                  }

        onSingleTapped: (point, button) => {
                            root.singleTapped(container.mapFromGlobal(point.globalPosition), button)
                        }

        onDoubleTapped: (point, button) => {
                            root.doubleTapped(container.mapFromGlobal(point.globalPosition), button)
                        }
    }
}
