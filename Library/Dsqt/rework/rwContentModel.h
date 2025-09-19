/**
 * @file rwContentModel.h
 * @brief Header file for the RwContentModel and RwContentLookup classes.
 *
 * This file defines the RwContentModel class, which extends QQmlPropertyMap to manage content properties with unique
 * identifiers, and the RwContentLookup class, which provides thread-safe storage and retrieval of RwContentModel
 * instances.
 */

#ifndef RWCONTENTMODEL_H
#define RWCONTENTMODEL_H

#include <QMutex>
#include <QQmlPropertyMap>
#include <QThread>
#include <QUuid>

namespace dsqt::rework {

class RwContentModel;

/**
 * @class RwContentLookup
 * @brief A thread-safe lookup table for managing RwContentModel instances.
 *
 * This class provides static methods to retrieve and destroy RwContentModel instances in a thread-safe way.
 */
class RwContentLookup {
  public:
    /**
     * @brief Retrieves the lookup table for the current thread.
     * @return Reference to the QHash mapping UIDs to RwContentModel pointers for the current thread.
     */
    static QHash<QString, RwContentModel*>& get() { return get(QThread::currentThreadId()); }

    /**
     * @brief Retrieves the lookup table for a specified thread ID.
     * @param threadId The thread ID for which to retrieve the lookup table.
     * @return Reference to the QHash mapping UIDs to RwContentModel pointers for the specified thread.
     */
    static QHash<QString, RwContentModel*>& get(Qt::HANDLE threadId);

    /**
     * @brief Destroys all RwContentModel instances for the current thread.
     */
    static void destroy() { destroy(QThread::currentThreadId()); }

    /**
     * @brief Destroys all RwContentModel instances for a specified thread ID.
     * @param threadId The thread ID for which to destroy the lookup table.
     */
    static void destroy(Qt::HANDLE threadId);

  private:
    /// @brief Mutex for thread-safe access to the lookup table.
    inline static QMutex s_mutex{};
    /// @brief Lookup table mapping thread IDs to QHash of UIDs and RwContentModel pointers.
    inline static QHash<Qt::HANDLE, QHash<QString, RwContentModel*>> s_lookup{};
};

/**
 * @class RwContentModel
 * @brief A property map for managing content with a unique identifier.
 *
 * This class extends QQmlPropertyMap to store content properties and provides methods
 * for creating, finding, and linking instances, with automatic registration in the RwContentLookup.
 */
class RwContentModel : public QQmlPropertyMap {
  public:
    /**
     * @brief Creates a new RwContentModel with a generated UUID.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created RwContentModel instance.
     */
    static RwContentModel* create(QObject* parent = nullptr) {
        return new RwContentModel(QUuid::createUuid().toString(QUuid::WithoutBraces), parent);
    }

    /**
     * @brief Creates a new RwContentModel with a specified UID.
     * @param uid The unique identifier for the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created RwContentModel instance.
     */
    static RwContentModel* create(const QString& uid, QObject* parent = nullptr) {
        return new RwContentModel(uid, parent);
    }

    /**
     * @brief Creates a new RwContentModel with specified properties.
     * @param props The initial properties to set on the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created RwContentModel instance.
     */
    static RwContentModel* create(const QVariantHash& props, QObject* parent = nullptr) {
        return new RwContentModel(props, parent);
    }

    /**
     * @brief Creates a new RwContentModel with a name and generated UUID.
     * @param name The name property to set on the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created RwContentModel instance.
     */
    static RwContentModel* createNamed(const QString& name, QObject* parent = nullptr) {
        auto ptr = new RwContentModel(QUuid::createUuid().toString(QUuid::WithoutBraces), parent);
        ptr->setProperty("name", name);
        return ptr;
    }

    /**
     * @brief Creates a new RwContentModel with a name and specified UID.
     * @param name The name property to set on the model.
     * @param uid The unique identifier for the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created RwContentModel instance.
     */
    static RwContentModel* createNamed(const QString& name, const QString& uid, QObject* parent = nullptr) {
        auto ptr = new RwContentModel(uid, parent);
        ptr->setProperty("name", name);
        return ptr;
    }

