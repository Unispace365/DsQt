import QtQuick
import Dsqt.Core

Item {
    id: root

    property list<var> model
    property var config: ({fontFamily: defaultFont.font.family, fontSize: 16})
    property color highlightColor: "#00ADF7"
    property Component menuComponent: Component { DsQuickMenu {} }
    property bool rightClickEnabled: true
    property bool touchEnabled: true
    property real edgeMargin: 8
    property alias minimumTouchCount: clusterInput.minClusterTouchCount
    property alias minimumClusterSeparation: clusterInput.minClusterSeperation
    property alias clusterSize: clusterInput.boundingBoxSize
    property alias triggerTime: clusterInput.triggerTime
    readonly property Item mouseMenu: mouseHost.menu

    signal itemSelected(int index, point position)
    signal closeRequested()

    function closeMenus() { root.closeRequested(); }

    // Positions supplied to openAt are local to this host; selections are global.
    function openAt(position: point) {
        if (root.enabled && root.visible)
            mouseHost.openAt(position, false);
    }

    function boundedPosition(position: point, radius: real): point {
        const inset = Math.max(0, radius) + root.edgeMargin;
        return Qt.point(root.width < 2 * inset ? root.width / 2
                            : Math.max(inset, Math.min(root.width - inset, position.x)),
                        root.height < 2 * inset ? root.height / 2
                            : Math.max(inset, Math.min(root.height - inset, position.y)));
    }

    onEnabledChanged: if (!enabled) closeMenus()
    onVisibleChanged: if (!visible) closeMenus()
    onMenuComponentChanged: closeMenus()

    FontMetrics { id: defaultFont }

    DsClusterManager {
        id: clusterInput
        anchors.fill: parent
        enabled: root.touchEnabled
        minClusterTouchCount: 5
        minClusterSeperation: 300
        boundingBoxSize: 300
        triggerTime: 0.1
        holdOpenOnTouch: true
        doubleTapActivation: false
    }

    TapHandler {
        enabled: root.rightClickEnabled
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
        acceptedButtons: Qt.RightButton
        onTapped: point => root.openAt(point.position)
    }

    DsQuickMenuTouchHost {
        id: mouseHost
        clustered: false
        config: ({owner: root})
    }

    DsClusterView {
        anchors.fill: parent
        manager: clusterInput
        menuConfig: ({owner: root})
        delegate: DsQuickMenuTouchHost {}
    }
}
