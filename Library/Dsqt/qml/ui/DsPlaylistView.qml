pragma ComponentBehavior: Bound
import QtQuick 2.15
import Dsqt
Item {
    id: root

    property alias nextItem: internal.nextItem
    property alias nextContentModel: internal.nextContentModel
    property var transitionAnimation: PauseAnimation {
        duration: 0
    }
    state: "normal"
    anchors.fill:parent
    signal startTransition()

    //component definitions are private to this component
    //and not visible to the outside world.
    //these are so that QML knows what the types are

    // component to hold private properties
    component Private:QtObject {
        property Holder current: aItem
        property Holder next:bItem
        property Component nextItem: null
        property DsContentModel nextContentModel: null

        //combine the passed in animation with the switch script
        property var transitionAnimList: [
            root.transitionAnimation,
            switchScript
        ]

        onNextItemChanged: {
            if(nextItem){
                console.log("NextItem changed")
            } else {
                console.log("nextItem is null")
            }
        }
    }

    //a Holder is a holder for and instances of the playlist components
    component Holder: Item {
        property Item chilli: null
    }
    //end components

    //instance of our private properties
    Private {
        id: internal
    }

    //this script action will swap the current and next holders,
    //destroy the chilli item in the next holder,
    //and set the nextItem holder to null (which should move the state to "normal").
    //this should be done after any animations have completed
    ScriptAction {
        id: switchScript
        script: {
            if(internal.current == aItem){
                internal.current = bItem;
                internal.next = aItem;
            } else {
                internal.current = aItem;
                internal.next = bItem;
            }
            //console.log("deleting");
            //internal.next.chilli?.destroy();

        }
    }

    //holders for the created items. references to these get
    //swapped in the Private component
    Holder {
        id:aItem
        anchors.fill: root
        opacity:1
        chilli:null
        z:2
    }

    Holder {
        id:bItem
        anchors.fill: root
        opacity:1
        chilli:null
        z:1
    }

    //the normal state really just the "not animating" state
    //there is intentionally no Transition that covers going to the normal state.
    states: [
        State {
            name: "normal"
            when: internal.nextItem == null
        },

        State {
            name: "next"
            when: internal.nextItem != null
            StateChangeScript {
                name:"load"
                script: {
                    //set up for the animation
                    internal.next.z=1
                    //internal.next.opacity=0;
                    internal.current.z=0
                    root.startTransition()
                    if(internal.current.chilli){
                        root.startTransition.disconnect( internal.current.chilli?.runAnimateOff)
                    }
                    //set the state of the current item to the exit state if it exists
                    //this can trigger an "off" animation
                    //if(internal.current?.chilli?.exitState) {
                    //    internal.current.chilli.state = internal.current.chilli.exitState;
                    //}
                    let incubator = internal.nextItem.incubateObject(internal.next,{model:internal.nextContentModel})
                    if (incubator.status !== Component.Ready) {
                        incubator.onStatusChanged = function(status) {
                            if (status === Component.Ready) {
                                internal.next.chilli = incubator.object
                                root.startTransition.connect( internal.next.chilli.runAnimateOff)
                                Qt.callLater( function() {
                                    internal.nextItem = null;
                                })
                            }
                        }
                    } else {
                        internal.next.chilli = incubator.object
                        root.startTransition.connect( internal.next.chilli.runAnimateOff)
                        Qt.callLater( function() {
                            internal.nextItem = null;
                        })
                    }


                }
            }
        }
    ]

    transitions: [
        Transition {
            from: "*"
            to: "next"
            SequentialAnimation {
                animations: internal.transitionAnimList

            }
        }
    ]

}
