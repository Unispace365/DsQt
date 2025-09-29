import QtQuick
import QtQuick.Shapes
import Dsqt

// Serves as the base for all elements displaying CMS content.
Item {
    anchors.fill: parent

    // Content model from the CMS.
    property var model: ({})

    // Can be used to signal the element is ready for display.
    signal contentReady()
}
