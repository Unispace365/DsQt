#include "ui/dsQmlClusterManager.h"

#include <QPointerEvent>
#include <algorithm>

namespace dsqt::ui {
DsQmlClusterManager::DsQmlClusterManager(QQuickItem* parent):QQuickItem(parent) {
    this->setAcceptTouchEvents(true);
    this->setAcceptedMouseButtons(Qt::MouseButton::AllButtons);
    mClusters = new DsQmlTouchClusterList(this);
}

uint64_t DsQmlClusterManager::mMaxClusterId = 1;

void DsQmlClusterManager::updateSimulatedTouches(const QPointF &position, QEventPoint::State state)
{
    for (int i = 0; i < m_simulatedTouchCount; ++i) {
        const QTransform rotation = QTransform().rotate(360.0 * i / m_simulatedTouchCount);
        TouchInfo touch;
        touch.mPoint = position + 50 * rotation.map(QPointF(0, 1));
        // Negative IDs keep simulated contacts separate from real touch IDs (+1 below).
        touch.mFingerId = -i - 1;
        touch.mState = state;
        parseTouch(touch);
    }
}

void DsQmlClusterManager::claimTouchClusters(QTouchEvent *event)
{
    for (int i = 0; i < mClusters->rowCount(); ++i) {
        const auto cluster = mClusters->cluster(i);
        bool claimed = false;
        for (const auto &touch : cluster->mTouches)
            claimed |= m_claimedTouchIds.contains(touch.mFingerId);
        const bool qualifies = cluster->mTouchCount >= m_minClusterTouchCount
            && cluster->mBoundingBox.width() < m_boundingBoxSize
            && cluster->mBoundingBox.height() < m_boundingBoxSize;
        if (claimed || qualifies) {
            for (const auto &touch : cluster->mTouches)
                if (touch.mFingerId > 0)
                    m_claimedTouchIds.insert(touch.mFingerId);
        }
    }

    for (qsizetype i = 0; i < event->pointCount(); ++i) {
        auto &point = event->point(i);
        if (!m_claimedTouchIds.contains(point.id() + 1))
            continue;
        // Cancel passive taps as well as exclusive drags/buttons already tracking a finger.
        // Retain the grab until release so a shrinking cluster cannot leak its final tap.
        event->clearPassiveGrabbers(point);
        event->setExclusiveGrabber(point, this);
        point.setAccepted();
    }
    setKeepTouchGrab(!m_claimedTouchIds.isEmpty());
}

bool DsQmlClusterManager::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease: {
        auto mouse = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseButtonPress
            && (mouse->modifiers() & Qt::ShiftModifier) && m_simulatedTouchCount == 0) {
            m_simulatedTouchCount = qMax(1, m_minClusterTouchCount);
            setKeepMouseGrab(true);
        }
        if (m_simulatedTouchCount == 0)
            break;
        auto state = QEventPoint::Updated;
        if (event->type() == QEvent::MouseButtonPress)
            state = QEventPoint::Pressed;
        else if (event->type() == QEvent::MouseButtonRelease && mouse->buttons() == Qt::NoButton)
            state = QEventPoint::Released;
        mouse->clearPassiveGrabbers(mouse->point(0));
        mouse->setExclusiveGrabber(mouse->point(0), this);
        updateSimulatedTouches(mouse->position(), state);
        if (state == QEventPoint::Released) {
            m_simulatedTouchCount = 0;
            setKeepMouseGrab(false);
        }
        mouse->accept();
        return true;
    }
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
        auto touchEvent = static_cast<QTouchEvent *>(event);
        // Qt delivers touch packets initially accepted. Accept only the owned cluster,
        // leaving other users' fingers eligible for normal delivery.
        touchEvent->setAccepted(false);
        // Process the complete packet before claiming it: the fifth press may be last.
        for (const auto &point : touchEvent->points()) {
            if (point.state() == QEventPoint::Released || point.state() == QEventPoint::Stationary)
                continue;
            if (point.state() == QEventPoint::Pressed)
                touchEvent->addPassiveGrabber(point, this);
            parseTouch({point.id() + 1, point.state(), point.position()});
        }
        claimTouchClusters(touchEvent);
        bool consumed = false;
        for (const auto &point : touchEvent->points()) {
            consumed |= m_claimedTouchIds.contains(point.id() + 1);
            if (point.state() == QEventPoint::Released) {
                parseTouch({point.id() + 1, QEventPoint::Released, point.position()});
                m_claimedTouchIds.remove(point.id() + 1);
            }
        }
        setKeepTouchGrab(!m_claimedTouchIds.isEmpty());
        if (consumed) {
            if (touchEvent->allPointsAccepted())
                touchEvent->accept();
            return true; // Only claimed points are accepted; unrelated touches still pass.
        }
        break;
    }
    case QEvent::TouchCancel:
        cancelTouches(false);
        m_claimedTouchIds.clear();
        setKeepTouchGrab(false);
        break;
    default:
        break;
    }
    return QQuickItem::event(event);
}

void DsQmlClusterManager::cancelTouches(bool simulated)
{
    // Cancellation must not activate a menu as a successful release would.
    for (int i = mClusters->rowCount() - 1; i >= 0; --i) {
        const auto cluster = mClusters->cluster(i);
        const auto touches = cluster->mTouches;
        const bool affected = std::any_of(touches.begin(), touches.end(), [simulated](const auto &touch) {
            return (touch.mFingerId < 0) == simulated;
        });
        if (affected)
            emit clusterCanceled(cluster);
        for (const auto &touch : touches) {
            if ((touch.mFingerId < 0) != simulated)
                continue;
            if (cluster->mTriggerable) {
                cluster->mTriggerable = false;
                emit triggerableChanged(false, cluster);
            }
            parseTouch({touch.mFingerId, QEventPoint::Released, touch.mPoint});
        }
    }
}

void DsQmlClusterManager::mouseUngrabEvent()
{
    if (m_simulatedTouchCount > 0) {
        cancelTouches(true);
        m_simulatedTouchCount = 0;
        setKeepMouseGrab(false);
    }
    QQuickItem::mouseUngrabEvent();
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
            clust->mTriggerable = false;
            clust->mInitialTouchTime = QDateTime::currentMSecsSinceEpoch();
            clust->m_active = false;
            clust->m_closing = false;

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

} // namespace dsqt::ui
