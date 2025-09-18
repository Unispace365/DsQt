#ifndef RWCONTENTMODEL_H
#define RWCONTENTMODEL_H

#include <QMutex>
#include <QQmlPropertyMap>
#include <QThread>
#include <QUuid>

namespace dsqt::rework {

class RwContentModel;

class RwContentLookup {
  public:
    static QHash<QString, RwContentModel*>& get() { return get(QThread::currentThreadId()); }

    static QHash<QString, RwContentModel*>& get(Qt::HANDLE threadId);

    static void destroy() { destroy(QThread::currentThreadId()); }

    static void destroy(Qt::HANDLE threadId);

  private:
    inline static QMutex                                             s_mutex{};
    inline static QHash<Qt::HANDLE, QHash<QString, RwContentModel*>> s_lookup{};
};

class RwContentModel : public QQmlPropertyMap {
  public:
    static RwContentModel* create(QObject* parent = nullptr) {
        return new RwContentModel(QUuid::createUuid().toString(QUuid::WithoutBraces), parent);
    }

    static RwContentModel* create(const QString& uid, QObject* parent = nullptr) {
        return new RwContentModel(uid, parent);
    }

    static RwContentModel* create(const QVariantHash& props, QObject* parent = nullptr) {
        return new RwContentModel(props, parent);
    }

    static RwContentModel* createNamed(const QString& name, QObject* parent = nullptr) {
        auto ptr = new RwContentModel(QUuid::createUuid().toString(QUuid::WithoutBraces), parent);
        ptr->setProperty("name", name);
        return ptr;
    }

    static RwContentModel* createNamed(const QString& name, const QString& uid, QObject* parent = nullptr) {
        auto ptr = new RwContentModel(uid, parent);
        ptr->setProperty("name", name);
        return ptr;
    }

    static void createOrUpdate(const QVariantHash& props) {
        if (auto ptr = find(props["uid"].toString()); ptr) {
            ptr->setProperties(props);
        } else
            create(props);
    }

    static RwContentModel* find(const QString& uid) {
        auto&      lookup = RwContentLookup::get();
        const auto itr    = lookup.constFind(uid);
        return itr == lookup.constEnd() ? nullptr : *itr;
    }

    static QObjectList find(const QStringList& uids) {
        QObjectList result;

        auto& lookup = RwContentLookup::get();
        for (const auto& uid : uids) {
            const auto itr = lookup.constFind(uid);
            if (itr != lookup.constEnd()) result.append(*itr);
        }

        return result;
    }

    static void linkUp() {
        auto& lookup = RwContentLookup::get();
        for (const auto& [uid, record] : lookup.asKeyValueRange()) {
            const auto itr = lookup.constFind(record->getProperty<QString>("parent_uid"));
            if (itr != lookup.end()) record->setParent(itr.value());
        }
    }

    RwContentModel(QObject* parent = nullptr) = delete;
    ~RwContentModel() {
        // Remove from lookup table.
        auto& lookup = RwContentLookup::get();
        lookup.remove(property("uid").toString());
    }

    /// Performs a deep comparison. DO WE NEED THIS?
    bool isEqualTo(const RwContentModel* other) const {
        if (!other) return false;
        if (other == this) return true;
        if (size() != other->size()) return false;

        const auto names = keys();
        if (names != other->keys()) return false; // Assumes sorted.

        for (const auto& key : names) {
            if (key == "uid") continue; // Skip uid, as it will always be different.

            const auto val = value(key);
            const auto ptr = qvariant_cast<RwContentModel*>(val);
            if (ptr) {
                const auto theirs = qvariant_cast<RwContentModel*>(other->value(key));
                if (ptr == theirs) continue;
                if (!ptr->isEqualTo(theirs)) return false;
            } else if (val != other->value(key))
                return false;
        }

        return true;
    }

    template <typename T>
    T getProperty(const char* key) const {
        return property(key).value<T>();
    }

    template <typename T>
    T getProperty(const QString& key) const {
        return value(key).value<T>();
    }

    void setProperty(const QString& key, const QVariant& value) { insert(key, value); }

    void setProperties(const QVariantHash& props) { insert(props); }

  private:
    RwContentModel(const QString& uid, QObject* parent = nullptr)
        : QQmlPropertyMap(this, parent) {
        insert("uid", uid);
        // All content models should have a unique id.
        if (uid.isEmpty()) throw;
        // Lookup by id. If exists, throw.
        auto& lookup = RwContentLookup::get();
        if (lookup.contains(uid)) throw;
        // Add to lookup table.
        lookup.insert(uid, this);
    }
    RwContentModel(const QVariantHash& props, QObject* parent = nullptr)
        : QQmlPropertyMap(this, parent) {
        insert(props);
        // All content models should have a unique id. TODO use generator if uid is missing.
        QString uid = property("uid").toString();
        if (uid.isEmpty()) throw;
        // Lookup by id. If exists, throw.
        auto& lookup = RwContentLookup::get();
        if (lookup.contains(uid)) throw;
        // Add to lookup table.
        lookup.insert(uid, this);
    }
};

} // namespace dsqt::rework

#endif
