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
    mMinimumTouchesNeeded = mManager->minClusterTouchCount();
    emit managerChanged();
}

void ClusterView::onClusterUpdated(const QEventPoint::State &state, TouchCluster *cluster)
{

    switch(state){
    case QEventPoint::Pressed:
        {
            bool newMenu = false;
            //auto id = cluster->mClusterId;

            DelegateInstance *instance = nullptr;
            auto instanceIt = std::find_if(mInstanceStorage.begin(),mInstanceStorage.end(),[this](DelegateInstance* inst){
                return inst->mInUse == false || mManager->holdOpenOnTouch();
            });
            if(instanceIt != mInstanceStorage.end()){
                instance = *instanceIt;
                //qDebug()<<"Cluster ID added:"<<cluster->mClusterId<<":"<<(uint64_t)cluster<<" - instance reused";
            }
            else if(mDelegate) {
                //create an instance of the delegate.
                QVariantMap initProps = {{"model",menuModel()},{"config",menuConfig()}};
                QObject* obj = mDelegate->createWithInitialProperties(initProps);

                if(mDelegate->isError()){
                    for(auto& err:mDelegate->errors()){
                        //qDebug()<<"Menu creation error:"<<err.description();
                    }
                }
                QQuickItem* item = qobject_cast<QQuickItem*>(obj);

                if(item){
                    newMenu = true;
                    item->setParent(this);
                    //item->setProperty("model",model());

                    //qDebug()<<"Cluster ID added:"<<cluster->mClusterId<<":"<<(uint64_t)cluster<< " - new instance";
                    instance = new DelegateInstance();
                    instance->mItem = item;

                    mInstanceStorage.push_back(instance);


                }
            }

            if(instance){

                mInstanceMap[cluster->mClusterId] = instance;
                instance->mInUse = true;
                instance->mItem->setParentItem(this);
                ClusterAttachedType* attached = qobject_cast<ClusterAttachedType*>(qmlAttachedPropertiesObject<ClusterView>(instance->mItem));
                if(attached){
                    auto id = cluster->mClusterId;
                    instance->mConnection = connect(attached,&ClusterAttachedType::animateOffFinished,this,[this,instance,id](){
                        mInstanceMap.erase(id);
                        //qDebug()<<"Cluster ID Removed:"<<id<<":";
                        instance->mInUse = false;
                        instance->mItem->setParentItem(nullptr);
                        disconnect(instance->mConnection);
                    });
                    updateInstanceFromCluster(instance,cluster);
                    emit attached->created();
                }

                if(newMenu){
                    menuAdded(instance->mItem);
                }
                menuShown(instance->mItem);
            }

        }
        break;
    case QEventPoint::Released:
        {

            if(cluster && mInstanceMap.find(cluster->mClusterId)!=mInstanceMap.end()){
                DelegateInstance *instance = mInstanceMap.at(cluster->mClusterId);

                if(instance){
                    ClusterAttachedType* attached = qobject_cast<ClusterAttachedType*>(qmlAttachedPropertiesObject<ClusterView>(instance->mItem));
                    emit attached->released();
                    if(!mManager->holdOpenOnTouch()){
                        emit attached->removed();
                        updateInstanceFromCluster(instance,cluster);
                    }
                } else {
                    //qDebug()<<"Release but no instance 2";
                }
            } else {
                //qDebug()<<"1.Release but no instance :"<<cluster->mClusterId<<":"<<(uint64_t)cluster;;
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
        if(attached->minimumMet() == false){
        instance->mItem->setX(cluster->boundingBox().center().x());
        instance->mItem->setY(cluster->boundingBox().center().y());
        }
        attached->setMinimumMet(true);


    } else {
        attached->setMinimumMet(false);
    }
    auto point = instance->mItem->mapFromItem(this,cluster->boundingBox().center());
    emit attached->updated(point);
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

void ClusterAttachedType::setClusterView(ClusterView *view)
{
    m_clusterView = view;
}

void ClusterAttachedType::closeCluster()
{
   // m_clusterView->manager()->
}

QVariantList ClusterView::menuModel() const
{
    return m_menuModel;
}

void ClusterView::setMenuModel(const QVariantList &newModel)
{
    if (m_menuModel == newModel)
        return;
    m_menuModel = newModel;
    emit menuModelChanged();
}

QVariantMap ClusterView::menuConfig() const
{
    return m_menuConfig;
}

void ClusterView::setMenuConfig(const QVariantMap &newMenuConfig)
{
    if (m_menuConfig == newMenuConfig)
        return;
    m_menuConfig = newMenuConfig;
    emit menuConfigChanged();
}

void ClusterView::componentComplete()
{
    QQuickItem::componentComplete();

    //create an instance of the delegate. this should be the first one.
    //it's created to speed the creation upon first use by a user.
    DelegateInstance *instance = nullptr;
    QVariantMap initProps = {{"model",menuModel()},{"config",menuConfig()}};
    QObject* obj = mDelegate->createWithInitialProperties(initProps);

    if(mDelegate->isError()){
        for(auto& err:mDelegate->errors()){
            qDebug()<<"Menu creation error:"<<err.description();
        }
    }
    QQuickItem* item = qobject_cast<QQuickItem*>(obj);

    if(item){

        item->setParent(this);


        instance = new DelegateInstance();
        instance->mItem = item;

        mInstanceStorage.push_back(instance);


    }
}

}
