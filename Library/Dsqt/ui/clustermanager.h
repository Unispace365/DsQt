#ifndef CLUSTERMANAGER_H
#define CLUSTERMANAGER_H

#include "touchcluster.h"
#include <QQuickItem>
#include <QEvent>
#include <QPointerEvent>
namespace dsqt::ui {


class ClusterManager : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int minClusterTouchCount READ minClusterTouchCount WRITE setMinClusterTouchCount NOTIFY minClusterTouchCountChanged FINAL)
    Q_PROPERTY(float boundingBoxSize READ boundingBoxSize WRITE setBoundingBoxSize NOTIFY boundingBoxSizeChanged FINAL)
    Q_PROPERTY(bool doubleTapActivation READ doubleTapActivation WRITE setDoubleTapActivation NOTIFY doubleTapActivationChanged FINAL)
    Q_PROPERTY(bool holdOpenOnTouch READ holdOpenOnTouch WRITE setHoldOpenOnTouch NOTIFY holdOpenOnTouchChanged FINAL)
    Q_PROPERTY(float minClusterSeperation READ minClusterSeperation WRITE setMinClusterSeperation NOTIFY minClusterSeperationChanged FINAL)
    Q_PROPERTY(float triggerTime READ triggerTime WRITE setTriggerTime NOTIFY triggerTimeChanged FINAL)
    Q_PROPERTY(TouchClusterList* clusters READ clusters NOTIFY clustersChanged FINAL)

public:
    explicit ClusterManager(QQuickItem* parent=nullptr);
    bool event(QEvent *event) override;

    int minClusterTouchCount() const;
    void setMinClusterTouchCount(int newClusterTouchCount);

    TouchClusterList* clusters() const;

    bool holdOpenOnTouch() const;
    void setHoldOpenOnTouch(bool newHoldOpenOnTouch);

    bool doubleTapActivation() const;
    void setDoubleTapActivation(bool newDoubleTapActivation);

    float boundingBoxSize() const;
    void setBoundingBoxSize(float newBoundingBoxSize);

    float minClusterSeperation() const;
    void setMinClusterSeperation(float newMinClusterSeperation);

    float triggerTime() const;
    void setTriggerTime(float newTriggerTime);

signals:
    void clusterRadiusChanged();
    void holdOpenOnTouchChanged();
    void minClusterTouchCountChanged();
    void clustersChanged();
    void doubleTapActivationChanged();
    void boundingBoxSizeChanged();

    //non property signals
    void clusterUpdated(const QEventPoint::State& state, TouchCluster*);
    void triggerableChanged(bool triggerable,TouchCluster*);
    void triggered(TouchCluster*);
    void minClusterSeperationChanged();

    void triggerTimeChanged();

private:
    /*
     * Finds the nearest cluster or if no cluster is nearest returns a new cluster.
     */


    TouchCluster* closeToCluster(const QPointF& point);
    TouchCluster* findCluster(int fingerId, const QPointF& point);


    void updateClusters(const QPointerEvent::Type& type);
    TouchClusterList* mClusters;
    static uint64_t mMaxClusterId;
    bool m_holdOpenOnTouch = false;
    int m_minClusterTouchCount = 3;
    bool m_doubleTapActivation = false;
    float m_boundingBoxSize;

    float m_minClusterSeparation;
    void parseTouch(const TouchInfo &ti);
    float m_triggerTime;
};
}
#endif // CLUSTERMANAGER_H
