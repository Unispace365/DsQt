#pragma once

#include <functional>
#include <utility>

#include <QFileSystemWatcher>
#include <QMultiHash>
#include <QPointer>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QReadWriteLock>
#include <QStringList>
#include <QVariant>

#include "settings/dsSettingsFile.h"

namespace dsqt {

class Settings : public QQmlPropertyMap {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QStringList searchPaths READ searchPaths WRITE setSearchPaths NOTIFY searchPathsChanged)

  public:
    // Returns the singleton instance.
    static Settings& instance() {
        static Settings instance;
        return instance;
    }

    // QML singleton factory — returns the same instance as instance().
    static Settings* create(QQmlEngine*, QJSEngine*) { return &instance(); }

    // Returns the shared search directories, in priority order (lowest first).
    QStringList searchPaths() const;

    // Sets the shared search paths and triggers a reload of all registered SettingsFile
    // instances that have no per-instance paths of their own.
    void setSearchPaths(QStringList paths);

    //
    bool hasSearchPath(const QString &path) const;

    // Returns the names of all registered SettingsFile instances.
    Q_INVOKABLE QStringList settingsNames() const;
    // Returns the SettingsFile registered under `name`, or nullptr if not found.
    Q_INVOKABLE SettingsFile* settingsFile(const QString& name) const;
    //
    Q_INVOKABLE bool hasSettingsFile(const QString& name) const { return nullptr != settingsFile(name); }

    static QStringList resolveFilePaths(const QString& fileName) { return instance().resolveFilePathsImpl(fileName); }

    static void forget(const QString& name) { instance().forgetImpl(name); }
    static void add(const QString& name) { instance().addImpl(name); }
    static void add(const QString& name, const QString& fileName) { instance().addImpl(name, fileName); }

    // Safe to call from any thread. The read lock is held for the duration of
    // the lookup so a concurrent forgetImpl() cannot delete the SettingsFile
    // mid-read.
    template <typename T>
    static T find(const QString& name, const QString& key, const T& defaultValue = {}) {
        return instance().findImpl(name, key, defaultValue);
    }

    template <typename T, typename Context, typename Func>
    static void bind(const QString& name, const QString& key, Context* context, Func&& callback,
                     const T& defaultValue = {}) {
        instance().bindImpl(name, key, context, std::forward<Func>(callback), defaultValue);
    }

  signals:
    void searchPathsChanged();
    void instancesChanged();

  private:
    Settings();

    // Returns all existing file paths for fileName across the current search paths, in order.
    QStringList resolveFilePathsImpl(const QString& fileName) const;

    // Removes the named SettingsFile, stops watching its files, and deletes the object.
    // Does nothing if no file with that name is registered.
    void forgetImpl(const QString& name);

    // Registers a SettingsFile named `name`, loading `name`.toml from the search paths.
    void addImpl(const QString& name) { addImpl(name, name + ".toml"); }

    // Registers a SettingsFile named `name`, loading `fileName` from the search paths.
    // If a file named `name` is already registered, updates its fileName instead.
    void addImpl(const QString& name, const QString& fileName);

    // Returns the value at `key` in the named SettingsFile, converted to T.
    // Returns defaultValue if the file or key is not found.
    // The read lock is held across the sf->find() call so a concurrent
    // forgetImpl() (which needs the write lock) cannot delete sf mid-read.
    template <typename T>
    T findImpl(const QString& name, const QString& key, const T& defaultValue = {}) const {
        QReadLocker locker(&m_namedLock);
        const auto* sf = m_named.value(name, nullptr);
        if (!sf) return defaultValue;
        return sf->find(key, defaultValue);
    }

    // Calls callback immediately with the current value of `key` in the named SettingsFile,
    // then again whenever it changes. The connection is removed when context is destroyed.
    //
    // If no SettingsFile is registered under `name` yet, the callback is still called once
    // with defaultValue and the bind is parked in m_pendingBinds, to be replayed by addImpl()
    // as soon as the file appears. Callers therefore do not have to be constructed after the
    // settings load.
    template <typename T, typename Context, typename Func>
    void bindImpl(const QString& name, const QString& key, Context* context, Func&& callback,
                  const T& defaultValue = {}) const {
        if (auto* sf = qvariant_cast<SettingsFile*>(value(name))) {
            sf->bind<T>(key, context, std::forward<Func>(callback), defaultValue);
            return;
        }

        // Type-erase the bind so it can be replayed without knowing T at the call site.
        auto cb = std::forward<Func>(callback);
        m_pendingBinds.insert(name, {context, [key, cb, context, defaultValue](SettingsFile* sf) {
                                         sf->bind<T>(key, context, cb, defaultValue);
                                     }});

        qWarning("Settings::bind: no settings file registered for '%s' - binding '%s' is deferred "
                 "until one is added.",
                 qPrintable(name), qPrintable(key));

        // Honour bind()'s guarantee that the callback runs once, immediately.
        if constexpr (std::is_member_function_pointer_v<std::decay_t<Func>>)
            std::invoke(cb, context, defaultValue);
        else
            cb(defaultValue);
    }

    // Prevent accidental access to internal SettingsFile instances via value(); use find<T>(name, key) instead.
    using QQmlPropertyMap::value;

    // Prevent accidental modification of internal SettingsFile instances via insert/remove/clear; use add()/forget()
    // instead.
    using QQmlPropertyMap::clear;
    using QQmlPropertyMap::insert;

    // Called by SettingsFile when its manager is set or cleared.
    friend class SettingsFile;
    void registerSettingsFile(SettingsFile* s);
    void unregisterSettingsFile(SettingsFile* s);

    // Applies and discards every bind parked for `name`, skipping those whose context died.
    void flushPendingBinds(const QString& name, SettingsFile* sf);

    void onFileChanged(const QString& path);
    void onDirectoryChanged(const QString& path);
    void rebuildWatcher();

    /// Binds made before their settings file existed, keyed by file name.
    /// Each entry is the bind context (to detect it being destroyed while parked)
    /// and a replay function that performs the original SettingsFile::bind<T>().
    using PendingBind = std::pair<QPointer<QObject>, std::function<void(SettingsFile*)>>;
    mutable QMultiHash<QString, PendingBind> m_pendingBinds;

    QStringList                  m_searchPaths;
    QList<SettingsFile*>         m_instances;
    QMap<QString, SettingsFile*> m_named;
    mutable QReadWriteLock       m_namedLock; // guards m_named for cross-thread reads
    QFileSystemWatcher           m_watcher;
};

} // namespace dsqt
