import QtQuick 2.15

Item {
    id: root
    property Image icon
    property string title
    property var items
    property Component delegate
    Repeater {
        model: items
        delegate: delegate
    }
}
