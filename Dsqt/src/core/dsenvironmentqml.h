#ifndef DSENVIRONMENTQML_H
#define DSENVIRONMENTQML_H

#include <QObject>
#include <QQmlEngine>
#include <dsenvironment.h>
namespace dsqt {
class DSEnvironmentQML : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Aint no need for you to be making one.")
public:
    explicit DSEnvironmentQML(QObject *parent = nullptr);
    Q_INVOKABLE const QString expand(const QString& string);
    Q_INVOKABLE const QUrl expandUrl(const QString& string);
    //Q_INVOKABLE QString getLocalFolder();
signals:

};
}//namespace dsqt

#endif // DSENVIRONMENTQML_H
