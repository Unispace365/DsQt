#include "touchcluster.h"


namespace dsqt::ui {

TouchCluster::TouchCluster(QQuickItem *parent): QQuickItem(parent)
{
    //addTouch(touchPoint);
    //updateTouchCount();
}

void TouchCluster::addToBoundingBox(QPointF point, QRectF &boundingBox)
{
    if(!boundingBox.contains(point)){
        boundingBox = boundingBox.united(QRectF(point,point));
    }
}

void TouchCluster::configureCurrentBoundingBox(){
    auto oldbb = mCurrentBoundingBox;
    auto oldCenter = m_center;
    if(mTouches.size()<1){
        mCurrentBoundingBox.setCoords(0,0,0,0);

    } else {
        mCurrentBoundingBox.setTopLeft(mTouches[0].mPoint);
        mCurrentBoundingBox.setBottomRight(mTouches[0].mPoint);
        for(auto& touch:mTouches){
            addToBoundingBox(touch.mPoint,mCurrentBoundingBox);
        }
    }
    if(oldbb != mCurrentBoundingBox){
        emit boundingBoxChanged();
    }

    m_center = mCurrentBoundingBox.center();
    if(m_center != oldCenter) {
        emit clusterCenterChanged();
    }
}


//TouchClusterList
TouchClusterList::TouchClusterList(QObject *parent):QAbstractListModel(parent)
{

}

TouchClusterList::~TouchClusterList()
{

}

int TouchClusterList::rowCount(const QModelIndex &parent) const
{
    //qDebug()<<"CLUSTER SIZE:"<<mClusters.size();
    if(parent.isValid()){
        return mClusters.size();
    }
    return mClusters.size();
}

QVariant TouchClusterList::data(const QModelIndex &index, int role) const
{
    int i = index.row();
    if(i <0 || i >= mClusters.size())
        return QVariant();
    if(role==Cluster){
        return QVariant::fromValue(mClusters.at(i));
    }
    return QVariant();

}

void TouchClusterList::addCluster(TouchCluster *cluster)
{

    if(std::find(mClusters.begin(),mClusters.end(),cluster) == mClusters.end()){
        beginInsertRows(QModelIndex(),rowCount(),rowCount());
        mClusters.push_back(cluster);
        endInsertRows();
    }
}

void TouchClusterList::removeCluster(TouchCluster *cluster)
{
    auto itr = std::find(mClusters.begin(),mClusters.end(),cluster);
    if(itr == mClusters.end()) return;
    int idx = itr - mClusters.begin();

    beginRemoveRows(QModelIndex(),idx,idx);
    mClusters.erase(itr);
    endRemoveRows();
}


TouchCluster *TouchClusterList::cluster(int idx)
{
    if(idx <0 || idx >= mClusters.size())
        return nullptr;

    return mClusters.at(idx);
}

QHash<int, QByteArray> TouchClusterList::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Cluster] = "cluster";
    return roles;

}

QRectF TouchCluster::boundingBox() const
{
    return mCurrentBoundingBox;
}

QPointF TouchCluster::clusterCenter() const
{
    return m_center;
}

bool TouchCluster::active() const
{
    return m_active;
}

bool TouchCluster::closing() const
{
    return m_closing;
}


}
