#include "dsQmlIdle.h"
#include "dsQmlIdlePreventer.h"
#include <qguiapplication.h>
#include <QPointerEvent>
#include <QQmlListProperty>
Q_LOGGING_CATEGORY(lgIdle, "idle")
Q_LOGGING_CATEGORY(lgIdleVerbose, "idle.verbose")
namespace dsqt {
DsQmlIdle::DsQmlIdle(QObject* parent)
    : QObject(parent),mIdleTimer(new QTimer(this)) {
    mIdleTimer->setSingleShot(true);
    connect(mIdleTimer,&QTimer::timeout,this,[this]() {
        this->startIdling();
    });


}

void DsQmlIdle::startIdling(bool force)
{
    qCInfo(lgIdle)<<"Maybe starting idle";
    if(mIdling && !force) return;
    mIdleTimer->stop();
    setIdling(true);
    emit idleStarted();
}

void DsQmlIdle::stopIdling(bool force)
{
    qCInfo(lgIdle)<<"Stopping Idle";
    if(!mIdling && !force) return;
    mWouldHaveGoneToIdle = false;
    setIdling(false);
    emit idleEnded();
    mIdleTimer->stop();
    mIdleTimer->start(mIdleTimeout);
}

void DsQmlIdle::addIdlePreventer(DsQmlIdlePreventer *preventer)
{
    if(preventer == nullptr || mIdlePreventers.contains(preventer)){
        return;
    }

    connect(preventer,&DsQmlIdlePreventer::preventIdleChanged,this,&DsQmlIdle::preventionChanged);
    connect(preventer,&DsQmlIdlePreventer::destroyed,this,[this,preventer](){clearIdlePreventer(preventer);});
    mIdlePreventers.insert(preventer);
}

void DsQmlIdle::clearIdlePreventer(DsQmlIdlePreventer *preventer)
{
    if(preventer == nullptr){
        return;
    }
    mIdlePreventers.remove(preventer);
}

void DsQmlIdle::clearAllIdlePreventers()
{
    mIdlePreventers.clear();
}


void DsQmlIdle::preventionChanged(bool preventIdle)
{
    //if preventIdle is true, then we don't care because
    //we are either already idleing and we can't prevent it
    //or we arent idleing yet so nothing to prevent.
    //we could possibly care if we wanted IdlePreventers to
    //drop us out of Idle
    if(preventIdle == false) {
        if(mWouldHaveGoneToIdle){
            startIdling();
        }
    }
}

QQmlListProperty<DsQmlIdlePreventer> DsQmlIdle::idlePreventers()
{
    auto list = QList<DsQmlIdlePreventer*>(mIdlePreventers.begin(),mIdlePreventers.end());
    return QQmlListProperty<DsQmlIdlePreventer>(this,&list);
}

bool DsQmlIdle::idling() const
{
    return mIdling;
}

void DsQmlIdle::setIdling(bool newIdling)
{
    if (mIdling == newIdling){
        return;
    }

    bool preventIdle = false;
    if(newIdling == true){
        for(auto preventer:std::as_const(mIdlePreventers)){
            if(preventer->preventIdle()) {
                preventIdle = true;
            }
        }

        if(preventIdle == true){
            qCInfo(lgIdle)<<"Prevented from going to idle";
            mWouldHaveGoneToIdle = true;
            return;
        }
        qCInfo(lgIdle)<<"Starting Idling";
    }

    mIdling = newIdling;
    emit idlingChanged();
}

int DsQmlIdle::idleTimeout() const
{
    return mIdleTimeout;
}

void DsQmlIdle::setIdleTimeout(int newIdleTimeout)
{
    if (mIdleTimeout == newIdleTimeout)
        return;
    mIdleTimeout = newIdleTimeout;
    emit idleTimeoutChanged();
}

bool DsQmlIdle::eventFilter(QObject *watched, QEvent *ev)
{
    if(ev->isPointerEvent()){
        if(!mIdling){
            stopIdling(true);
        }
        auto mouseEv = dynamic_cast<QMouseEvent*>(ev);
        if(mouseEv && mouseEv->button() != Qt::NoButton && mouseEv->isBeginEvent()) {
            // mouse button pressed, stop idling
            stopIdling();
            return false;
        }
        auto touchEv = dynamic_cast<QTouchEvent*>(ev);
        if(touchEv && (touchEv->touchPointStates()&Qt::TouchPointPressed) != 0 && touchEv->isBeginEvent()){
            stopIdling();
            return false;
        }

    }
    return false;
}

QObject *DsQmlIdle::areaItemTarget() const
{
    return mAreaItemTarget;
}

void DsQmlIdle::setAreaItemTarget(QObject *newIdleTarget)
{
    if (mAreaItemTarget == newIdleTarget)
        return;
    if(mAreaItemTarget){
        mAreaItemTarget->removeEventFilter(this);
    }
    mAreaItemTarget = newIdleTarget;
    mAreaItemTarget->installEventFilter(this);
    emit areaItemTargetChanged();
}
} //namespace dsqt
