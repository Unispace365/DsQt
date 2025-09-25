/**
 * @file rwContentModel.h
 * @brief Header file for the ContentModel and ContentLookup classes.
 *
 * This file defines the ContentModel class, which extends QQmlPropertyMap to manage content properties with unique
 * identifiers, and the ContentLookup class, which provides thread-safe storage and retrieval of ContentModel
 * instances.
 */

#ifndef RWCONTENTMODEL_H
#define RWCONTENTMODEL_H

#include <QMutex>
#include <QQmlPropertyMap>
#include <QThread>
#include <QUuid>

namespace dsqt::model {

class ContentModel;
using ContentModelList = QList<ContentModel*>;
using ContentModelHash = QHash<QString, ContentModel*>;

/**
 * @class ContentLookup
 * @brief A thread-safe lookup table for managing ContentModel instances.
 *
 * This class provides static methods to retrieve and destroy ContentModel instances in a thread-safe way.
 */
class ContentLookup {
  public:
    /**
     * @brief Retrieves the lookup table for the current thread.
     * @return Reference to the QHash mapping UIDs to ContentModel pointers for the current thread.
     */
    static ContentModelHash& get() { return get(QThread::currentThreadId()); }

    /**
     * @brief Retrieves the lookup table for a specified thread ID.
     * @param threadId The thread ID for which to retrieve the lookup table.
     * @return Reference to the QHash mapping UIDs to ContentModel pointers for the specified thread.
     */
    static ContentModelHash& get(Qt::HANDLE threadId);

    /**
     * @brief Destroys all ContentModel instances for the current thread.
     */
    static void destroy() { destroy(QThread::currentThreadId()); }

    /**
     * @brief Destroys all ContentModel instances for a specified thread ID.
     * @param threadId The thread ID for which to destroy the lookup table.
     */
    static void destroy(Qt::HANDLE threadId);

  private:
    /// @brief Mutex for thread-safe access to the lookup table.
    inline static QMutex s_mutex{};
    /// @brief Lookup table mapping thread IDs to QHash of UIDs and ContentModel pointers.
    inline static QHash<Qt::HANDLE, ContentModelHash> s_lookup{};
};

/**
 * @class ContentModel
 * @brief A property map for managing content with a unique identifier.
 *
 * This class extends QQmlPropertyMap to store content properties and provides methods
 * for creating, finding, and linking instances, with automatic registration in the ContentLookup.
 */
class ContentModel : public QQmlPropertyMap {
  public:
    /**
     * @brief Creates a new ContentModel with a generated UUID.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created ContentModel instance.
     */
    static ContentModel* create(QObject* parent = nullptr) {
        return create(QUuid::createUuid().toString(QUuid::Id128), parent);
    }

    /**
     * @brief Creates a new ContentModel with a specified UID.
     * @param uid The unique identifier for the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created ContentModel instance.
     */
    static ContentModel* create(const QString& uid, QObject* parent = nullptr) { return new ContentModel(uid, parent); }

    /**
     * @brief Creates a new ContentModel with specified properties.
     * @param props The initial properties to set on the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created ContentModel instance.
     */
    static ContentModel* create(const QVariantHash& props, QObject* parent = nullptr) {
        return new ContentModel(props, parent);
    }

    /**
     * @brief Creates a new ContentModel with a name and generated UUID.
     * @param name The name property to set on the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created ContentModel instance.
     */
    static ContentModel* createNamed(const QString& name, QObject* parent = nullptr) {
        auto ptr = new ContentModel(QUuid::createUuid().toString(QUuid::Id128), parent);
        ptr->setProperty("record_name", name);
        return ptr;
    }

    /**
     * @brief Creates a new ContentModel with a name and specified UID.
     * @param name The name property to set on the model.
     * @param uid The unique identifier for the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @return Pointer to the created ContentModel instance.
     */
    static ContentModel* createNamed(const QString& name, const QString& uid, QObject* parent = nullptr) {
        auto ptr = new ContentModel(uid, parent);
        ptr->setProperty("record_name", name);
        return ptr;
    }

