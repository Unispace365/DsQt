#ifndef DSQMLAPPHOST_H
#define DSQMLAPPHOST_H

#include <QObject>
#include <QQmlEngine>


class QNetworkAccessManager;
class QUrl;
namespace dsqt {
class DsQmlAppHost : public QObject {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppHost)
  public:
    explicit DsQmlAppHost(QObject* parent = nullptr);

    Q_INVOKABLE void exit(bool quit=false);

  signals:
  private:
    QNetworkAccessManager* manager;
    QUrl baseUrl;
};
}
#endif // DSQMLAPPHOST_H
