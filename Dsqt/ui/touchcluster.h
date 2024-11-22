#ifndef TOUCHCLUSTER_H
#define TOUCHCLUSTER_H

#include <QObject>
#include <QQmlEngine>
#include <QPointerEvent>
#include <QEventPoint>
#include <QAbstractListModel>
#include <qproperty.h>
#include <QQuickItem>
#include <glm/vec2.hpp>

namespace dsqt::ui {

//delete when we are sure we dont need it

struct TouchInfo {
    int mFingerId;
    QEventPoint::State mState; //mPhase
    QPointF mPoint = QPointF(0,0);
};

class ClusterManager;

class TouchCluster : public QQuickItem
{
    friend ClusterManager;
    Q_OBJECT
    QML_ELEMENT
    //QML_UNCREATABLE("")
    Q_PROPERTY(QRectF boundingBox MEMBER mCurrentBoundingBox READ boundingBox NOTIFY boundingBoxChanged FINAL)
    Q_PROPERTY(QPointF clusterCenter MEMBER m_center READ clusterCenter NOTIFY clusterCenterChanged FINAL)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool closing READ closing NOTIFY closingChanged FINAL)
public:
    explicit TouchCluster(QQuickItem *parent = nullptr);

    void addToBoundingBox(QPointF point,QRectF& boundingBox);
    void configureCurrentBoundingBox();

signals:

    void touchCountChanged();
    void activeChanged();

    void boundingBoxChanged();

    void clusterCenterChanged();

    void closingChanged();

public:

    int mClusterId;
    QRectF mBoundingBox;
    QRectF mCurrentBoundingBox;
    std::vector<TouchInfo> mTouches;
    int mTouchCount; //mMaxTouches
    bool mTriggerable;
    int64_t mInitialTouchTime;

    QRectF boundingBox() const;
    QPointF clusterCenter() const;

    bool active() const;

    bool closing() const;

private:
    QPointF m_center;

    bool m_active;
    bool m_closing;
};

typedef std::shared_ptr<TouchCluster> TouchClusterPtr;

class TouchClusterList : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum Roles {
        Cluster = Qt::UserRole + 1
    };
    friend ClusterManager;
    explicit TouchClusterList(QObject * parent= nullptr);
    ~TouchClusterList();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index,int role = Qt::DisplayRole) const override;
    void addCluster(TouchCluster* cluster);
    void removeCluster(TouchCluster* cluster);

public slots:
    TouchCluster* cluster(int idx);
private:
    std::vector<TouchCluster*> mClusters;


    // QAbstractItemModel interface
public:


    // QAbstractItemModel interface
public:
    QHash<int, QByteArray> roleNames() const override;
};

}

Q_DECLARE_METATYPE(dsqt::ui::TouchCluster)

#endif // TOUCHCLUSTER_H
