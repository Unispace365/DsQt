#pragma once

#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <functional>

class PackageCatalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY changed)
    Q_PROPERTY(QString packagePath READ packagePath WRITE setPackagePath NOTIFY changed)
    Q_PROPERTY(QVariantList packages READ packages NOTIFY changed)
    Q_PROPERTY(int packageIndex READ packageIndex WRITE setPackageIndex NOTIFY changed)
    Q_PROPERTY(QVariantList modules READ modules NOTIFY changed)
    Q_PROPERTY(QStringList selectedModules READ selectedModules NOTIFY changed)
    Q_PROPERTY(QString minimumQt READ minimumQt NOTIFY changed)
    Q_PROPERTY(QString exactQt READ exactQt NOTIFY changed)
    Q_PROPERTY(QString requirements READ requirements NOTIFY changed)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit PackageCatalog(QObject *parent = nullptr);
    ~PackageCatalog() override;
    QString source() const { return m_source; }
    void setSource(const QString &value);
    QString packagePath() const { return m_path; }
    void setPackagePath(const QString &value);
    QVariantList packages() const { return m_packages; }
    int packageIndex() const { return m_index; }
    void setPackageIndex(int value);
    QVariantList modules() const;
    QStringList selectedModules() const;
    QString minimumQt() const;
    QString exactQt() const;
    QString requirements() const;
    QString statusMessage() const;
    bool ready() const;
    bool busy() const { return m_busy; }
    Q_INVOKABLE void setModule(const QString &name, bool enabled);
    Q_INVOKABLE void refreshRegistry();
    Q_INVOKABLE void inspectPath();
    Q_INVOKABLE bool acceptsQt(const QString &version) const;
    QJsonObject selection() const;
signals:
    void changed();
private:
    QJsonObject metadata() const;
    void runGit(const QStringList &args, std::function<void(bool, QByteArray)> callback);
    void readVersions();
    void readNextPackage(int index);
    QString m_source = "registry";
    QString m_path;
    QStringList m_requested = {"Core"};
    QJsonObject m_bundled, m_local, m_localMetadata;
    QVariantList m_packages;
    int m_index = -1;
    bool m_busy = false;
    QString m_error, m_registryCommit, m_cache;
    QProcess m_git;
    QTimer m_timeout;
    std::function<void(bool, QByteArray)> m_callback;
};
