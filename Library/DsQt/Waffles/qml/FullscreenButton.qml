import QtQuick

IconButton {
    iconSource: "file:///%APP%/data/images/waffles/window/fullscreen.svg"
    signal toggleFullscreen()
    onPressed: {
        toggleFullscreen();
        signalObject?.toggleFullscreen?.()
    }
}
