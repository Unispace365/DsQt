import QtQuick 2.15

Item {
    id: viewer

    // Preferred size of the content in pixels
    property real preferredWidth: 1920 // Default example value; set as needed
    property real preferredHeight: 1080 // Default example value; set as needed

    // Current user-controlled scale (1.0 = 100%)
    property real userScale: 1.0

    // Whether view fitting is enabled
    property bool fitEnabled: true

    // Whether original size is enabled
    property bool fitOriginal: false

    // Whether centering is enabled
    property bool fitCentered: true

    // Allow children to be declared inside this component and forward them to the content wrapper
    default property alias contentData: contentWrapper.data

    //
    // property alias cursorShape: mouse.cursorShape

    // Enable keyboard focus
    focus: true

    // Handle key presses
    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Shift) {
                            mouse.enabled = true
                            event.accepted = true
                        }
                    }

    Keys.onReleased: (event) => {
                         if (event.key === Qt.Key_Shift) {
                             mouse.enabled = false
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
        id: mouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        //hoverEnabled: true
        enabled: false

        // Change cursor based on mode
        cursorShape: enabled ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor

        property point lastPos

        onPressed: (mouse) => {
                       if(mouse.button === Qt.RightButton) {
                           fitToScreen()
                           mouse.accepted = true
                       }
                       else if (mouse.button === Qt.LeftButton) {
                           lastPos = Qt.point(mouse.x, mouse.y)
                           mouse.accepted = true
                       }
                   }

        onPositionChanged: (mouse) => {
                               if (mouse.buttons & Qt.LeftButton) {
                                   var dx = mouse.x - lastPos.x
                                   var dy = mouse.y - lastPos.y
                                   contentWrapper.x += dx
                                   contentWrapper.y += dy
                                   lastPos = Qt.point(mouse.x, mouse.y)
                               }
                           }

        onWheel: (wheel) => {
                     // Determine zoom factor (adjust as needed for sensitivity)
                     var factor = (wheel.angleDelta.y > 0) ? 1.1 : 1 / 1.1
                     zoomAt(wheel.x, wheel.y, factor)
                     wheel.accepted = true
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
        if(fitEnabled) {
            if(fitOriginal) {
                userScale = 1.0
                contentWrapper.x = fitCentered ? (width - preferredWidth) / 2 : 0
                contentWrapper.y = fitCentered ? (height - preferredHeight) / 2 : 0
                contentWrapper.width = preferredWidth
                contentWrapper.height = preferredHeight
            }
            else {
                var fitScale = Math.min(width / preferredWidth, height / preferredHeight)
                userScale = fitScale

                // Center the content
                var displayedWidth = preferredWidth * fitScale
                var displayedHeight = preferredHeight * fitScale
                contentWrapper.x = fitCentered ? (width - displayedWidth) / 2 : 0
                contentWrapper.y = fitCentered ? (height - displayedHeight) / 2 : 0
                contentWrapper.width = preferredWidth
                contentWrapper.height = preferredHeight
            }
        } else {
            contentWrapper.x = x
            contentWrapper.y = y
            contentWrapper.width = width
            contentWrapper.height = height
            userScale = 1.0
        }
    }

    // Refit when properties change.
    onFitEnabledChanged: fitToScreen()
    onFitOriginalChanged: fitToScreen()
    onFitCenteredChanged: fitToScreen()

    // Refit when size changes.
    onWidthChanged: fitToScreen()
    onHeightChanged: fitToScreen()

    // Refit when preferred size changes (e.g., set at runtime)
    onPreferredWidthChanged: fitToScreen()
    onPreferredHeightChanged: fitToScreen()

    // Initially fit to screen
    Component.onCompleted: fitToScreen()
}
