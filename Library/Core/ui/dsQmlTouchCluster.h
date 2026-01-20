#ifndef DSQMLTOUCHCLUSTER_H
#define DSQMLTOUCHCLUSTER_H

#include <QAbstractListModel>
#include <QEventPoint>
#include <QObject>
#include <QPointerEvent>
#include <QQmlEngine>
#include <QQuickItem>
#include <qproperty.h>

namespace dsqt::ui {

//delete when we are sure we dont need it

struct TouchInfo {
    int mFingerId;
    QEventPoint::State mState; //mPhase
    QPointF mPoint = QPointF(0,0);
};

class DsQmlClusterManager;

class DsQmlTouchCluster : public QQuickItem
{
    friend DsQmlClusterManager;
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchCluster)
    //QML_UNCREATABLE("")
    Q_PROPERTY(QRectF boundingBox MEMBER mCurrentBoundingBox READ boundingBox NOTIFY boundingBoxChanged FINAL)
    Q_PROPERTY(QPointF clusterCenter MEMBER m_center READ clusterCenter NOTIFY clusterCenterChanged FINAL)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool closing READ closing NOTIFY closingChanged FINAL)
public:
    explicit DsQmlTouchCluster(QQuickItem *parent = nullptr);

    void addToBoundingBox(QPointF point,QRectF& boundingBox);
    void configureCurrentBoundingBox();

signals:


    void touchCountChanged();
    void activeChanged();

    void boundingBoxChanged();

    void clusterCenterChanged();

    void closingChanged();

public:

    int mClusterId=0;
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

typedef std::shared_ptr<DsQmlTouchCluster> TouchClusterPtr;

class DsQmlTouchClusterList : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchClusterList)
public:
    enum Roles {
        Cluster = Qt::UserRole + 1
    };
    friend DsQmlClusterManager;
    explicit DsQmlTouchClusterList(QObject * parent= nullptr);
    ~DsQmlTouchClusterList();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index,int role = Qt::DisplayRole) const override;
    void addCluster(DsQmlTouchCluster* cluster);
    void removeCluster(DsQmlTouchCluster* cluster);

public slots:
    dsqt::ui::DsQmlTouchCluster* cluster(int idx);
private:
    std::vector<DsQmlTouchCluster*> mClusters;


    // QAbstractItemModel interface
public:


    // QAbstractItemModel interface
public:
    QHash<int, QByteArray> roleNames() const override;
};

}

Q_DECLARE_METATYPE(dsqt::ui::DsQmlTouchCluster)

#endif // DSQMLTOUCHCLUSTER_H
