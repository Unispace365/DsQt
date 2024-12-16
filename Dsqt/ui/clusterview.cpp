#include "clusterview.h"


namespace dsqt::ui {
ClusterView::ClusterView(QQuickItem* parent):QQuickItem(parent) {}

QQmlComponent *ClusterView::delegate() const
{
    return mDelegate;
}

void ClusterView::setDelegate(QQmlComponent *newDelegate)
{
    if (mDelegate == newDelegate)
        return;
    mDelegate = newDelegate;
    emit delegateChanged();
}

ClusterManager *ClusterView::manager() const
{
    return mManager;
}

void ClusterView::setManager(ClusterManager *newManager)
{
    if (mManager == newManager)
        return;

    //diconnect from the signals;
    for(auto& conn:mManagerConnections){
        QObject::disconnect(conn);
    }
    mManagerConnections.clear();

    mManager = newManager;

    //connect to signals
    auto conn=connect(mManager,&ClusterManager::clusterUpdated,this,&ClusterView::onClusterUpdated);
    mManagerConnections.push_back(conn);

    emit managerChanged();
}

void ClusterView::onClusterUpdated(const QEventPoint::State &state, TouchCluster *cluster)
{

    switch(state){
    case QEventPoint::Pressed:
        if(mDelegate){
            //create an instance of the delegate.
            QObject* obj = mDelegate->create();
            QQuickItem* item = qobject_cast<QQuickItem*>(obj);

            if(item){
                item->setParent(this);
                item->setParentItem(this);
                item->setX(cluster->boundingBox().center().x());
                item->setY(cluster->boundingBox().center().y());
                //qDebug()<<"Cluster ID added:"<<cluster->mClusterId<<":"<<(uint64_t)cluster;
                DelegateInstance *instance = new DelegateInstance();
                instance->mItem = item;
                instance->mInUse = true;
                mInstanceStorage.push_back(instance);
                mInstanceMap[cluster->mClusterId] = instance;

                ClusterAttachedType* attached = qobject_cast<ClusterAttachedType*>(qmlAttachedPropertiesObject<ClusterView>(instance->mItem));
                if(attached){
                    auto id = cluster->mClusterId;
                    instance->mConnection = connect(attached,&ClusterAttachedType::animateOffFinished,this,[this,instance,id](){
                        mInstanceMap.erase(id);
                        //qDebug()<<"Cluster ID Removed:"<<id<<":";
                        instance->mInUse = false;
                        disconnect(instance->mConnection);
                    });
                    emit attached->created();
                }
                updateInstanceFromCluster(instance,cluster);
            }
        }
        break;
    case QEventPoint::Released:
        {
            if(cluster && mInstanceMap.find(cluster->mClusterId)!=mInstanceMap.end()){
                DelegateInstance *instance = mInstanceMap.at(cluster->mClusterId);

                if(instance){
                    ClusterAttachedType* attached = qobject_cast<ClusterAttachedType*>(qmlAttachedPropertiesObject<ClusterView>(instance->mItem));
                    emit attached->removed();
                    updateInstanceFromCluster(instance,cluster);
                } else {
                    qDebug()<<"Release but no instance 2";
                }
            } else {
                qDebug()<<"1.Release but no instance :"<<cluster->mClusterId<<":"<<(uint64_t)cluster;;
            }

        }
        break;
    default:
        {
            if(cluster && mInstanceMap.find(cluster->mClusterId)!=mInstanceMap.end()){
                DelegateInstance *instance = mInstanceMap.at(cluster->mClusterId);
                if(instance){
                    updateInstanceFromCluster(instance,cluster);
                }
            }
        }
        break;

    }
}

void dsqt::ui::ClusterView::updateInstanceFromCluster(DelegateInstance *instance, TouchCluster *cluster)
{
    ClusterAttachedType* attached = qobject_cast<ClusterAttachedType*>(qmlAttachedPropertiesObject<ClusterView>(instance->mItem));
    if(cluster->mTouchCount>=mMinimumTouchesNeeded){
        //get the attached property object.

        attached->setMinimumMet(true);
    } else {
        attached->setMinimumMet(false);
    }
}

ClusterAttachedType::ClusterAttachedType(QObject *parent):QObject(parent)
{

}

bool ClusterAttachedType::minimumMet() const
{
    return m_minimumMet;
}

void ClusterAttachedType::setMinimumMet(bool newMinimumMet)
{
    if (m_minimumMet == newMinimumMet)
        return;
    m_minimumMet = newMinimumMet;
    emit minimumMetChanged();
}

}
