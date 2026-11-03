pragma ComponentBehavior: Bound
import QtQuick
import Dsqt.Core
import Dsqt.Waffles

DsQuickMenuHost {
    id: root
    required property DsWaffleStage stage
    property alias actionIds: actions.actionIds
    property alias actionOverrides: actions.overrides
    property alias ambientAvailable: actions.ambientAvailable
    readonly property DsQuickMenuActions actionController: actions
    signal ambientRequested()

    model: actions.entries
    config: ({fontFamily: "Roboto", fontSize: 16})
    highlightColor: DsTheme.accent
    enabled: !stage.whiteboardShown && !stage.ambientShown
    visible: !stage.ambientShown

    DsQuickMenuActions {
        id: actions
        stage: root.stage
        onAmbientRequested: root.ambientRequested()
    }
    Connections {
        target: root
        function onItemSelected(index, position) { actions.trigger(index, position); }
    }
}
