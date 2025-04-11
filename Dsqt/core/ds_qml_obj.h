#ifndef DS_QML_OBJ_H
#define DS_QML_OBJ_H

#include <QObject>
#include <QQmlEngine>
#include <QDebug>
#include "core/dsqmlapplicationengine.h"
#include "core/dsenvironmentqml.h"
#include "settings/dssettings_proxy.h"
#include <qqmlintegration.h>

Q_DECLARE_LOGGING_CATEGORY(lgQmlObj)
Q_DECLARE_LOGGING_CATEGORY(lgQmlObjVerbose)
namespace dsqt {
class DsQmlObj : public QObject
{

    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(DS)

    Q_PROPERTY(dsqt::DSEnvironmentQML* env READ env NOTIFY envChanged)
    Q_PROPERTY(dsqt::DSSettingsProxy* appSettings READ appSettings NOTIFY appSettingsChanged)
    Q_PROPERTY(dsqt::DSQmlApplicationEngine* engine READ engine NOTIFY engineChanged)
public:
    explicit DsQmlObj(int force,QObject *parent = nullptr);
    static DsQmlObj* create(QQmlEngine *qmlEngine, QJSEngine *);
    DSSettingsProxy* appSettings() const;
    DSEnvironmentQML* env() const;
    DSQmlApplicationEngine* engine() const;
signals:

    void envChanged();
    void appSettingsChanged();
    void engineChanged();

private:
    DSQmlApplicationEngine* mEngine = nullptr;

};
}

#endif // DS_QML_OBJ_H
