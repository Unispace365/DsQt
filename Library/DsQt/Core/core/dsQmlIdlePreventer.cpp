#include "core/dsQmlIdlePreventer.h"
#include "core/dsQmlIdle.h"

namespace dsqt {
DsQmlIdlePreventer::DsQmlIdlePreventer(QObject* parent):QObject(parent),mPreventionTimer(new QTimer(this)) {
    connect(mPreventionTimer,&QTimer::timeout,this,[this](){
        setPreventIdle(false);
    });
}

void DsQmlIdlePreventer::preventIdleForMilliseconds(long milliseconds)
{
    if(milliseconds<=0) return;
    if(mPreventionTimer->isActive()){
        mPreventionTimer->stop();
    }
    setPreventIdle(true);
    mPreventionTimer->setSingleShot(true);
    mPreventionTimer->start(milliseconds);
}

bool DsQmlIdlePreventer::preventIdle() const
{
    return mPreventIdle;
}

void DsQmlIdlePreventer::setPreventIdle(bool newPreventIdle)
{
    if (mPreventIdle == newPreventIdle)
        return;
    mPreventIdle = newPreventIdle;
    emit preventIdleChanged(mPreventIdle);
}

DsQmlIdle *DsQmlIdlePreventer::targetIdle() const
{
    return mTargetIdle;
}

void DsQmlIdlePreventer::setTargetIdle(DsQmlIdle *newTargetIdle)
{
    if (mTargetIdle == newTargetIdle)
        return;
    if(mTargetIdle){
        mTargetIdle->clearIdlePreventer(this);
    }
    mTargetIdle = newTargetIdle;
    mTargetIdle->addIdlePreventer(this);

    emit targetIdleChanged();
}
}//namespace dsqt
