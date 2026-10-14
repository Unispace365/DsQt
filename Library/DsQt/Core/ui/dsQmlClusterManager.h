#ifndef DSQMLCLUSTERMANAGER_H
#define DSQMLCLUSTERMANAGER_H

#include "ui/dsQmlTouchCluster.h"

#include <QEvent>
#include <QPointerEvent>
#include <QQuickItem>
#include <QSet>

namespace dsqt::ui {


class DsQmlClusterManager : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsClusterManager)

    Q_PROPERTY(int minClusterTouchCount READ minClusterTouchCount WRITE setMinClusterTouchCount NOTIFY minClusterTouchCountChanged FINAL)
    Q_PROPERTY(float boundingBoxSize READ boundingBoxSize WRITE setBoundingBoxSize NOTIFY boundingBoxSizeChanged FINAL)
    Q_PROPERTY(bool doubleTapActivation READ doubleTapActivation WRITE setDoubleTapActivation NOTIFY doubleTapActivationChanged FINAL)
    Q_PROPERTY(bool holdOpenOnTouch READ holdOpenOnTouch WRITE setHoldOpenOnTouch NOTIFY holdOpenOnTouchChanged FINAL)
    Q_PROPERTY(float minClusterSeperation READ minClusterSeperation WRITE setMinClusterSeperation NOTIFY minClusterSeperationChanged FINAL)
    Q_PROPERTY(float triggerTime READ triggerTime WRITE setTriggerTime NOTIFY triggerTimeChanged FINAL)
    Q_PROPERTY(dsqt::ui::DsQmlTouchClusterList* clusters READ clusters NOTIFY clustersChanged FINAL)

public:
    explicit DsQmlClusterManager(QQuickItem* parent=nullptr);
    bool event(QEvent *event) override;

    int minClusterTouchCount() const;
    void setMinClusterTouchCount(int newClusterTouchCount);

    DsQmlTouchClusterList* clusters() const;

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
    void clusterUpdated(const QEventPoint::State& state, DsQmlTouchCluster*);
    void triggerableChanged(bool triggerable,DsQmlTouchCluster*);
    void triggered(DsQmlTouchCluster*);
    void minClusterSeperationChanged();

    void triggerTimeChanged();

protected:
    void mouseUngrabEvent() override;

private:
    /*
     * Finds the nearest cluster or if no cluster is nearest returns a new cluster.
     */


    DsQmlTouchCluster* closeToCluster(const QPointF& point);
    DsQmlTouchCluster* findCluster(int fingerId, const QPointF& point);


    void updateClusters(const QPointerEvent::Type& type);
    DsQmlTouchClusterList* mClusters;
    static uint64_t mMaxClusterId;
    bool m_holdOpenOnTouch = false;
    int m_minClusterTouchCount = 3;
    bool m_doubleTapActivation = false;
    float m_boundingBoxSize = 300;

    float m_minClusterSeparation = 300;
    void parseTouch(const TouchInfo &ti);
    float m_triggerTime = 0.1f;
    // Once claimed, a cluster keeps its remaining fingers even below the activation count.
    QSet<int> m_claimedTouchIds;
    int m_simulatedTouchCount = 0;
    void updateSimulatedTouches(const QPointF &position, QEventPoint::State state);
    void claimTouchClusters(QTouchEvent *event);
    void cancelTouches(bool simulated);
};
}
#endif // DSQMLCLUSTERMANAGER_H
