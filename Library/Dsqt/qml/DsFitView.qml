import QtQuick 2.15

Item {
    id: viewer

    // Preferred size of the content in pixels
    property real preferredWidth: 1920 // Default example value; set as needed
    property real preferredHeight: 1080 // Default example value; set as needed

    // Current user-controlled scale (1.0 = 100%)
    property real userScale: 1.0
    property real originalScale: 1.0

    // Mode toggled by holding SHIFT
    property bool mouseEnabled: false

    // Allow children to be declared inside this component and forward them to the content wrapper
    default property alias contentData: contentWrapper.data

    // Enable keyboard focus
    focus: true

    // Handle key presses
    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Shift) {
                            mouseEnabled = true
                            event.accepted = true
                        } else if (event.key === Qt.Key_0) {
                            userScale = 1.0
                            contentWrapper.x = (Window.width - preferredWidth) / 2
                            contentWrapper.y = (Window.height - preferredHeight) / 2
                            event.accepted = true
                        }
                    }

    Keys.onReleased: (event) => {
                         if (event.key === Qt.Key_Shift) {
                             mouseEnabled = false
                             event.accepted = true
                         }
                     }

    // The wrapper for content, which is scaled and translated
    Item {
        id: contentWrapper
        width: preferredWidth
        height: preferredHeight
        transformOrigin: Item.TopLeft
        scale: userScale
    }

    // Mouse area for handling wheel, hover, and drag
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true

        // Change cursor based on mode
        cursorShape: mouseEnabled ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor

        property point lastPos

        onPressed: (mouse) => {
                       if(mouseEnabled){
                           if(mouse.button === Qt.RightButton) {
                               fitToScreen()
                               mouse.accepted = true
                           }
                           else if (mouse.button === Qt.LeftButton) {
                               lastPos = Qt.point(mouse.x, mouse.y)
                               mouse.accepted = true
                           }
                       }
                   }

        onPositionChanged: (mouse) => {
                               if (mouseEnabled && pressedButtons & Qt.LeftButton) {
                                   var dx = mouse.x - lastPos.x
                                   var dy = mouse.y - lastPos.y
                                   contentWrapper.x += dx
                                   contentWrapper.y += dy
                                   lastPos = Qt.point(mouse.x, mouse.y)
                               }
                           }

        onWheel: (wheel) => {
                     if (mouseEnabled) {
                         // Determine zoom factor (adjust as needed for sensitivity)
                         var factor = (wheel.angleDelta.y > 0) ? 1.1 : 1 / 1.1
                         zoomAt(wheel.x, wheel.y, factor)
                         wheel.accepted = true
                     }
                 }
    }

    // Function to zoom around a specific point (mouse position)
    function zoomAt(mx, my, factor) {
        var oldScale = userScale
        var newScale = oldScale * factor

        // Calculate local position in content coordinates
        var localX = (mx - contentWrapper.x) / oldScale
        var localY = (my - contentWrapper.y) / oldScale

        // Update scale and adjust position to keep the point fixed
        userScale = newScale
        contentWrapper.x = mx - localX * newScale
        contentWrapper.y = my - localY * newScale
    }

    // Function to fit content to the view (letterboxed)
    function fitToScreen() {
        var fitScale = Math.min(width / preferredWidth, height / preferredHeight)
        userScale = fitScale

        // Center the content
        var displayedWidth = preferredWidth * fitScale
        var displayedHeight = preferredHeight * fitScale
        contentWrapper.x = (width - displayedWidth) / 2
        contentWrapper.y = (height - displayedHeight) / 2
    }

    // Refit when size changes.
    onWidthChanged: fitToScreen()
    onHeightChanged: fitToScreen()

    // Refit when preferred size changes (e.g., set at runtime)
    onPreferredWidthChanged: fitToScreen()
    onPreferredHeightChanged: fitToScreen()

    // Initially fit to screen
    Component.onCompleted: fitToScreen()
}
