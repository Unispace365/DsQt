#include "settings/dsSettings.h"
#include "settings/dsSettingsFile.h"
#include "settings/dsSettingsUtils.h"
#include "core/dsEnvironment.h"

#include <QCoreApplication>
#include <QThread>

namespace dsqt {

// ── Settings ───────────────────────────────────────────────────────────────

// Instantiate the singleton on the main thread right after QCoreApplication is
// constructed, before any worker thread can win the first-call race in
// instance(). The singleton's thread affinity (and thus where the file watcher
// delivers its signals) is decided by that first call.
static void createSettingsSingleton()
{
    Settings::instance();
}
Q_COREAPP_STARTUP_FUNCTION(createSettingsSingleton)

Settings::Settings()
    : QQmlPropertyMap(this, nullptr)
{
    // Construction off the main thread would give the watcher and all QML
    // interaction the wrong thread affinity. This is a startup programming
    // error, so fail fast — also in Release builds.
    if (!QCoreApplication::instance()
        || QThread::currentThread() != QCoreApplication::instance()->thread())
        qFatal("Settings must be instantiated on the main thread, "
               "after QCoreApplication has been created.");

    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &Settings::onFileChanged);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Settings::onDirectoryChanged);
}

QStringList Settings::searchPaths() const
{
    return m_searchPaths;
}

void Settings::setSearchPaths(QStringList paths) {
    settings::checkThread(this, Q_FUNC_INFO);

    // Expand and remove duplicates.
    for (auto& path : paths)
        path = DsEnvironment::expandq(path);
    paths.removeDuplicates();

    // Also remove paths that failed to expand.
    paths.removeIf([](const QString &s) { return s.contains('%'); });

    if (m_searchPaths == paths)
        return;
    m_searchPaths = paths;

    // All registered instances must reload with the new paths.
    for (SettingsFile *s : std::as_const(m_instances))
        s->reload();

    rebuildWatcher();
    emit searchPathsChanged();
}

bool Settings::hasSearchPath(const QString& path) const {
    return m_searchPaths.contains(path);
}

void Settings::registerSettingsFile(SettingsFile *s)
{
    if (m_instances.contains(s))
        return;

    // Clean up automatically if the SettingsFile object is destroyed.
    connect(s, &QObject::destroyed, this, [this, s] { unregisterSettingsFile(s); });

    // When a SettingsFile instance changes its fileName, the set of watched files changes.
    connect(s, &SettingsFile::fileNameChanged, this, [this] { rebuildWatcher(); });

    m_instances.append(s);
    rebuildWatcher();
}

void Settings::unregisterSettingsFile(SettingsFile *s)
{
    m_instances.removeOne(s);
    emit instancesChanged();

    rebuildWatcher();
}

QStringList Settings::resolveFilePathsImpl(const QString &fileName) const
{
    return settings::resolveFilePaths(fileName, m_searchPaths);
}

QStringList Settings::settingsNames() const
{
    QReadLocker locker(&m_namedLock);
    return m_named.keys();
}

SettingsFile *Settings::settingsFile(const QString &name) const
{
    QReadLocker locker(&m_namedLock);
    return m_named.value(name, nullptr);
}

void Settings::forgetImpl(const QString &name)
{
    settings::checkThread(this, Q_FUNC_INFO);
    auto *sf = qvariant_cast<SettingsFile *>(value(name));
    if (!sf)
        return;
    // Remove from m_named under the write lock so any concurrent findImpl()
    // holding the read lock finishes before we proceed, and new lookups see
    // nullptr immediately. Delete sf only after releasing the lock — deleting
    // inside the lock would extend the write-lock window unnecessarily.
    {
        QWriteLocker locker(&m_namedLock);
        m_named.remove(name);
    }
    insert(name, QVariant{}); // clear the entry in the property map
    delete sf;                // triggers destroyed signal → unregisterSettingsFile
}

void Settings::addImpl(const QString &name, const QString &fileName)
{
    settings::checkThread(this, Q_FUNC_INFO);
    // If a SettingsFile already exists under this name, just update its fileName.
    if (auto *existing = qvariant_cast<SettingsFile *>(value(name))) {
        existing->setFileName(fileName);
        return;
    }

    // Otherwise create a new SettingsFile, wire it up, and expose it as a property.
    auto *sf = new SettingsFile(this);
    sf->setManager(this);
    sf->setFileName(fileName);
    insert(name, QVariant::fromValue(sf));
    {
        QWriteLocker locker(&m_namedLock);
        m_named[name] = sf;
    }
    emit instancesChanged();
}

void Settings::onFileChanged(const QString &path)
{
    // Some platforms (e.g. Linux inotify) stop watching a file after it is replaced
    // rather than edited in-place. Re-add it so we keep receiving changes.
    if (QFile::exists(path) && !m_watcher.files().contains(path))
        m_watcher.addPath(path);

    // Reload only the SettingsFile instances whose resolved file paths include this file.
    for (SettingsFile *s : std::as_const(m_instances)) {
        if (resolveFilePathsImpl(s->fileName()).contains(path))
            s->reload();
    }
}

void Settings::onDirectoryChanged(const QString &)
{
    // A file may have been created in one of the watched directories.
    // Reload any instance whose resolved paths have changed, and rebuild
    // the watcher so newly-created files are watched directly.
    for (SettingsFile *s : std::as_const(m_instances))
        s->reload();

    rebuildWatcher();
}

void Settings::rebuildWatcher()
{
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());

    for (SettingsFile *s : std::as_const(m_instances)) {
        for (const QString &file : resolveFilePathsImpl(s->fileName())) {
            if (!settings::isResourcePath(file))
                m_watcher.addPath(file);
        }
    }

    // Always watch the search path directories so we detect file creation.
    for (const QString &dir : std::as_const(m_searchPaths)) {
        if (!settings::isResourcePath(dir))
            m_watcher.addPath(dir);
    }
}

}
