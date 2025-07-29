#ifndef DSENVIRONMENTQML_H
#define DSENVIRONMENTQML_H

#include <QObject>
#include <QQmlEngine>
#include <dsenvironment.h>

namespace dsqt {

class DSEnvironmentQML : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Aint no need for you to be making one.")

    Q_PROPERTY(QString platformName READ getPlatformName WRITE setPlatformName NOTIFY platformNameChanged)

  public:
    explicit DSEnvironmentQML(QObject* parent = nullptr);

    Q_INVOKABLE const QString expand(const QString& string);
    Q_INVOKABLE const QUrl    expandUrl(const QString& string);
    // Q_INVOKABLE QString getLocalFolder();

    QString getPlatformName() const;
    void    setPlatformName(const QString& name);

  signals:
    void platformNameChanged(); // Signal emitted when platformName changes.

  private:
    QString m_platformName{"Unknown Platform"};
};

} // namespace dsqt

#endif // DSENVIRONMENTQML_H
