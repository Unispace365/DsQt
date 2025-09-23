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
        return create(QUuid::createUuid().toString(QUuid::Id128), parent);
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
        auto ptr = new RwContentModel(QUuid::createUuid().toString(QUuid::Id128), parent);
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
    static RwContentModel* createOrUpdate(const QVariantHash& props) {
        if (auto ptr = find(props["uid"].toString()); ptr) {
            ptr->setProperties(props);
			return ptr;
        } else
            return create(props);
    }


    /**
     * @brief Finds a RwContentModel by its UID in the current thread's lookup table.
     * @param uid The unique identifier to search for.
     * @return Pointer to the RwContentModel if found, nullptr otherwise.
     */
    static RwContentModel* find(const QString& uid) {
        auto&      lookup = RwContentLookup::get();
        const auto itr    = lookup.constFind(uid);
        return itr == lookup.constEnd() ? nullptr : itr.value();
    }

    /**
     * @brief Finds multiple RwContentModel instances by their UIDs.
     * @param uids The list of unique identifiers to search for.
     * @return QObjectList containing pointers to found RwContentModel instances.
     */
    static QList<RwContentModel*> find(const QStringList& uids) {
        QList<RwContentModel*> result;

        auto& lookup = RwContentLookup::get();
        for (const auto& uid : uids) {
            const auto itr = lookup.constFind(uid);
            if (itr != lookup.constEnd()) result.append(itr.value());
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
            if (itr != lookup.constEnd() && itr.value()) {
                // qInfo() << "Linking" << record->getProperty<QString>("record_name") << "to"
                //         << itr.value()->getProperty<QString>("record_name");
                record->setParent(itr.value());
            }
        }
        // Sort children by order.
        for (const auto& [uid, record] : lookup.asKeyValueRange()) {
            record->sortChildren();

            // Populate "children" property, as a QObject or QQmlPropertyMap does not by default allow access.
            QList<RwContentModel*> list;
            for (auto child : record->children()) {
                if (auto ptr = dynamic_cast<RwContentModel*>(child); ptr) list.append(ptr);
            }
            record->setProperty("children", QVariant::fromValue(list));
        }
    }

    // Removes all records that are not listed in the provided keys.
    static void cleanUp(const QStringList& keys) {
        auto& lookup = RwContentLookup::get();
        for (auto itr = lookup.begin(); itr != lookup.end(); ++itr) {
            if (!keys.contains(itr.key())) itr.value()->deleteLater();
        }
    }

    /**
     * @brief Deleted default constructor to prevent direct instantiation, e.g. "RwContentModel model;"
     */
    RwContentModel(QObject* parent = nullptr) = delete;

    QString getId() const { return property("uid").toString(); }

    QString getName() const { return property("record_name").toString(); }

    // /**
    //  * @brief Performs a deep comparison between this and another RwContentModel.
    //  * @param other The other RwContentModel to compare against.
    //  * @return True if the models are equal (excluding UID), false otherwise.
    //  */
    // bool isEqualTo(const RwContentModel* other) const {
    //     if (!other) return false;
    //     if (other == this) return true;
    //     if (size() != other->size()) return false;

    //     const auto names = keys();
    //     if (names != other->keys()) return false; // Assumes sorted.

    //     for (const auto& key : names) {
    //         if (key == "uid") continue; // Skip uid, as it will always be different.

    //         const auto val = value(key);
    //         const auto ptr = qvariant_cast<RwContentModel*>(val);
    //         if (ptr) {
    //             const auto theirs = qvariant_cast<RwContentModel*>(other->value(key));
    //             if (ptr == theirs) continue;
    //             if (!ptr->isEqualTo(theirs)) return false;
    //         } else if (val != other->value(key))
    //             return false;
    //     }

    //     return true;
    // }

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
    void setProperty(const QString& key, const QVariant& val) { insert(key, val); }

    /**
     * @brief Helper to set multiple properties from a QVariantHash.
     * @param props The properties to set.
     */
    void setProperties(const QVariantHash& props) {
        for (auto prop : props.asKeyValueRange()) {
            insert(prop.first, prop.second); // Emits valueChanged signal.
        }
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

    RwContentModel* getChildByName(const char* name, const QString& prop = "record_name") const {
        return getChildByName(QString(name), prop);
    }

    // Sorts children based on record order from database.
    void sortChildren() {
        std::stable_sort(d_ptr->children.begin(), d_ptr->children.end(), [](QObject* a, QObject* b) {
            RwContentModel* ptrA = dynamic_cast<RwContentModel*>(a);
            RwContentModel* ptrB = dynamic_cast<RwContentModel*>(b);
            if (!ptrA || !ptrB) return false;
            int rankA = ptrA->getProperty<int>("rank");
            int rankB = ptrB->getProperty<int>("rank");
            return rankA < rankB;
        });
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

    /**
     * @brief Destructor that removes the instance from the lookup table. Private, because lifetime is entirely managed
     * by the lookup table.
     */
    ~RwContentModel() {
        auto& lookup = RwContentLookup::get();
        lookup.remove(property("uid").toString());
    }
};

} // namespace dsqt::rework

// Make the content model available to standard stream operators
std::ostream& operator<<(std::ostream& os, const dsqt::rework::RwContentModel* o);

#endif
