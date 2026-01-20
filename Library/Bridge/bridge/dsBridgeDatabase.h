#ifndef DSBRIDGEDATABASE_H
#define DSBRIDGEDATABASE_H

#include <QHash>
#include <QString>
#include <QVariantHash>

namespace dsqt::bridge {

using DatabaseRecord     = QVariantHash;
using DatabaseRecordList = QList<DatabaseRecord>;
using DatabaseRecordHash = QHash<QString, DatabaseRecord>;

class DatabaseContent {
  public:
    DatabaseContent() = default;

    DatabaseRecord     find(const QString& uid) const { return m_records.value(uid); }
    DatabaseRecordList find(const QStringList& uids) const {
        DatabaseRecordList result;
        for (const auto& uid : uids) {
            if (auto itr = m_records.constFind(uid); itr != m_records.constEnd()) result.append(itr.value());
        }
        return result;
    }

    /// Returns a list of record uids, sorted in the correct order.
    const QStringList& recordUids() const { return m_sorted; }
    /// Returns a list of record uids for all content root records.
    const QStringList& contentUids() const { return m_content; }
    /// Returns a list of record uids for all event records.
    const QStringList& eventUids() const { return m_events; }
    /// Returns a list of record uids for all platform records.
    const QStringList& platformUids() const { return m_platforms; }

    /// Returns all records.
    const DatabaseRecordHash& records() const { return m_records; }
    /// Returns all content root records.
    DatabaseRecordList content() const { return find(m_content); }
    /// Returns all event records.
    DatabaseRecordList events() const { return find(m_events); }
    /// Returns all platform records.
    DatabaseRecordList platforms() const { return find(m_platforms); }

  private:
    friend class DatabaseIterator;
    friend class DatabaseTree;
    friend class DsBridgeSqlQuery;

    /// Links all records to their parents and all parents to their children.
    void buildTree() {
        // Clear existing list of children.
        for (auto node : m_records.asKeyValueRange()) {
            node.second["child_uid"] = QStringList();
        }
        // Create the sorted list of records.
        m_sorted = m_records.keys();
        std::sort(m_sorted.begin(), m_sorted.end(), [&](const QString& uid1, const QString& uid2) {
            int rank1 = m_records.value(uid1).value("rank").toInt();
            int rank2 = m_records.value(uid2).value("rank").toInt();
            return rank1 < rank2; // Ascending order
        });
        // Populate list of children. Children are sorted in order.
        for (auto node : m_records.asKeyValueRange()) {
            const auto uid         = node.second["uid"].toString();
            const auto parent_uids = node.second["parent_uid"].toStringList();
            if (parent_uids.size() != 1) continue; // Skip records with multiple parents, or no parents at all.
            for (const auto& parent_uid : parent_uids) {
                const auto id = parent_uid.trimmed();
                if (m_records.contains(id)) {
                    auto children = m_records[id]["child_uid"].toStringList();
                    if (!children.contains(uid)) children.append(uid);
                    std::sort(children.begin(), children.end(), [&](const QString& uid1, const QString& uid2) {
                        int rank1 = m_records.value(uid1).value("rank").toInt();
                        int rank2 = m_records.value(uid2).value("rank").toInt();
                        return rank1 < rank2; // Ascending order
                    });
                    m_records[id]["child_uid"] = children;
                }
            }
        }
    }

    DatabaseRecordHash m_records;   // All records.
    QStringList        m_content;   // All content records.
    QStringList        m_platforms; // All platform records.
    QStringList        m_events;    // All event records.
    QStringList        m_sorted;    // All records sorted in order.
    QStringList        m_queue;     // All records in the order in which they need to be processed.
};

// Iterator class for tree traversal
class DatabaseIterator {
  public:
    DatabaseIterator(DatabaseRecordHash& nodes, const QStringList& order, qsizetype index)
        : m_nodes(nodes)
        , m_order(order)
        , m_index(index) {}

    // Dereference operator
    const DatabaseRecord& operator*() const { return m_nodes[m_order[m_index]]; }

    // Pointer operator
    const DatabaseRecord* operator->() const { return &m_nodes[m_order[m_index]]; }

    // Increment operator (pre-increment)
    DatabaseIterator& operator++() {
        if (m_index < m_order.size()) {
            ++m_index;
        }
        return *this;
    }

    // Equality operator
    bool operator==(const DatabaseIterator& other) const {
        return m_index == other.m_index && &m_nodes == &other.m_nodes;
    }

    // Inequality operator
    bool operator!=(const DatabaseIterator& other) const { return !(*this == other); }

  private:
    DatabaseRecordHash& m_nodes; // Reference to the node hash
    const QStringList&  m_order; // UIDs in the correct order
    qsizetype           m_index; // Current position in traversal
};

// Helper class to traverse the database.
class DatabaseTree {
  public:
    enum class Traversal { PreOrder, PostOrder, Roots };

    DatabaseTree(DatabaseRecordHash& nodes, const QString& rootUid, Traversal order = Traversal::PostOrder)
        : m_nodes(nodes) {
        if (m_nodes.contains(rootUid)) {
            switch (order) {
            case Traversal::PreOrder: // Visits the root node first, then recursively traverses its children.
                preOrderTraversal(rootUid);
                break;
            case Traversal::PostOrder: // Visits the child nodes before their parents, with the root node visited
                                       // last.
                postOrderTraversal(rootUid);
                break;
            case Traversal::Roots: // Only visits nodes that have no single parent.
                rootsTraversal();
                break;
            }
        }
    }

    DatabaseTree(DatabaseContent& content, Traversal order = Traversal::PostOrder)
        : DatabaseTree(content.m_records, content.m_content.front(), order) {}

    // Begin iterator
    DatabaseIterator begin() const { return DatabaseIterator(m_nodes, m_order, 0); }

    // End iterator
    DatabaseIterator end() const { return DatabaseIterator(m_nodes, m_order, m_order.size()); }

  private:
    // Recursive pre-order traversal
    void preOrderTraversal(const QString& uid) {
        // Visit the node itself first
        m_order.append(uid);
        // Then visit children
        const QStringList children = m_nodes[uid]["child_uid"].toStringList();
        for (const QString& childUid : children) {
            if (m_nodes.contains(childUid)) {
                preOrderTraversal(childUid);
            }
        }
    }

    // Recursive post-order traversal
    void postOrderTraversal(const QString& uid) {
        // Visit children first
        const QStringList children = m_nodes[uid]["child_uid"].toStringList();
        for (const QString& childUid : children) {
            if (m_nodes.contains(childUid)) {
                postOrderTraversal(childUid);
            }
        }
        // Then visit the node itself
        m_order.append(uid);
    }

    // Roots traversal
    void rootsTraversal() {
        for (const auto itr : m_nodes.asKeyValueRange()) {
            const auto& record = itr.second;
            if (!record.contains("parent_uid")) continue;
            const auto parents = record["parent_uid"].toStringList();
            if (parents.size() != 1) m_order.append(itr.first);
        }
    }

    DatabaseRecordHash& m_nodes; // Reference to the node hash (non-const for modification)
    QStringList         m_order; // UIDs of nodes in post-order
};

} // namespace dsqt::bridge

#endif
