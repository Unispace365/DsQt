import QtQuick
import Dsqt

DsAnimateOnOffItem  {
    id: root
    property DsContentModel model: null
    //setting this to tru will cause this item to attempt to destroy it's self
    //when animated off. This is useful for templates that are only needed
    //for dynamically created items

    property bool destroyAfterOff: true

    animateOnAnimation: NumberAnimation {
        target: root
        properties: "opacity"
        to: 1
        duration: 250
    }

    animateOffAnimation: NumberAnimation {
        target: root
        properties: "opacity"
        to: 0
        duration: 250
        onFinished: {
            if(root.destroyAfterOff){
                console.log("self destroying")
                root.destroy()
            }
        }
    }
    opacity:0



}
