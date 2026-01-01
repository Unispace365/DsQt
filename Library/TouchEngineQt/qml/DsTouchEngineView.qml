import QtQuick
import TouchEngineQt

/*
 * TouchEngineView - QML wrapper component
 * This provides additional QML-friendly features on top of the C++ TouchEngineView
 */
Item {
    id: root
    
    // Re-expose properties with additional behaviors if needed
    property alias instanceId: view.instanceId
    property alias outputLink: view.outputLink
    property alias autoUpdate: view.autoUpdate
    
    // Forward request frame method
    function requestFrame() {
        view.requestFrame()
    }
    
    // The actual C++ backed view
    DsTouchEngineOutputView {
        id: view
        anchors.fill: parent
    }
    
    // Optional: Add visual feedback
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: view.autoUpdate ? "#4CAF50" : "#757575"
        border.width: 1
        opacity: 0.3
    }
}
