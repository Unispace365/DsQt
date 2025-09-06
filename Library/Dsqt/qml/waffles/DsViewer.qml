import QtQuick

Item {
    enum ViewerType {
        TitledMediaViewer,
        Launcher,
        PresentationController,
        FullscreenController,
        Unknown
    }
    property int viewerType: DsViewer.ViewerType.Unknown
}
