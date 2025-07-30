#include "ui/dsQmlClusterView.h"

namespace dsqt::ui {
DsQmlClusterView::DsQmlClusterView(QQuickItem* parent):QQuickItem(parent) {}

QQmlComponent *DsQmlClusterView::delegate() const
{
    return mDelegate;
}

void DsQmlClusterView::setDelegate(QQmlComponent *newDelegate)
{
    if (mDelegate == newDelegate)
        return;
    mDelegate = newDelegate;
    emit delegateChanged();
}

DsQmlClusterManager *DsQmlClusterView::manager() const
{
    return mManager;
}

void DsQmlClusterView::setManager(DsQmlClusterManager *newManager)
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
    auto conn=connect(mManager,&DsQmlClusterManager::clusterUpdated,this,&DsQmlClusterView::onClusterUpdated);
    mManagerConnections.push_back(conn);
    mMinimumTouchesNeeded = mManager->minClusterTouchCount();
    emit managerChanged();
}

void DsQmlClusterView::onClusterUpdated(const QEventPoint::State &state, DsQmlTouchCluster *cluster)
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
                DsQmlClusterAttachedType* attached = qobject_cast<DsQmlClusterAttachedType*>(qmlAttachedPropertiesObject<DsQmlClusterView>(instance->mItem));
                if(attached){
                    auto id = cluster->mClusterId;
                    instance->mConnection = connect(attached,&DsQmlClusterAttachedType::animateOffFinished,this,[this,instance,id](){
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
                    DsQmlClusterAttachedType* attached = qobject_cast<DsQmlClusterAttachedType*>(qmlAttachedPropertiesObject<DsQmlClusterView>(instance->mItem));
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

void dsqt::ui::DsQmlClusterView::updateInstanceFromCluster(DelegateInstance *instance, DsQmlTouchCluster *cluster)
{
    DsQmlClusterAttachedType* attached = qobject_cast<DsQmlClusterAttachedType*>(qmlAttachedPropertiesObject<DsQmlClusterView>(instance->mItem));
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

DsQmlClusterAttachedType::DsQmlClusterAttachedType(QObject *parent):QObject(parent)
{

}

bool DsQmlClusterAttachedType::minimumMet() const
{
    return m_minimumMet;
}

void DsQmlClusterAttachedType::setMinimumMet(bool newMinimumMet)
{
    if (m_minimumMet == newMinimumMet)
        return;
    m_minimumMet = newMinimumMet;
    emit minimumMetChanged();
}

void DsQmlClusterAttachedType::setClusterView(DsQmlClusterView *view)
{
    m_clusterView = view;
}

void DsQmlClusterAttachedType::closeCluster()
{
   // m_clusterView->manager()->
}

QVariantList DsQmlClusterView::menuModel() const
{
    return m_menuModel;
}

void DsQmlClusterView::setMenuModel(const QVariantList &newModel)
{
    if (m_menuModel == newModel)
        return;
    m_menuModel = newModel;
    emit menuModelChanged();
}

QVariantMap DsQmlClusterView::menuConfig() const
{
    return m_menuConfig;
}

void DsQmlClusterView::setMenuConfig(const QVariantMap &newMenuConfig)
{
    if (m_menuConfig == newMenuConfig)
        return;
    m_menuConfig = newMenuConfig;
    emit menuConfigChanged();
}

void DsQmlClusterView::componentComplete()
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
