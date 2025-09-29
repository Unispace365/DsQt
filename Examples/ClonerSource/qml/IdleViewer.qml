import QtQuick
import Dsqt
Item {

    id: root
    anchors.fill: parent
    property bool idle:DS.engine.idle.idling
    Item {
        id: attractElement

        Text {
            id:idleText
            property real deltax:1
            property real deltay:-1
            property real speedx:10
            property real speedy:5
            text:"Idle"
            font.pixelSize: 50
        }
        FrameAnimation {
            id: animator
            running: root.idle
            onTriggered: {
                let x = idleText.x
                let y = idleText.y
                let deltax = idleText.deltax
                let deltay = idleText.deltay
                let speedx = idleText.speedx
                let speedy = idleText.speedy

                x = x + deltax * speedx
                y = y + deltay * speedy
                if(x<0) {
                    x = 0
                    deltax=1
                }
                if(x+idleText.width>root.width) {
                    x = root.width - idleText.width
                    deltax=-1
                }
                if(y<0) {
                    y = 0
                    deltay=1
                }
                if(y+idleText.height>root.height) {
                    y = root.height - idleText.height
                    deltay=-1
                }

                idleText.x = x
                idleText.y = y
                idleText.deltax= deltax
                idleText.deltay = deltay
            }
        }
    }
    Item {
        id: preventer
        anchors.fill: root
        property bool selected: false
        Rectangle {
            id: rect1
            anchors.horizontalCenter: preventer.horizontalCenter
            anchors.verticalCenter: preventer.verticalCenter
            width:300
            height: 100
            color: preventer.selected ? "red" : "white"
            border.color: "black"
            border.width: 5
            TapHandler {
                target: rect1
                onTapped: {
                    preventer.selected = !preventer.selected
                    if(!preventer.selected){
                        DS.engine.idle.startIdling()
                    }
                }
            }

        }
        DsIdlePreventer {
            id: idlePreventer
            targetIdle: DS.engine.idle
            preventIdle: preventer.selected
        }
    }
}
