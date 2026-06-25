#include "dsQmlAppHost.h"
#include "QNetworkAccessManager"
#include "QNetworkReply"
#include "QNetworkRequest"
#include "dsSettings.h"
#include <qguiapplication.h>

namespace dsqt {
DsQmlAppHost::DsQmlAppHost(QObject* parent)
    : QObject{parent} {
    baseUrl.setUrl(Settings::find<QString>("engine", "appHost.baseUrl", "http://localhost:7800"));
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

void DsQmlAppHost::kiosk()
{
    {
        QUrl doKiosk("api/kiosk");
        auto response = manager->get(QNetworkRequest(baseUrl.resolved(doKiosk)));
        connect(response,&QNetworkReply::finished,this, [this]{});
    }

    {
        QUrl alwaysOnTop("api/alwaysontop/reset");
        auto response = manager->get(QNetworkRequest(baseUrl.resolved(alwaysOnTop)));
        connect(response,&QNetworkReply::finished,this, [this]{});
    }
}

void DsQmlAppHost::unkiosk()
{
    {
        QUrl doUnKiosk("api/unkiosk");
        auto response = manager->get(QNetworkRequest(baseUrl.resolved(doUnKiosk)));
        connect(response,&QNetworkReply::finished,this, [this]{});
    }

    {
        QUrl noAlwaysOnTop("api/alwaysontop/disable");
        auto response = manager->get(QNetworkRequest(baseUrl.resolved(noAlwaysOnTop)));
        connect(response,&QNetworkReply::finished,this, [this]{});
    }
}

void DsQmlAppHost::reconfigure()
{
    QUrl doReconfigure("api/Reconfigure");
    auto response = manager->get(QNetworkRequest(baseUrl.resolved(doReconfigure)));
    connect(response,&QNetworkReply::finished,this, [this]{});
}

// QString DsQmlAppHost::getStatus()
// {
//     QUrl doReconfigure("api/Status");
//     auto response = manager->get(QNetworkRequest(baseUrl.resolved(doReconfigure)));
//     connect(response,&QNetworkReply::finished,this, [this, response]{
//         qInfo() << response->readAll();
//       });
//     return QString();
// }

}
