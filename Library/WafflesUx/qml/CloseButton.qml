import QtQuick

IconButton {
    iconSource: "file:///%APP%/data/images/waffles/window/close.svg"
    signal close()
    onPressed: {
        close()
        signalObject?.close?.()
    }
}
