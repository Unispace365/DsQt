#ifndef DSQMLENVIRONMENT_H
#define DSQMLENVIRONMENT_H

#include <QObject>
#include <QQmlEngine>
#include <dsEnvironment.h>

namespace dsqt {
class DsQmlEnvironment : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEnvironment)
    QML_UNCREATABLE("Aint no need for you to be making one.")

    Q_PROPERTY(QString platformName READ platformName WRITE setPlatformName NOTIFY platformNameChanged)

  public:
    explicit DsQmlEnvironment(QObject* parent = nullptr);

    Q_INVOKABLE const QString expand(const QString& string);
    Q_INVOKABLE const QUrl    expandUrl(const QString& string);
    // Q_INVOKABLE QString getLocalFolder();


    QString platformName() const { return m_platformName; }
    void    setPlatformName(const QString& name) {
        if (name == m_platformName) return;
        m_platformName = name;
        emit platformNameChanged();
    }

  signals:
    void platformNameChanged(); // Signal emitted when platformName changes.

  private:
    QString m_platformName{"Unknown Platform"};
};

} // namespace dsqt

#endif // DSENVIRONMENTQML_H
