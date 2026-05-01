#ifndef CLONER_H
#define CLONER_H

#include <QQuickItem>
#include <QDir>
#include <QStringList>
#include "git_ignore_checker.h"

class Cloner : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString applicationName READ applicationName WRITE setApplicationName NOTIFY applicationNameChanged FINAL)
    Q_PROPERTY(QString projectName READ projectName WRITE setProjectName NOTIFY projectNameChanged FINAL)
    Q_PROPERTY(QString platformName READ platformName WRITE setPlatformName NOTIFY platformNameChanged FINAL)
    Q_PROPERTY(QString cloneToDirectory READ cloneToDirectory WRITE setCloneToDirectory NOTIFY cloneToDirectoryChanged FINAL)
    Q_PROPERTY(QString cloneFromDirectory READ cloneFromDirectory WRITE setCloneFromDirectory NOTIFY cloneFromDirectoryChanged FINAL)
    Q_PROPERTY(QList<QString> excludeList READ excludeList WRITE setExcludeList NOTIFY excludeListChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError WRITE setLastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(bool copyTopLevel READ copyTopLevel WRITE setCopyTopLevel NOTIFY copyTopLevelChanged FINAL)
    Q_PROPERTY(bool usePlatformDir READ usePlatformDir WRITE setUsePlatformDir NOTIFY usePlatformDirChanged FINAL)
    Q_PROPERTY(QStringList detectedQtVersions READ detectedQtVersions NOTIFY detectedQtVersionsChanged FINAL)
    Q_PROPERTY(QStringList selectedQtVersions READ selectedQtVersions WRITE setSelectedQtVersions NOTIFY selectedQtVersionsChanged FINAL)
public:
    enum Status {
      PRECLONE,
      CLONING,
      POSTCLONE
    };
    Q_ENUM(Status)
public:
    Cloner();
    Q_INVOKABLE void clone();
    Q_INVOKABLE void clearLastError();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void detectQtVersions();

    QString applicationName() const;
    void setApplicationName(const QString &newApplicationName);

    QString projectName() const;
    void setProjectName(const QString &newProjectName);

    QString platformName() const;
    void setPlatformName(const QString &newPlatformName);

    QString cloneToDirectory() const;
    void setCloneToDirectory(const QString &newCloneToDirectory);

    QString cloneFromDirectory() const;
    void setCloneFromDirectory(const QString &newCloneFromDirectory);

    QList<QString> excludeList() const;
    void setExcludeList(const QList<QString> &newExcludeList);

    QString lastError() const;
    void setLastError(const QString &newLastError);

    Status status() const;
    void setStatus(Status newStatus);

    bool copyTopLevel() const;
    void setCopyTopLevel(bool newCopyTopLevel);

    bool usePlatformDir() const;
    void setUsePlatformDir(bool newUsePlatformDir);

    QStringList detectedQtVersions() const;

    QStringList selectedQtVersions() const;
    void setSelectedQtVersions(const QStringList &newSelectedQtVersions);

signals:
    void cloned();
    void applicationNameChanged();
    void projectNameChanged();
    void platformNameChanged();

    void cloneToDirectoryChanged();

    void cloneFromDirectoryChanged();

    void excludeListChanged();

    void lastErrorChanged();
    void lastErrorSet();
    void lastErrorCleared();

    void statusChanged();
    void copyTopLevelChanged();
    void usePlatformDirChanged();
    void detectedQtVersionsChanged();
    void selectedQtVersionsChanged();

private:
    GitIgnoreChecker* checker = nullptr;
    QString m_applicationName="";
    QString m_platformName="";
    QString m_projectName="";
    QString m_cloneToDirectory="";
    QString m_parentDirectory="";
    QString m_cloneFromDirectory;
    QList<QString> m_excludeList;
    QList<QString> m_permanentExcludeList = {"CMakeLists.txt.user","build\\"};
    QString m_lastError;

private:
    bool processDirectory(QDir dir,QDir rootDir);
    bool replaceInFile(QString file,QString match,QString replacement);
    bool copyIfNotExists(QDir sourceDir, QDir sourceRoot, const QString &destRoot);
    void addPlatformToProjectReadme(const QString &projectDir);
    QStringList scanForQtInstallations() const;
    QByteArray generateCMakePresets(const QStringList &versions) const;
    bool writeCMakePresets(const QString &destDir, const QStringList &versions);
    Status m_status = Status::PRECLONE;
    bool m_copyTopLevel = true;
    bool m_usePlatformDir = true;
    QStringList m_detectedQtVersions;
    QStringList m_selectedQtVersions;
};

#endif // CLONER_H
