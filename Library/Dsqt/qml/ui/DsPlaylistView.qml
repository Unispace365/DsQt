pragma ComponentBehavior: Bound
import QtQuick 2.15

Item {
    id: root

    property alias nextItem: internal.nextItem
    property var transitionAnimation: ParallelAnimation {
        //OpacityAnimator {
        //    target: internal.current
        //    from: 1
        //    to: 0
        //    duration: 1000
        //}
        OpacityAnimator {
            target: internal.next
            from: 0
            to: 1
            duration: 1000
        }

    }

    anchors.fill:parent



    component Private:QtObject {
        property Holder current: aItem
        property Holder next:bItem
        property Component nextItem: null
        property var transitionAnimList: [
            root.transitionAnimation,
            switchScript
        ]

        onNextItemChanged: {
            if(nextItem){
                //console.log("NextItem changed")
            } else {
                //console.log("nextItem is null")
            }
        }
    }

    component Holder: Item {
        property Item chilli: null
    }

    Private {
        id: internal
    }

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
            internal.next.chilli?.destroy();
            internal.nextItem = null;
        }
    }

    Holder {
        id:aItem
        anchors.fill: root
        opacity:0
        chilli:null
        z:2
    }

    Holder {
        id:bItem
        anchors.fill: root
        opacity:0
        chilli:null
        z:1
    }

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
                    //console.log("next")
                    internal.next.z=1
                    internal.next.opacity=0;
                    internal.current.z=0
                    let incubator = internal.nextItem.incubateObject(internal.next,{})
                    if (incubator.status !== Component.Ready) {
                        incubator.onStatusChanged = function(status) {
                            if (status === Component.Ready) {
                                internal.next.chilli = incubator.object
                            }
                        };
                    } else {
                        internal.next.chilli = incubator.object
                        //print("Object", incubator.object, "is ready immediately!");
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