    /**
     * @brief Creates or updates a RwContentModel with specified properties.
     * @param props The properties to set or update, including the UID.
     */
    static void createOrUpdate(const QVariantHash& props) {
        if (auto ptr = find(props["uid"].toString()); ptr) {
            ptr->setProperties(props);
        } else
            create(props);
    }

    /**
     * @brief Finds a RwContentModel by its UID in the current thread's lookup table.
     * @param uid The unique identifier to search for.
     * @return Pointer to the RwContentModel if found, nullptr otherwise.
     */
    static RwContentModel* find(const QString& uid) {
        auto&      lookup = RwContentLookup::get();
        const auto itr    = lookup.constFind(uid);
        return itr == lookup.constEnd() ? nullptr : *itr;
    }

    /**
     * @brief Finds multiple RwContentModel instances by their UIDs.
     * @param uids The list of unique identifiers to search for.
     * @return QObjectList containing pointers to found RwContentModel instances.
     */
    static QObjectList find(const QStringList& uids) {
        QObjectList result;

        auto& lookup = RwContentLookup::get();
        for (const auto& uid : uids) {
            const auto itr = lookup.constFind(uid);
            if (itr != lookup.constEnd()) result.append(*itr);
        }

        return result;
    }

    /**
     * @brief Links RwContentModel instances based on their parent_uid properties.
     *
     * Iterates through the lookup table and sets the parent of each model to the
     * instance corresponding to its parent_uid, if found.
     */
    static void linkUp() {
        auto& lookup = RwContentLookup::get();
        // Set parent for all nodes.
        for (const auto& [uid, record] : lookup.asKeyValueRange()) {
            if (!record->contains("parent_uid")) continue;
            const auto parent_uids = record->getProperty<QStringList>("parent_uid");
            if (parent_uids.size() != 1) continue; // Skip records with multiple parents, or no parent at all.
            const auto itr = lookup.constFind(parent_uids.front());
            if (itr != lookup.constEnd()) {
                // qInfo() << "Linking" << record->getProperty<QString>("record_name") << "to" <<
                // itr.value()->getProperty<QString>("record_name");
                record->setParent(itr.value());
            }
        }
        // TODO sort child_uid by order.
        // Populate "children" property, as a QObject or QQmlPropertyMap does not by default allow access.
        for (const auto& [uid, record] : lookup.asKeyValueRange()) {
            QList<RwContentModel*> list;

            const auto child_uids = record->getProperty<QStringList>("child_uid");
            for (const auto& child_uid : child_uids)
                list.append(lookup.find(child_uid).value());
            record->setProperty("children", QVariant::fromValue(list));
        }
    }

    /**
     * @brief Deleted default constructor to prevent direct instantiation, e.g. "RwContentModel model;"
     */
    RwContentModel(QObject* parent = nullptr) = delete;

    /**
     * @brief Destructor that removes the instance from the lookup table.
     */
    ~RwContentModel() {
        auto& lookup = RwContentLookup::get();
        lookup.remove(property("uid").toString());
    }

    /**
     * @brief Performs a deep comparison between this and another RwContentModel.
     * @param other The other RwContentModel to compare against.
     * @return True if the models are equal (excluding UID), false otherwise.
     */
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

    /**
     * @brief Helper to retrieve a property value by key, cast to the specified type.
     * @tparam T The type to cast the property value to.
     * @param key The property key.
     * @return The property value cast to type T.
     */
    template <typename T>
    T getProperty(const char* key) const {
        return property(key).value<T>();
    }

    /**
     * @brief Helper to retrieve a property value by QString key, cast to the specified type.
     * @tparam T The type to cast the property value to.
     * @param key The property key.
     * @return The property value cast to type T.
     */
    template <typename T>
    T getProperty(const QString& key) const {
        return value(key).value<T>();
    }

