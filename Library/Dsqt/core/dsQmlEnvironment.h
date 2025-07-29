#ifndef DSQMLENVIRONMENT_H
#define DSQMLENVIRONMENT_H

#include <QObject>
#include <QQmlEngine>
#include <dsEnvironment.h>
namespace dsqt {
class DsQmlEnvironment : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEnvironment)
    QML_UNCREATABLE("Aint no need for you to be making one.")
public:
    explicit DsQmlEnvironment(QObject *parent = nullptr);
    Q_INVOKABLE const QString expand(const QString& string);
    Q_INVOKABLE const QUrl expandUrl(const QString& string);
    //Q_INVOKABLE QString getLocalFolder();
signals:

};
}//namespace dsqt

#endif // DSQMLENVIRONMENT_H
