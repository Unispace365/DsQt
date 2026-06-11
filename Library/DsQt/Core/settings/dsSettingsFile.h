#pragma once

#include <functional>
#include <type_traits>

#include <QColor>
#include <QDateTime>
#include <QPointF>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QReadWriteLock>
#include <QQuaternion>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace dsqt {

class Settings;

// Internal helper that holds a std::function callback as a QObject slot.
// Parented to the bind() context so it is destroyed when the context is destroyed.
// Using a stable QObject pointer as the slot allows Qt::UniqueConnection to
// deduplicate connections per (signal, SettingsBinding*) pair.
// The callback can be replaced without reconnecting the signal.
class SettingsBinding : public QObject
{
    Q_OBJECT
public:
    explicit SettingsBinding(QObject *parent)
        : QObject(parent)
    {}

    void setCallback(std::function<void(QVariant)> cb) { m_cb = std::move(cb); }

public slots:
    void onChange(const QString &, const QVariant &v)
    {
        if (m_cb)
            m_cb(v);
    }

private:
    std::function<void(QVariant)> m_cb;
};

class SettingsFile : public QQmlPropertyMap
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(Settings *manager READ manager WRITE setManager NOTIFY managerChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(
        QStringList searchPaths READ searchPaths WRITE setSearchPaths NOTIFY searchPathsChanged)

public:
    // Constructs a SettingsFile with an optional parent and initial search paths.
    explicit SettingsFile(QObject *parent = nullptr, const QStringList &searchPaths = {});

    // Sets the Settings instance that provides shared search paths and file watching.
    void setManager(Settings *manager);
    // Returns the Settings instance managing this file, or nullptr if standalone.
    Settings *manager() const;

    // Sets the TOML filename to load, resolved across the search paths.
    void setFileName(const QString &fileName);
    // Returns the TOML filename.
    QString fileName() const;

    // Sets per-instance search paths, overriding the manager's shared paths for this file.
    void setSearchPaths(const QStringList &paths);
    // Returns the per-instance search paths, or an empty list if using the manager's paths.
    QStringList searchPaths() const;

    // Returns the value at `key` as a QString, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getString(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QString>(key, def);
    }
    // Returns the value at `key` as an int, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getInt(const QString &key, const QVariant &def = {}) const
    {
        return getAs<int>(key, def);
    }
    // Returns the value at `key` as a double, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getFloat(const QString &key, const QVariant &def = {}) const
    {
        return getAs<double>(key, def);
    }
    // Returns the value at `key` as a bool, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getBool(const QString &key, const QVariant &def = {}) const
    {
        return getAs<bool>(key, def);
    }
    // Returns the value at `key` as a QDate, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getDate(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QDate>(key, def);
    }
    // Returns the value at `key` as a QTime, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getTime(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QTime>(key, def);
    }
    // Returns the value at `key` as a QDateTime, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getDateTime(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QDateTime>(key, def);
    }
    // Returns the value at `key` as a QColor, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getColor(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QColor>(key, def);
    }
    // Returns the value at `key` as a QPointF, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getPoint(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QPointF>(key, def);
    }
    // Returns the value at `key` as a QSizeF, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getSize(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QSizeF>(key, def);
    }
    // Returns the value at `key` as a QRectF, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getRect(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QRectF>(key, def);
    }
    // Returns the value at `key` as a QVector3D, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getVec3(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QVector3D>(key, def);
    }
    // Returns the value at `key` as a QVector4D, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getVec4(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QVector4D>(key, def);
    }
    // Returns the value at `key` as a QQuaternion, or def if absent or unconvertible.
    Q_INVOKABLE QVariant getQuat(const QString &key, const QVariant &def = {}) const
    {
        return getAs<QQuaternion>(key, def);
    }
    // Returns the value at `key` as a QVariantList, or def if absent or unconvertible.
    Q_INVOKABLE QVariantList getList(const QString &key, const QVariant &def = {}) const
    {
        return find<QVariantList>(key, def.toList());
    }
    // Returns the value at `key` as a QVariantMap, or def if absent or unconvertible.
    Q_INVOKABLE QVariantMap getObj(const QString &key, const QVariant &def = {}) const
    {
        return find<QVariantMap>(key, def.toMap());
    }

    // Reloads all resolved files from disk and rebuilds the merged settings.
    Q_INVOKABLE void reload();

    // Returns the raw QVariant at the dot-separated key, or defaultValue if absent.
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = {}) const;
    // Returns true if the dot-separated key exists in the merged settings.
    Q_INVOKABLE bool contains(const QString &key) const { return value(key).isValid(); }
    // Returns the merged, reference-resolved settings as a nested QVariantMap.
    // Safe to call from any thread.
    Q_INVOKABLE QVariantMap allSettings() const;
    // Returns the source of `key`: a file path if it came from a loaded file,
    // "default" if it was registered via setDefault(), "override" if set via setOverride(),
    // or an empty string if the key is not present.
    Q_INVOKABLE QString provenance(const QString &key) const;

    // Alias of find<T>() with a default-constructed fallback. Kept for DsSettings compatibility.
    template<typename T>
    T get(const QString &key) const
    {
        return find<T>(key);
    }

    // Alias of find<T>(). Kept for DsSettings compatibility.
    template<typename T>
    T getOr(const QString &key, const T &defaultValue) const
    {
        return find<T>(key, defaultValue);
    }

    // Returns the value at the dot-separated key converted to T, or defaultValue if absent.
    // Safe to call from any thread.
    template<typename T>
    T find(const QString &key, const T &defaultValue = {}) const
    {
        const QVariant v = value(key);
        if (!v.isValid() || !v.canConvert<T>())
            return defaultValue;
        return v.value<T>();
    }

    // Registers a default for key so that if it is absent after a reload, this value is used.
    // Also creates the QQmlPropertyMap path immediately so bind() can be established before
    // the file is loaded.
    template<typename T>
    void setDefault(const QString &key, const T &defaultValue)
    {
        ensurePath(key, QVariant::fromValue(defaultValue));
    }

    // Sets a runtime override for `key`. Overrides sit above defaults and file values
    // in the merge order (defaults → file data → overrides) and survive reload().
    template<typename T>
    void set(const QString &key, const T &value)
    {
        setOverride(key, QVariant::fromValue(value));
    }

    // Sets a runtime override for `key` (QVariant form, callable from QML).
    Q_INVOKABLE void setOverride(const QString &key, const QVariant &value);
    // Removes the runtime override for `key`, restoring the file or default value.
    Q_INVOKABLE void resetOverride(const QString &key);
    // Removes all runtime overrides.
    Q_INVOKABLE void resetOverrides();

    // Returns the current override map (all keys set via setOverride / set()).
    QVariantMap overrides() const { return m_overrides; }

    // Saves the current overrides to filePath as TOML.
    // If filePath already exists, only overridden keys are updated or inserted;
    // all other content in the file is preserved.
    // If filePath does not exist, a new file containing only the overrides is written.
    // The in-memory overrides are kept after the save (subsequent saves include them).
    // Returns an empty string on success, or an error description on failure.
    // Note: toml++ does not preserve comments when round-tripping an existing file.
    Q_INVOKABLE QString saveOverridesTo(const QString &filePath) const;

    // Calls callback immediately with the current value of key, then again on every change.
    // Re-registering with the same key and context replaces the existing binding.
    // The connection is removed when context is destroyed.
    template<typename T, typename Context, typename Func>
    void bind(const QString &key, Context *context, Func &&callback, const T &defaultValue = {})
    {
        // Ensure the full key path exists in the QQmlPropertyMap tree,
        // inserting intermediate maps and the leaf default as needed.
        setDefault<T>(key, defaultValue);

        // Walk the QQmlPropertyMap tree to the parent of the leaf key.
        QQmlPropertyMap *map = this;
        const QStringList parts = key.split(u'.');
        for (int i = 0; i < parts.size() - 1; ++i)
            map = qvariant_cast<QQmlPropertyMap *>(map->value(parts[i]));
        const QString leafKey = parts.last();

        // Reuse or create a SettingsBinding child on context, keyed by (map, leafKey).
        // This gives Qt::UniqueConnection a stable slot to deduplicate against.
        const QByteArray bindingName = QByteArray("__sb_")
                                       + QByteArray::number(reinterpret_cast<quintptr>(map)) + "_"
                                       + leafKey.toUtf8();

        auto *binding = context->template findChild<SettingsBinding *>(QString::fromLatin1(
                                                                           bindingName),
                                                                       Qt::FindDirectChildrenOnly);

        if (!binding) {
            binding = new SettingsBinding(context);
            binding->setObjectName(QString::fromLatin1(bindingName));
            QObject::connect(map,
                             &QQmlPropertyMap::valueChanged,
                             binding,
                             &SettingsBinding::onChange,
                             Qt::UniqueConnection);
        }

        // Replace (or set) the callback — the signal connection is reused.
        // Member function pointers are invoked with context; plain callables are called directly.
        binding->setCallback([cb = std::forward<Func>(callback), ctx = context](const QVariant &v) {
            if constexpr (std::is_member_function_pointer_v<std::decay_t<Func>>)
                std::invoke(cb, ctx, v.value<T>());
            else
                cb(v.value<T>());
        });

        // Call immediately with the current value.
        if constexpr (std::is_member_function_pointer_v<std::decay_t<Func>>)
            std::invoke(callback, context, find<T>(key, defaultValue));
        else
            callback(find<T>(key, defaultValue));
    }

