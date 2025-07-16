import QtQuick 2.15

Item {
    enum ViewerType {
        TitledMediaViewer,
        Launcher,
        PresentationController,
        FullscreenController,
        Unknown
    }
    property int viewerType: Viewer.ViewerType.Unknown
}
