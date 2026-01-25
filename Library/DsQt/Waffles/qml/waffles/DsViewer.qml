import QtQuick

Item {
    enum ViewerType {
        TitledMediaViewer,
        Launcher,
        PresentationController,
        FullscreenController,
        Unknown
    }
    property var signalObject: null
    property int viewerType: DsViewer.ViewerType.Unknown
    property int viewerWidth: 400
    property int viewerHeight: 300
}
