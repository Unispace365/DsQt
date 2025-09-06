import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

    // Expose source property like a standard Loader.
    property url source: ""
    // Expose sourceComponent property like a standard Loader.
    property Component sourceComponent
    // Expose model property for content model.
    property var model: []
    // (Optional) Set fade duration in milliseconds.
    property int fadeDuration: 500

    onSourceChanged: {
        if(a.z === 1) {
            a.source = ""
            a.source = source
            a.opacity = 0
            a. visible = true
        } else {
            b.source = ""
            b.source = source
            b.opacity = 0
            b.visible = true
        }
    }

    onModelChanged: {
        if(model && model.length !== 0) {
            // Set new source and model to the top-most loader.
            if( a.z === 1 ) {
                if(sourceComponent) {
                    a.sourceComponent = null
                    a.sourceComponent = sourceComponent
                }
                else {
                    a.source = ""
                    a.source = source
                }
                a.opacity = 0
                a. visible = true
            } else {
                if(sourceComponent) {
                    b.sourceComponent = null
                    b.sourceComponent = sourceComponent
                }
                else {
                    b.source = ""
                    b.source = source
                }
                b.opacity = 0
                b.visible = true
            }
        }
    }

    Loader {
        id: a
        asynchronous: true
        anchors.fill: parent
        visible: false // Initially invisible
        z: 0 // Initially at the bottom

        Connections {
            target: a.item
            function onModelChanged() {
                faderA.start()
            }
        }

        onLoaded: {
            item.model = model
        }
    }

    Loader {
        id: b
        asynchronous: true
        anchors.fill: parent
        visible: false // Initially invisible
        z: 1 // Initially at the top

        Connections {
            target: b.item
            function onModelChanged() {
                faderB.start()
            }
        }

        onLoaded: {
            item.model = model
        }
    }

    NumberAnimation {
        id: faderA
        target: a
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: root.fadeDuration
        easing.type: Easing.InQuad

        onStopped: {
            b.visible = false
            b.z = 1
            a.z = 0
        }
    }

    NumberAnimation {
        id: faderB
        target: b
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: root.fadeDuration
        easing.type: Easing.InQuad

        onStopped: {
            a.visible = false
            a.z = 1
            b.z = 0
        }
    }
}