signals:
    void managerChanged();
    void fileNameChanged();
    void searchPathsChanged();
    // Emitted at the end of every rebuild() — fires for file reloads, setOverride,
    // resetOverride, and ensurePath. Use this instead of valueChanged when you need
    // to know that *any* value (including deeply nested ones) may have changed.
    void settingsRebuilt();

private:
    // Returns the value at `key` converted to T (wrapped in a QVariant), or `def`
    // unchanged if the key is absent or the value cannot be converted to T.
    // Returning `def` as-is mirrors the old DsSettingsProxy getters: whatever the
    // QML caller passed as default comes back untouched.
    template<typename T>
    QVariant getAs(const QString &key, const QVariant &def) const
    {
        const QVariant v = value(key);
        if (!v.isValid() || !v.canConvert<T>())
            return def;
        return QVariant::fromValue(v.value<T>());
    }

    void deepMerge(QVariantMap &base,
                   const QVariantMap &overlay,
                   int fileIndex = -1,
                   const QString &prefix = {});
    void syncMap(QQmlPropertyMap *target, const QVariantMap &newData);
    void ensurePath(const QString &key, const QVariant &defaultValue);
    void rebuild();

    Settings *m_manager = nullptr;
    QString m_fileName;
    QStringList m_searchPaths;
    QVariantMap m_defaults;
    QVariantMap m_settings;
    QVariantMap m_overrides;
    QVariantMap m_merged;
    mutable QReadWriteLock m_mergedLock; // guards m_merged for cross-thread reads
    QStringList m_files;
    QMap<QString, int> m_provenance;
};

}
