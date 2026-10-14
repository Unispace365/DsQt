pragma ComponentBehavior: Bound
import QtQuick
import Dsqt.Waffles

QtObject {
    id: root
    required property DsWaffleStage stage
    property bool ambientAvailable: false
    property list<string> actionIds: [
        "content", "whiteboard", "forward", "arrange", "close_assets",
        "gather", "back", "asset_browsing", "ambient"
    ]
    // Each ID may override iconText, iconSource/icon/iconPath, available and trigger(position).
    property var overrides: ({})
    readonly property var entries: actionIds.map(action => root.entry(action))
    signal ambientRequested()

    function entry(action: string): var {
        const labels = {
            content: qsTr("Content"), whiteboard: qsTr("Whiteboard"), forward: qsTr("Forward"),
            arrange: qsTr("Arrange"), close_assets: qsTr("Close Assets"), gather: qsTr("Gather"),
            back: qsTr("Back"), asset_browsing: qsTr("Asset Browsing"), ambient: qsTr("Ambient")
        };
        const custom = root.overrides[action] || {};
        const available = custom.available !== undefined ? custom.available : root.isAvailable(action);
        const standard = labels[action] !== undefined;
        const suffix = !available && action !== "whiteboard" ? "_disabled" : "";
        return Object.assign({
            action: action, iconText: labels[action] || action, available: available,
            icon: standard ? action + suffix + ".svg" : "",
            iconPath: "qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/quick_menu/"
        }, custom, {action: action});
    }

    function isAvailable(action: string): bool {
        switch (action) {
        case "content":
        case "asset_browsing": return root.stage.activeLauncher !== null;
        case "whiteboard": return root.stage.whiteboard !== null;
        case "forward":
        case "back": return root.stage.canNavigate;
        case "arrange":
        case "gather": return root.stage.hasMedia && !root.stage.mediaLayoutPending;
        case "close_assets": return root.stage.hasMedia;
        case "ambient": return root.ambientAvailable;
        default: return typeof root.overrides[action]?.trigger === "function";
        }
    }

    // Callbacks and selection signals receive global coordinates.
    function trigger(index: int, globalPosition: point) {
        const entry = root.entries[index];
        if (!entry || entry.available === false)
            return;
        if (typeof entry.trigger === "function") {
            entry.trigger(globalPosition);
            return;
        }
        const position = root.stage.mapFromGlobal(globalPosition);
        switch (entry.action) {
        case "content": root.stage.showLauncher("content", position); break;
        case "asset_browsing": root.stage.showLauncher("search", position); break;
        case "whiteboard": root.stage.openWhiteboard(); break;
        case "forward": root.stage.nextSlide(); break;
        case "back": root.stage.previousSlide(); break;
        case "arrange": root.stage.arrangeMedia(); break;
        case "gather": root.stage.gatherMedia(position); break;
        case "close_assets": root.stage.closeMedia(); break;
        case "ambient": root.ambientRequested(); break;
        }
    }
}
