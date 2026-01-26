// DsLoader.qml
import QtQuick 2.15
import Dsqt.Core

Loader {
    property alias url: editable.source

    DsEditableSource {
        id: editable
    }

    source: editable.source
}
