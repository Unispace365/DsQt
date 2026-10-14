pragma ComponentBehavior: Bound
import QtQuick
import Dsqt.Core

// Adapts cluster lifetime to the replaceable visual's menu interface.
Item {
    id: root
    property list<var> model
    property var config
    property bool clustered: true
    readonly property var owner: config ? config.owner : null
    readonly property var menu: menuLoader.item
    property point requestedPosition
    property bool openingWithTouch: false

    function openAt(position: point, touch: bool) {
        if (!root.owner || !root.owner.enabled || !root.owner.visible)
            return;
        root.owner.closeMenus();
        menuLoader.active = false;
        root.requestedPosition = position;
        root.openingWithTouch = touch;
        menuLoader.active = true;
    }

    function closeMenu() {
        if (root.menu)
            root.menu.closeMenu();
    }

    // ClusterView's pooled delegate must begin each sequence below the threshold.
    Component.onCompleted: root.DsClusterView.minimumMet = false

    Loader {
        id: menuLoader
        active: false
        sourceComponent: root.owner ? root.owner.menuComponent : null
        readonly property point center: root.owner && root.menu
            ? root.owner.boundedPosition(root.requestedPosition, root.menu.outerRadius)
            : Qt.point(0, 0)
        readonly property point localCenter: root.owner
            ? root.mapFromItem(root.owner, center) : Qt.point(0, 0)
        x: localCenter.x
        y: localCenter.y
        onLoaded: {
            // Bind to the live host, rather than the cluster delegate's initial model snapshot.
            root.menu.model = Qt.binding(() => root.owner.model);
            root.menu.config = Qt.binding(() => root.owner.config);
            root.menu.highlightColor = Qt.binding(() => root.owner.highlightColor);
            root.menu.openMenu(root.openingWithTouch);
        }
    }

    Connections {
        target: root.owner
        function onCloseRequested() { root.closeMenu(); }
    }
    Connections {
        target: root.menu
        function onItemSelected(index, position) {
            if (index < 0 || index >= root.owner.model.length
                    || root.owner.model[index]?.available === false)
                return;
            root.owner.closeMenus();
            root.owner.itemSelected(index, position);
        }
        function onClosed() {
            menuLoader.active = false;
            if (root.clustered) {
                root.DsClusterView.minimumMet = false;
                root.DsClusterView.animateOffFinished();
            }
        }
    }

    DsClusterView.onMinimumMetChanged: {
        if (root.DsClusterView.minimumMet && root.owner)
            root.openAt(root.mapToItem(root.owner, 0, 0), true);
    }
    DsClusterView.onUpdated: point => {
        if (root.menu)
            root.menu.updatePoint(root.menu.mapFromItem(root, point));
    }
    DsClusterView.onReleased: {
        if (root.menu)
            root.menu.releaseGesture();
    }
    DsClusterView.onRemoved: root.closeMenu()
}
