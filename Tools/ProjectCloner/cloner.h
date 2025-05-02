#ifndef CLONER_H
#define CLONER_H

#include <QQuickItem>
#include <QDir>
#include <QtEnvironmentVariables>

#include "git_ignore_checker.h"

class Cloner : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString applicationName READ applicationName WRITE setApplicationName NOTIFY applicationNameChanged FINAL)
    Q_PROPERTY(QString projectName READ projectName WRITE setProjectName NOTIFY projectNameChanged FINAL)
    Q_PROPERTY(QString cloneToDirectory READ cloneToDirectory WRITE setCloneToDirectory NOTIFY cloneToDirectoryChanged FINAL)
    Q_PROPERTY(QString cloneFromDirectory READ cloneFromDirectory WRITE setCloneFromDirectory NOTIFY cloneFromDirectoryChanged FINAL)
    Q_PROPERTY(QList<QString> excludeList READ excludeList WRITE setExcludeList NOTIFY excludeListChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError WRITE setLastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged FINAL)
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

    QString applicationName() const;
    void setApplicationName(const QString &newApplicationName);

    QString projectName() const;
    void setProjectName(const QString &newProjectName);

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

signals:
    void cloned();
    void applicationNameChanged();
    void projectNameChanged();

    void cloneToDirectoryChanged();

    void cloneFromDirectoryChanged();

    void excludeListChanged();

    void lastErrorChanged();
    void lastErrorSet();
    void lastErrorCleared();

    void statusChanged();

private:
    GitIgnoreChecker* checker = nullptr;
    QString m_applicationName="";
    QString m_projectName="";
    QString m_cloneToDirectory="";
    QString m_cloneFromDirectory=qEnvironmentVariable("DS_QT_PLATFORM_100")+"\\Examples\\ClonerSource";
    QList<QString> m_excludeList;
    QList<QString> m_permanentExcludeList = {"CMakeLists.txt.user","build\\"};
    QString m_lastError;
    QString m_description = "Another fine Downstream production.";

private:
    bool processDirectory(QDir dir,QDir rootDir);
    bool replaceInFile(QString file,QString match,QString replacement);
    Status m_status = Status::PRECLONE;
};

#endif // CLONER_H
