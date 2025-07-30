#include "dsQmlClusterManager.h"
#include "glm/gtx/rotate_vector.hpp"
#include "glm/gtx/scalar_multiplication.hpp"
#include <QPointerEvent>
namespace dsqt::ui {
DsQmlClusterManager::DsQmlClusterManager(QQuickItem* parent):QQuickItem(parent) {
    this->setAcceptTouchEvents(true);
    this->setAcceptedMouseButtons(Qt::MouseButton::AllButtons);
    mClusters = new DsQmlTouchClusterList(this);
}

uint64_t DsQmlClusterManager::mMaxClusterId = 1;

bool DsQmlClusterManager::event(QEvent *event)
{

    //qDebug()<<event->type();
    auto touchevent = static_cast<QTouchEvent*>(event);
    auto mouseevent = static_cast<QMouseEvent*>(event);
    auto keyevent = static_cast<QKeyEvent*>(event);
    int mouseoffset = 1;

    bool mousePressed = false, mouseMoved = false, mouseReleased = false;
    auto mouseState = QEventPoint::Unknown;
    switch(event->type()){
    //these mouse cases drop through
    case QEvent::MouseButtonPress:
        mouseevent->addPassiveGrabber(mouseevent->point(0),this);
        mousePressed = true;
        mouseState = QEventPoint::Pressed;
    case QEvent::MouseMove:
        if(!mousePressed && (mouseevent->modifiers() & Qt::ShiftModifier)){
            mouseMoved = true;
            mouseState = QEventPoint::Updated;
        }
    case QEvent::MouseButtonRelease:
        if(!mousePressed && !mouseMoved){
            mouseReleased = true;
            mouseState = QEventPoint::Released;
        }

        if((mouseevent->modifiers() & Qt::ShiftModifier) || (mouseReleased&&mClusters->rowCount()>0)){
            float angle = 2*glm::pi<float>()/m_minClusterTouchCount;
            glm::vec2 pos(mouseevent->pos().x(),mouseevent->pos().y());
            for(int i=0;i<m_minClusterTouchCount;i++){
                TouchInfo ti;

                glm::vec2 unit(0,1);
                unit = glm::rotate(unit,angle*i);
                unit = unit * 50;
                auto nPoint = pos + unit;

                ti.mPoint = QPointF(nPoint.x,nPoint.y);
                ti.mFingerId = i + 100;
                ti.mState = mouseState;
                parseTouch(ti);
            }
        }
        break;
    case QEvent::TouchBegin:
        for(auto& point:touchevent->points()){
             touchevent->addPassiveGrabber(point,this);
            TouchInfo ti;
            ti.mPoint = point.position();
            ti.mFingerId = point.id()+mouseoffset;
            ti.mState = point.state();
            parseTouch(ti);
            //qDebug()<<"touch begin - "<<point.id()+mouseoffset;
        }


        break;
    case QEvent::TouchUpdate:

        for(auto& point:touchevent->points()){
            TouchInfo ti;
            ti.mPoint = point.position();
            ti.mFingerId = point.id()+mouseoffset;
            ti.mState = point.state();
            if(point.state() == QEventPoint::State::Pressed){
                touchevent->addPassiveGrabber(point,this);
                 parseTouch(ti);
                //qDebug()<<"touch begin -"<<point.id();
            } else
            if(point.state() == QEventPoint::State::Updated){
                 parseTouch(ti);
                //qDebug()<<"touch updated -"<<point.id()<<nextId++;
            } else
            if(point.state() == QEventPoint::State::Released){
                parseTouch(ti);

            } else if(point.state() == QEventPoint::State::Unknown){

            }

        }
        break;
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        for(auto& point:touchevent->points()){
            TouchInfo ti;
            ti.mPoint = point.position();
            ti.mFingerId = point.id()+mouseoffset;
            ti.mState = QEventPoint::State::Released;
            if(point.state() == QEventPoint::State::Released){
                parseTouch(ti);

            }
            if(point.state() == QEventPoint::State::Unknown){
                parseTouch(ti);

            }

        }
        break;
    default:
        break;

    }

    return QQuickItem::event(event);
}

DsQmlTouchCluster* DsQmlClusterManager::closeToCluster(const QPointF& in_point)
{
    QVector2D point(in_point);
    float deltaX(0), deltaY(0);
    for (int i = 0; i < mClusters->rowCount(); i++) {
        DsQmlTouchCluster* c = mClusters->cluster(i);
        for (int t = 0; t < c->mTouches.size(); t++) {
            TouchInfo& ti = c->mTouches[t];
            deltaX				  = ti.mPoint.x() - in_point.x();
            deltaY				  = ti.mPoint.y() - in_point.y();
            float distSquared	  = deltaX * deltaX + deltaY * deltaY;
            if (distSquared < (m_minClusterSeparation * m_minClusterSeparation)) {
                return c;
            }
        }
    }

    return nullptr;

}

DsQmlTouchCluster* DsQmlClusterManager::findCluster(int fingerId, const QPointF& point){
    for (int i = 0; i < mClusters->rowCount(); i++) {
        DsQmlTouchCluster* c = mClusters->cluster(i);
        for (auto it = c->mTouches.begin(); it < c->mTouches.end(); ++it) {
            if ((*it).mFingerId == fingerId) {
                (*it).mPoint = point;
                return c;
            }
        }
    }

    return nullptr;
}

void DsQmlClusterManager::parseTouch(const TouchInfo& ti) {

    if (ti.mState == QEventPoint::Pressed) {
        TouchInfo nt			 = ti;
        DsQmlTouchCluster* relatedCluster = closeToCluster(ti.mPoint);
        DsQmlTouchCluster* alreadyCluster = findCluster(ti.mFingerId,ti.mPoint);
        if (relatedCluster == nullptr && alreadyCluster == nullptr) {
            DsQmlTouchCluster* clust = new DsQmlTouchCluster(this);

            clust->mTouches.push_back(nt);
            clust->mBoundingBox.setRect(nt.mPoint.x(), nt.mPoint.y(), 0.001, 0.001);
            clust->mCurrentBoundingBox.setRect(nt.mPoint.x(), nt.mPoint.y(), 0.001, 0.001);
            clust->mTouchCount = 1;
            clust->mClusterId  = mMaxClusterId;
            mMaxClusterId++;
            clust->configureCurrentBoundingBox();
            mClusters->addCluster(clust);//mClusters.push_back(clust);

            emit clusterUpdated(QEventPoint::Pressed, clust);
        } else if(!alreadyCluster) {
            DsQmlTouchCluster* clusty = relatedCluster;

            clusty->mTouches.push_back(nt);
            if (clusty->mTouches.size() == 1) {
                clusty->mBoundingBox.setRect(nt.mPoint.x(), nt.mPoint.y(), 0.001, 0.001);
            }

            if (clusty->mTouches.size() < m_minClusterTouchCount + 1 && !clusty->mTriggerable) {
                clusty->mInitialTouchTime = QDateTime::currentMSecsSinceEpoch();
            }
            clusty->addToBoundingBox(nt.mPoint, clusty->mBoundingBox);
            clusty->mTouchCount++;
            clusty->configureCurrentBoundingBox();
            emit clusterUpdated(QEventPoint::Updated, clusty);//if (mClusterUpdateFunction) mClusterUpdateFunction(ds::ui::TouchInfo::Moved, clusty);
        }


    } else if (ti.mState == QEventPoint::Updated) {
        DsQmlTouchCluster* relatedCluster = findCluster(ti.mFingerId, ti.mPoint);
        if (relatedCluster == nullptr) {
            // This seems to happen only if moved gets called before added,
            // which only happens in case of weird touch errors or from clicks.
            // In both cases, silently throw those touches out, they don't matter.
            // DS_LOG_WARNING("FiveFingerCluster couldn't find a related cluster for finger id:" << ti.fingerID);
            return;
        }

        DsQmlTouchCluster* clustyMcClustClust = relatedCluster;
        clustyMcClustClust->addToBoundingBox(ti.mPoint, clustyMcClustClust->mBoundingBox);
        clustyMcClustClust->configureCurrentBoundingBox();

        emit clusterUpdated(QEventPoint::Updated,clustyMcClustClust);//if (mClusterUpdateFunction) mClusterUpdateFunction(ds::ui::TouchInfo::Moved, clustyMcClustClust);

        if (clustyMcClustClust->mTriggerable) {
            if (clustyMcClustClust->mBoundingBox.width() > m_boundingBoxSize ||
                clustyMcClustClust->mBoundingBox.height() > m_boundingBoxSize) {
                clustyMcClustClust->mTriggerable = false;
                emit triggerableChanged(clustyMcClustClust->mTriggerable,clustyMcClustClust);
                // if (mTriggerableFunction)
                //     mTriggerableFunction(false, ci::vec2(clustyMcClustClust.mBoundingBox.getCenter().x,
                //                                          clustyMcClustClust.mBoundingBox.getCenter().y));
            }
        } else {
            if (clustyMcClustClust->mBoundingBox.width() < m_boundingBoxSize &&
                clustyMcClustClust->mBoundingBox.height() < m_boundingBoxSize &&
                clustyMcClustClust->mTouchCount > m_minClusterTouchCount - 1 &&
                (QDateTime::currentMSecsSinceEpoch() - clustyMcClustClust->mInitialTouchTime) >
                    m_triggerTime * 1000) {
                clustyMcClustClust->mTriggerable = true;
                emit triggerableChanged(clustyMcClustClust->mTriggerable,clustyMcClustClust);
                // if (mTriggerableFunction)
                //     mTriggerableFunction(true, ci::vec2(clustyMcClustClust.mBoundingBox.getCenter().x,
                //                                         clustyMcClustClust.mBoundingBox.getCenter().y));
            }
        }
    } else if (ti.mState == QEventPoint::Released) {
        DsQmlTouchCluster* relatedCluster = findCluster(ti.mFingerId, ti.mPoint);
        if (relatedCluster == nullptr) {
            // Disabling warning. This seems to happen only if removed gets called before added,
            // which only happens in case of weird touch errors or from RE clicks.
            // In both cases, silently throw those touches out, they don't matter.
            // DS_LOG_WARNING("FiveFingerCluster removed: couldn't find a related cluster for finger id:" <<
            // ti.fingerID);
            return;
        }

        DsQmlTouchCluster* clustyMcClustClust = relatedCluster;
        auto size = clustyMcClustClust->mTouches.size();
        for (int i = 0; i < size; i++) {
            if (clustyMcClustClust->mTouches[i].mFingerId == ti.mFingerId) {
                clustyMcClustClust->mTouches.erase(clustyMcClustClust->mTouches.begin() + i);
                clustyMcClustClust->mTouchCount--;
                if (clustyMcClustClust->mTouches.size() < m_minClusterTouchCount && !clustyMcClustClust->mTriggerable) {
                    clustyMcClustClust->mInitialTouchTime = QDateTime::currentMSecsSinceEpoch();

                }
                break;
            }
        }
        if (clustyMcClustClust->mTouches.size() == 0) {



            clustyMcClustClust->mTouches.clear();
            if (clustyMcClustClust->mTriggerable) {
                // if (mTriggeredFunction)
                //     mTriggeredFunction(ci::vec2(clustyMcClustClust.mBoundingBox.getCenter().x,
                //                                 clustyMcClustClust.mBoundingBox.getCenter().y));
                // if (mTriggerableFunction)
                //     mTriggerableFunction(false, ci::vec2(clustyMcClustClust.mBoundingBox.getCenter().x,
                //                                          clustyMcClustClust.mBoundingBox.getCenter().y));
                emit triggered(clustyMcClustClust);
                clustyMcClustClust->mTriggerable = false;
                emit triggerableChanged(clustyMcClustClust->mTriggerable,clustyMcClustClust);
            }
            clustyMcClustClust->mTouchCount = 0;
            clustyMcClustClust->m_closing = true;
            mClusters->removeCluster(clustyMcClustClust);
            clustyMcClustClust->deleteLater();
            emit clusterUpdated(QEventPoint::Released,clustyMcClustClust);//if (mClusterUpdateFunction) mClusterUpdateFunction(ds::ui::TouchInfo::Removed, clustyMcClustClust);
            //mClusters.erase(mClusters.begin() + relatedCluster);
        }
    }
}

int DsQmlClusterManager::minClusterTouchCount() const
{
    return m_minClusterTouchCount;
}

void DsQmlClusterManager::setMinClusterTouchCount(int newClusterTouchCount)
{
    if (m_minClusterTouchCount == newClusterTouchCount)
        return;
    m_minClusterTouchCount = newClusterTouchCount;
    emit minClusterTouchCountChanged();
}

DsQmlTouchClusterList* DsQmlClusterManager::clusters() const
{
    return mClusters;
}

bool DsQmlClusterManager::holdOpenOnTouch() const
{
    return m_holdOpenOnTouch;
}

void DsQmlClusterManager::setHoldOpenOnTouch(bool newHoldOpenOnTouch)
{
    if (m_holdOpenOnTouch == newHoldOpenOnTouch)
        return;
    m_holdOpenOnTouch = newHoldOpenOnTouch;
    emit holdOpenOnTouchChanged();
}

bool DsQmlClusterManager::doubleTapActivation() const
{
    return m_doubleTapActivation;
}

void DsQmlClusterManager::setDoubleTapActivation(bool newDoubleTapActivation)
{
    if (m_doubleTapActivation == newDoubleTapActivation)
        return;
    m_doubleTapActivation = newDoubleTapActivation;
    emit doubleTapActivationChanged();
}

float DsQmlClusterManager::boundingBoxSize() const
{
    return m_boundingBoxSize;
}

void DsQmlClusterManager::setBoundingBoxSize(float newBoundingBoxSize)
{
    if (qFuzzyCompare(m_boundingBoxSize, newBoundingBoxSize))
        return;
    m_boundingBoxSize = newBoundingBoxSize;
    emit boundingBoxSizeChanged();
}


float DsQmlClusterManager::minClusterSeperation() const
{
    return m_minClusterSeparation;
}

void DsQmlClusterManager::setMinClusterSeperation(float newMinClusterSeperation)
{
    if (qFuzzyCompare(m_minClusterSeparation, newMinClusterSeperation))
        return;
    m_minClusterSeparation = newMinClusterSeperation;
    emit minClusterSeperationChanged();
}

float DsQmlClusterManager::triggerTime() const
{
    return m_triggerTime;
}

void DsQmlClusterManager::setTriggerTime(float newTriggerTime)
{
    if (qFuzzyCompare(m_triggerTime, newTriggerTime))
        return;
    m_triggerTime = newTriggerTime;
    emit triggerTimeChanged();
}

} //namespace dsqt::ui