    /**
     * @brief Creates or updates a ContentModel with specified properties.
     * @param props The properties to set or update, including the UID.
     */
    static ContentModel* createOrUpdate(const QVariantHash& props) {
        if (auto ptr = find(props["uid"].toString()); ptr) {
            ptr->setProperties(props);
            return ptr;
        } else
            return create(props);
    }


    /**
     * @brief Finds a ContentModel by its UID in the current thread's lookup table.
     * @param uid The unique identifier to search for.
     * @return Pointer to the ContentModel if found, nullptr otherwise.
     */
    static ContentModel* find(const QString& uid) {
        auto&      lookup = ContentLookup::get();
        const auto itr    = lookup.constFind(uid);
        return itr == lookup.constEnd() ? nullptr : itr.value();
    }

    /**
     * @brief Finds multiple ContentModel instances by their UIDs.
     * @param uids The list of unique identifiers to search for.
     * @return QObjectList containing pointers to found ContentModel instances.
     */
    static ContentModelList find(const QStringList& uids) {
        ContentModelList result;

        auto& lookup = ContentLookup::get();
        for (const auto& uid : uids) {
            const auto itr = lookup.constFind(uid);
            if (itr != lookup.constEnd()) result.append(itr.value());
        }

        return result;
    }

    /**
     * @brief Links ContentModel instances based on their parent_uid properties.
     *
     * Iterates through the lookup table and sets the parent of each model to the
     * instance corresponding to its parent_uid, if found.
     */
    static void linkUp() {
        auto& lookup = ContentLookup::get();

        for (const auto& [uid, record] : lookup.asKeyValueRange()) {
            // Check if parent specified.
            if (!record->contains("parent_uid")) {
                record->setParent(nullptr);
                continue;
            }

            const auto parent_uids = record->getProperty<QStringList>("parent_uid");
            for (const auto& parent_uid : parent_uids) {
                auto parent = lookup.find(parent_uid).value();
                if (parent) {
                    // Link records with a single parent to the parent node.
                    if (parent_uids.size() == 1) record->setParent(parent);
                    // Always add child uid to parent's list of childs.
                    auto childs = parent->value("child_uid").toStringList();
                    if (!childs.contains(uid)) childs.append(uid);
                    parent->insert("child_uid", childs);
                }
            }
        }

        // Sort children by order.
        for (const auto& [uid, record] : lookup.asKeyValueRange()) {
            record->sortChildren();

            // Populate "children" property, as a QObject or QQmlPropertyMap does not by default allow access.
            ContentModelList list;
            for (auto child : record->children()) {
                if (auto ptr = dynamic_cast<ContentModel*>(child); ptr) list.append(ptr);
            }
            record->setProperty("children", QVariant::fromValue(list));
        }
    }

    // Removes all records that are not listed in the provided keys.
    static void cleanUp(const QStringList& keys) {
        auto& lookup = ContentLookup::get();
        for (auto itr = lookup.begin(); itr != lookup.end(); ++itr) {
            if (itr.key().length() > 12) continue; // Ignore records using Id128, as they are created externally.
            if (!keys.contains(itr.key())) itr.value()->deleteLater();
        }
    }

    /**
     * @brief Deleted default constructor to prevent direct instantiation, e.g. "ContentModel model;"
     */
    ContentModel(QObject* parent = nullptr) = delete;

    QString getId() const { return property("uid").toString(); }

