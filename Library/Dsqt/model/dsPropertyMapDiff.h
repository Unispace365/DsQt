#ifndef DSPROPERTYMAPDIFF_H
#define DSPROPERTYMAPDIFF_H

#include "model/dsReferenceMap.h"

#include <QList>
#include <QStringList>
#include <QVariant>

namespace dsqt::model {
class DsQmlContentModel;
/**
 * Encapsulates the differences between two QmlContentModels (including their nested children trees).
 * You can compute a diff, apply it to a map to bring it in sync,
 * or invert it to roll back the changes.
 */
class PropertyMapDiff
{
public:
    enum ChangeType {
        AddedProperty,
        RemovedProperty,
        ModifiedProperty,
        AddedNode,
        RemovedNode
    };

    struct Change {
        ChangeType    type;
        QStringList   path;      ///< Sequence of child-indices from the root
        QString       key;       ///< Property name (for property changes)
        QVariant      oldValue;  ///< For Removed/ModifiedProperty or RemovedNode
        QVariant      newValue;  ///< For Added/ModifiedProperty or AddedNode
    };

    /**
     * Compute the diff that turns @p from into @p to, recursing into "children".
     */
    PropertyMapDiff(const DsQmlContentModel &from,
                        const DsQmlContentModel &to);

    PropertyMapDiff();

    /// All changes in the order: removals, additions, modifications, in tree order
    QList<Change> changes() const { return m_changes; }

    /**
     * Apply this diff *in place* to @p map.  After calling,
     * map (and its children) will have the same structure & values
     * as the “to” map used to construct this diff.
     */
    void apply(DsQmlContentModel &map,ReferenceMap* itemMap) const;

    /**
     * Return a new diff which is the inverse of this one.
     * Applying inverted() to the “to” map rolls it back
     * to the “from” state.
     */
    PropertyMapDiff inverted() const;
    void dumpChanges();
private:
    QList<Change> m_changes;
    QSet<const QString> m_visited;
    // Recursively compare two maps at the given path
    void diffMaps(const DsQmlContentModel &from,
                  const DsQmlContentModel &to,
                  const QStringList    &path);

    // Helpers to traverse a DsQmlContentModel tree by path
    DsQmlContentModel* nodeAtPath(DsQmlContentModel &root,
                                const QStringList &path) const;
    const DsQmlContentModel* nodeAtPath(const DsQmlContentModel &root,
                                      const QStringList &path) const;

};
}

#endif // PROPERTY_MAP_DIFF_H
