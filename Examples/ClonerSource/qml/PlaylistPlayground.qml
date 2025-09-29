import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dsqt

Item {
id: root

DsSettingsProxy {
    id:appProxy
    target:"app_settings"
}

BaseTemplate {

}

DsPlaylistView {
    id:playlist
    anchors.fill: root
    nextItem: playlistControl.currentTemplateComp
    nextContentModel: playlistControl.currentModelItem
    DsPlaylistControl {
        id: playlistControl
        templateMap:appProxy.getObj("playlist.templateMap")
        model:Ds.getRecordById(Ds.getRecordById(contentEvents.current?.uid)?.ambient_playlist)
        interval: 10000
        active: Ds.engine.idle.idling
        onCurrentItemChanged: {
            playlist.nextContentModel = playlistControl.currentModelItem;
            playlist.nextItem = playlistControl.currentTemplateComp;
        }

        Connections {
            target: Ds.engine
            function onRootUpdated() {
                //console.log("root updated in qml")
                playlistControl.model = Ds.getRecordById(Ds.getRecordById(contentEvents.current?.uid)?.ambient_playlist)
                playlistControl.updateModel(true)
                //console.log(JSON.stringify(playlistControl.model,null,2))
            }
        }

    }

}

DsClock {
    id: clock
    // interval: 50
    // speed: 1800
    anchors.left: parent.left
    anchors.leftMargin: 20
    anchors.top: parent.top
    anchors.topMargin: 20
    width: 150
    height: 150
    secondHandColor: "#00000000"
}

DsEventSchedule {
    id: contentEvents
    clock: clock
}




}
