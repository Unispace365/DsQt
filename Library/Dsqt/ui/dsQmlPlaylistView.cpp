#include "dsQmlPlaylistView.h"
namespace dsqt::ui {
DsQmlPlaylistView::DsQmlPlaylistView()
{
    connect(this,&DsQmlPlaylistView::modelChanged,this,&DsQmlPlaylistView::loadTemplate);
    connect(this,&DsQmlPlaylistView::templateMapChanged,this,&DsQmlPlaylistView::processTemplateMap);
}


int DsQmlPlaylistView::intervalDefault() const
{
    return mIntervalDefault;
}

void DsQmlPlaylistView::setIntervalDefault(int newIntervalDefault)
{
    if (mIntervalDefault == newIntervalDefault)
        return;
    mIntervalDefault = newIntervalDefault;
    emit intervalDefaultChanged();
}

int DsQmlPlaylistView::interval() const
{
    return mInterval;
}

void DsQmlPlaylistView::setInterval(int newInterval)
{
    if (mInterval == newInterval)
        return;
    mInterval = newInterval;
    emit intervalChanged();
}

QString DsQmlPlaylistView::userKey() const
{
    return mUserKey;
}

void DsQmlPlaylistView::setUserKey(const QString &newUserKey)
{
    if (mUserKey == newUserKey)
        return;
    mUserKey = newUserKey;
    emit userKeyChanged();
}

QVariant DsQmlPlaylistView::model() const
{
    return mModel;
}

void DsQmlPlaylistView::setModel(const QVariant &newModel)
{
    if (mModel == newModel)
        return;
    mModel = newModel;
    emit modelChanged();
}

QVariantMap DsQmlPlaylistView::templateMap() const
{
    return mTemplateMap;
}

void DsQmlPlaylistView::setTemplateMap(const QVariantMap &newTemplateMap)
{
    if (mTemplateMap == newTemplateMap)
        return;
    mTemplateMap = newTemplateMap;
    emit templateMapChanged();
}

void DsQmlPlaylistView::next()
{

}

void DsQmlPlaylistView::prev()
{

}

void DsQmlPlaylistView::processTemplateMap()
{
    for (auto [key, value] : mTemplateComponents.asKeyValueRange()){
        value->deleteLater();
    }
    mTemplateComponents.clear();
    if(mTemplateMap.isEmpty()){
        return;
    }

    for (auto [key, value] : mTemplateMap.asKeyValueRange()){
        QUrl url;
        if(value.isValid()){
            url = QUrl::fromUserInput(value.toString());
            if(url.scheme() != "qrc" && url.scheme() != "file"){
                //log invalid url
                continue;
            }
        }
        QQmlComponent* component = new QQmlComponent(qmlEngine(this),this);

        component->loadUrl(url,QQmlComponent::CompilationMode::Asynchronous);
        mTemplateComponents.insert(key,component);
    }
    qInfo()<<"Processed "<<mTemplateComponents.size()<<" Templates to the map";
}

void DsQmlPlaylistView::loadTemplate()
{

}
}//namespace dsqt::ui
