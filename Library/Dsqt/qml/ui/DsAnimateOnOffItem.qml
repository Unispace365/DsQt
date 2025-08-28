import QtQuick

Item {
    required property Animation animateOnAnimation
    required property Animation animateOffAnimation
    Component.onCompleted: {
        runAnimateOn()
    }

    function runAnimateOn() {
        console.log("animate on");
        animateOn();
        if(animateOnAnimation) {
            animateOnAnimation.start();
        }
    }

    function runAnimateOff() {
        console.log("animate off");
        animateOff();
        if(animateOffAnimation) {
            animateOffAnimation.start();
        }
    }

    signal animateOn()
    signal animateOff()
}
