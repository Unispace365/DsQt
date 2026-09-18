#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>

namespace dsqt::settings {

// Verifies that `obj` is being accessed from the thread it lives in.
// Settings/SettingsFile are not thread-safe; this check is always compiled in
// (Q_ASSERT alone vanishes in Release builds): it logs a qCritical in every
// build and additionally halts in Debug builds.
inline void checkThread(const QObject *obj, const char *function)
{
    if (Q_LIKELY(QThread::currentThread() == obj->thread()))
        return;
    qCritical("%s called from thread %p, but the object lives in thread %p. "
              "Settings/SettingsFile are not thread-safe; access them only "
              "from the thread that owns them.",
              function,
              static_cast<void *>(QThread::currentThread()),
              static_cast<void *>(obj->thread()));
    Q_ASSERT(false);
}

// Returns true if path refers to a Qt resource (":/..." or "qrc:...").
inline bool isResourcePath(const QString &path)
{
    return path.startsWith(u":/") || path.startsWith(u"qrc:");
}

// Walks up from startDir looking for a subdirectory named "settings".
// Returns its absolute path if found, an empty string otherwise.
inline QString findSettingsDir(const QString &startDir)
{
    QDir dir(startDir);
    do {
        const QString candidate = dir.filePath("settings");
        if (QFileInfo(candidate).isDir())
            return candidate;
    } while (dir.cdUp());
    return {};
}

// Resolves all existing file paths for fileName across searchPaths.
// If searchPaths is empty, falls back to the auto-discovered "settings" directory.
inline QStringList resolveFilePaths(const QString &fileName, const QStringList &searchPaths)
{
    if (fileName.isEmpty())
        return {};

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString settingsDir = findSettingsDir(appDir);

    const QStringList paths = searchPaths.isEmpty() ? QStringList{settingsDir} : searchPaths;

    QStringList result;
    for (const QString &dir : paths) {
        if (dir.isEmpty())
            continue;
        const QString absDir = (!QDir::isRelativePath(dir) || isResourcePath(dir))
                                   ? dir
                                   : QDir(settingsDir).filePath(dir);
        const QString path = QDir(absDir).filePath(fileName);
        if (QFile::exists(path))
            result.append(path);
    }
    return result;
}

} // namespace dsqt::settings