    QString getName() const { return property("record_name").toString(); }

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
        insert(key, val); // Emits valueChanged signal.
    }

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
    ContentModelList getParents() const {
        ContentModelList result;
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
    ContentModelList getChildren() const {
        ContentModelList result;
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
    ContentModel* getChild(qsizetype index) const {
        for (auto child : children()) {
            auto model = dynamic_cast<ContentModel*>(child);
            if (model) {
                if (!index) return model;
                --index;
            }
        }
        return nullptr;
    }

    // Does not use the lookup table.
    ContentModel* getChildByName(QStringView name, const QString& prop = "record_name") const {
        qsizetype idx = name.indexOf('.');
        if (idx != -1) {
            auto left  = name.left(idx);
            auto right = name.right(idx);
            auto model = getChildByName(left);
            if (model) return model->getChildByName(right);
        } else {
            for (auto child : children()) {
                auto model = dynamic_cast<ContentModel*>(child);
                if (model && model->value(prop).toString() == name) return model;
            }
        }
        return nullptr;
    }

    // Does not use the lookup table.
    ContentModel* getChildByName(const char* name, const QString& prop = "record_name") const {
        return getChildByName(QString(name), prop);
    }

    // Sorts children based on record order from database.
    void sortChildren() {
        std::stable_sort(d_ptr->children.begin(), d_ptr->children.end(), [](QObject* a, QObject* b) {
            ContentModel* ptrA = dynamic_cast<ContentModel*>(a);
            ContentModel* ptrB = dynamic_cast<ContentModel*>(b);
            if (ptrA == ptrB || !ptrA || !ptrB) return false;
            int rankA = ptrA->getProperty<int>("rank");
            int rankB = ptrB->getProperty<int>("rank");
            return rankA < rankB;
        });
    }

    /**
     * @brief Performs a deep comparison between this and another ContentModel.
     * @param other The other ContentModel to compare against.
     * @return True if the models are equal (excluding UID), false otherwise.
     */
    bool isEqualTo(const ContentModel* other) const {
        if (!other) return false;
        if (other == this) return true;
        if (size() != other->size()) return false;

        const auto names = keys();
        if (names != other->keys()) return false; // Assumes sorted.

        for (const auto& key : names) {
            if (key == "uid") continue; // Skip uid, as it will always be different.

            const auto val = value(key);
            const auto ptr = qvariant_cast<ContentModel*>(val);
            if (ptr) {
                const auto theirs = qvariant_cast<ContentModel*>(other->value(key));
                if (ptr == theirs) continue;
                if (!ptr->isEqualTo(theirs)) return false;
            } else if (val != other->value(key))
                return false;
        }

        return true;
    }

  private:
    /**
     * @brief Private constructor for creating a model with a UID.
     * @param uid The unique identifier for the model.
     * @param parent The parent QObject, defaults to nullptr.
     * @throws If the UID is empty or already exists in the lookup table.
     */
    ContentModel(const QString& uid, QObject* parent = nullptr)
        : QQmlPropertyMap(this, parent) {
        insert("uid", uid);
        if (uid.isEmpty()) throw std::runtime_error("UID must be valid");
        auto& lookup = ContentLookup::get();
        if (lookup.contains(uid)) throw std::runtime_error("UID must be unique");
        // qInfo() << "Adding record" << uid << "with name" << value("record_name");
        lookup.insert(uid, this);
    }

    /**
     * @brief Private constructor for creating a model with properties.
     * @param props The initial properties to set.
     * @param parent The parent QObject, defaults to nullptr.
     * @throws If the UID is empty or already exists in the lookup table.
     */
    ContentModel(const QVariantHash& props, QObject* parent = nullptr)
        : QQmlPropertyMap(this, parent) {
        insert(props);
        if (!contains("uid")) throw std::runtime_error("UID must be specified");
        QString uid = property("uid").toString();
        if (uid.isEmpty()) throw std::runtime_error("UID must be valid");
        auto& lookup = ContentLookup::get();
        if (lookup.contains(uid)) throw std::runtime_error("UID must be unique");
        // qInfo() << "Adding record" << uid << "with name" << value("record_name");
        lookup.insert(uid, this);
    }

    /**
     * @brief Destructor that removes the instance from the lookup table. Private, because lifetime is entirely managed
     * by the lookup table.
     */
    ~ContentModel() {
        auto& lookup = ContentLookup::get();
        lookup.remove(property("uid").toString());
    }
};

} // namespace dsqt::model

// Make the content model available to standard stream operators
std::ostream& operator<<(std::ostream& os, const dsqt::model::ContentModel* o);

#endif
