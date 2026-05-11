#include "dsQmlAppHost.h"
#include "QNetworkAccessManager"
#include "QNetworkReply"
#include "QNetworkRequest"
#include "dsSettings.h"
#include <qguiapplication.h>

namespace dsqt {
DsQmlAppHost::DsQmlAppHost(QObject* parent)
    : QObject{parent} {
    DsSettingsRef settingsEng = DsSettings::getSettings("engine");

    baseUrl.setUrl(settingsEng->getOr("appHost.baseUrl",QString("http://localhost:7800")));
    manager = new QNetworkAccessManager(this);
}

void DsQmlAppHost::exit(bool quit)
{
    QUrl exit("api/exit");
    auto response = manager->get(QNetworkRequest(baseUrl.resolved(exit)));
    connect(response,&QNetworkReply::finished,this,[this,quit](){
        if(quit){
            qApp->quit();
        }
    });

}


}