    /**
     * @brief Helper to set a property value by key.
     * @param key The property key.
     * @param val The value to set.
     */
    void setProperty(const QString& key, const QVariant& val) {
        if (!contains(key) || value(key) != val) {
            insert(key, val);
            emit valueChanged(key, val);
        }
    }

    /**
     * @brief Helper to set multiple properties from a QVariantHash.
     * @param props The properties to set.
     */
    void setProperties(const QVariantHash& props) {
        for (auto prop : props.asKeyValueRange()) {
            if (!contains(prop.first) || value(prop.first) != prop.second) {
                insert(prop.first, prop.second);
                emit valueChanged(prop.first, prop.second);
            }
        }
        //  insert(props);
    }

    // Uses the lookup table.
    QList<RwContentModel*> getParents() const {
        QList<RwContentModel*> result;
        if (contains("parent_uid")) {
            const auto parents = value("parent_uid").toStringList();
            for (const auto& parent : parents) {
                auto node = find(parent.trimmed());
                if (node) result.append(node);
            }
        }
        return result;
    }

    // Uses the lookup table.
    QList<RwContentModel*> getChildren() const {
        QList<RwContentModel*> result;
        if (contains("child_uid")) {
            const auto parents = value("child_uid").toStringList();
            for (const auto& parent : parents) {
                auto node = find(parent.trimmed());
                if (node) result.append(node);
            }
        }
        return result;
    }

    // Does not use the lookup table.
    RwContentModel* getChild(qsizetype index) const {
        for (auto child : children()) {
            auto model = dynamic_cast<RwContentModel*>(child);
            if (model) {
                if (!index) return model;
                --index;
            }
        }
        return nullptr;
    }

    // Does not use the lookup table.
    RwContentModel* getChildByName(QStringView name, const QString& prop = "record_name") const {
        qsizetype idx = name.indexOf('.');
        if (idx != -1) {
            auto left  = name.left(idx);
            auto right = name.right(idx);
            auto model = getChildByName(left);
            if (model) return model->getChildByName(right);
        } else {
            for (auto child : children()) {
                auto model = dynamic_cast<RwContentModel*>(child);
                if (model && model->value(prop).toString() == name) return model;
            }
        }
        return nullptr;
    }

  private:
    /**
     * @brief Private constructor for creating a model with a UID.
     * @param uid The unique identifier for the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @throws If the UID is empty or already exists in the lookup table.
     */
    RwContentModel(const QString& uid, QObject* parent = nullptr)
        : QQmlPropertyMap(this, parent) {
        insert("uid", uid);
        if (uid.isEmpty()) throw std::runtime_error("UID must be valid");
        auto& lookup = RwContentLookup::get();
        if (lookup.contains(uid)) throw std::runtime_error("UID must be unique");
        //qInfo() << "Adding record" << uid << "with name" << value("record_name");
        lookup.insert(uid, this);
    }

    /**
     * @brief Private constructor for creating a model with properties.
     * @param props The initial properties to set.
     * @param parent The parent QObject, defaults to nullptr.
     * @throws If the UID is empty or already exists in the lookup table.
     */
    RwContentModel(const QVariantHash& props, QObject* parent = nullptr)
        : QQmlPropertyMap(this, parent) {
        insert(props);
        if (!contains("uid")) throw std::runtime_error("UID must be specified");
        QString uid = property("uid").toString();
        if (uid.isEmpty()) throw std::runtime_error("UID must be valid");
        auto& lookup = RwContentLookup::get();
        if (lookup.contains(uid)) throw std::runtime_error("UID must be unique");
        //qInfo() << "Adding record" << uid << "with name" << value("record_name");
        lookup.insert(uid, this);
    }
};

} // namespace dsqt::rework

// Overload operator<< for QDebug
inline QDebug operator<<(QDebug debug, const dsqt::rework::RwContentModel& obj) {
    QDebugStateSaver saver(debug); // Preserves debug stream state
    debug.nospace() << "(" << obj.value("record_name") << "(" << obj.value("uid") << ")";
    return debug;
}

#endif
