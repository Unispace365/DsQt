#include "settings/dsSettingsTreeModel.h"
#include "settings/dsSettingsFile.h"

#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace dsqt {

// ── Display helpers ───────────────────────────────────────────────────────────

// Returns true if every character in s is a decimal digit (and s is non-empty).
static bool isNumericString(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar ch : s)
        if (!ch.isDigit())
            return false;
    return true;
}

static bool isMap(const QVariant &v)
{
    return v.metaType() == QMetaType::fromType<QVariantMap>();
}

static bool isList(const QVariant &v)
{
    return v.metaType() == QMetaType::fromType<QVariantList>();
}

// Converts a QVariant to a human-readable string for display.
// Public static so that ArrayEditorDialog can reuse it.
QString SettingsTreeModel::displayString(const QVariant &v)
{
    if (!v.isValid())
        return {};
    if (v.metaType() == QMetaType::fromType<bool>())
        return v.toBool() ? "true" : "false";
    if (v.metaType() == QMetaType::fromType<QColor>())
        return v.value<QColor>().name(QColor::HexArgb);
    if (v.metaType() == QMetaType::fromType<QPointF>()) {
        const auto p = v.value<QPointF>();
        return QString("(%1, %2)").arg(p.x()).arg(p.y());
    }
    if (v.metaType() == QMetaType::fromType<QSizeF>()) {
        const auto s = v.value<QSizeF>();
        return QString("%1 × %2").arg(s.width()).arg(s.height());
    }
    if (v.metaType() == QMetaType::fromType<QRectF>()) {
        const auto r = v.value<QRectF>();
        return QString("(%1, %2, %3, %4)").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    }
    if (v.metaType() == QMetaType::fromType<QVector2D>()) {
        const auto vec = v.value<QVector2D>();
        return QString("(%1, %2)").arg(vec.x()).arg(vec.y());
    }
    if (v.metaType() == QMetaType::fromType<QVector3D>()) {
        const auto vec = v.value<QVector3D>();
        return QString("(%1, %2, %3)").arg(vec.x()).arg(vec.y()).arg(vec.z());
    }
    if (v.metaType() == QMetaType::fromType<QVector4D>()) {
        const auto vec = v.value<QVector4D>();
        return QString("(%1, %2, %3, %4)").arg(vec.x()).arg(vec.y()).arg(vec.z()).arg(vec.w());
    }
    if (v.metaType() == QMetaType::fromType<QDate>())
        return v.value<QDate>().toString(Qt::ISODate);
    if (v.metaType() == QMetaType::fromType<QTime>())
        return v.value<QTime>().toString(Qt::ISODate);
    if (v.metaType() == QMetaType::fromType<QDateTime>())
        return v.value<QDateTime>().toString(Qt::ISODate);
    // QVariantList is shown as a branch node; this fallback is only reached
    // for nested list elements that are themselves lists.
    if (v.metaType() == QMetaType::fromType<QVariantList>())
        return QString("[%1 items]").arg(v.toList().size());
    return v.toString();
}

// Returns a short, human-readable type label for a leaf value.
static QString typeName(const QVariant &v)
{
    if (!v.isValid())
        return {};
    const QMetaType mt = v.metaType();
    if (mt == QMetaType::fromType<bool>())
        return "Boolean";
    if (mt == QMetaType::fromType<qint32>())
        return "Int32";
    if (mt == QMetaType::fromType<qint64>())
        return "Int64";
    if (mt == QMetaType::fromType<double>() || mt == QMetaType::fromType<float>())
        return "Real";
    if (mt == QMetaType::fromType<QString>())
        return "String";
    if (mt == QMetaType::fromType<QColor>())
        return "Color";
    if (mt == QMetaType::fromType<QPointF>())
        return "Point";
    if (mt == QMetaType::fromType<QSizeF>())
        return "Size";
    if (mt == QMetaType::fromType<QRectF>())
        return "Rectangle";
    if (mt == QMetaType::fromType<QVector2D>())
        return "Vector2D";
    if (mt == QMetaType::fromType<QVector3D>())
        return "Vector3D";
    if (mt == QMetaType::fromType<QVector4D>())
        return "Vector4D";
    if (mt == QMetaType::fromType<QQuaternion>())
        return "Quaternion";
    if (mt == QMetaType::fromType<QDate>())
        return "Date";
    if (mt == QMetaType::fromType<QTime>())
        return "Time";
    if (mt == QMetaType::fromType<QDateTime>())
        return "DateTime";
    if (mt == QMetaType::fromType<QVariantList>())
        return QString("List[%1]").arg(v.toList().size());
    return QString::fromUtf8(mt.name());
}

// ── SettingsTreeModel ─────────────────────────────────────────────────────────

SettingsTreeModel::SettingsTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_root(new SettingsTreeItem)
{}

SettingsTreeModel::~SettingsTreeModel()
{
    delete m_root;
}

SettingsFile *SettingsTreeModel::settingsFile() const
{
    return m_settingsFile;
}

void SettingsTreeModel::setSettingsFile(SettingsFile *sf)
{
    if (m_settingsFile == sf)
        return;

    if (m_settingsFile)
        disconnect(m_settingsFile, nullptr, this, nullptr);

    m_settingsFile = sf;

    if (m_settingsFile)
        connect(m_settingsFile, &SettingsFile::settingsRebuilt, this, &SettingsTreeModel::rebuild);

    rebuild();
    emit settingsFileChanged();
}

void SettingsTreeModel::rebuild()
{
    beginResetModel();
    delete m_root;
    m_root = new SettingsTreeItem;

    if (m_settingsFile)
        buildChildren(m_root, m_settingsFile->allSettings(), {});

    endResetModel();
}

void SettingsTreeModel::buildChildren(SettingsTreeItem *parent,
                                      const QVariantMap &map,
                                      const QString &prefix)
{
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const QString fullPath = prefix.isEmpty() ? it.key() : prefix + u'.' + it.key();
        auto *item = new SettingsTreeItem(it.key(), fullPath, parent);

        if (isMap(it.value())) {
            buildChildren(item, it.value().toMap(), fullPath);
        } else if (isList(it.value())) {
            item->typeName   = typeName(it.value());
            item->provenance = m_settingsFile ? m_settingsFile->provenance(fullPath) : QString{};
            buildListChildren(item, it.value().toList(), fullPath);
        } else {
            item->isLeaf    = true;
            item->rawValue  = it.value();
            item->value     = displayString(it.value());
            item->typeName  = typeName(it.value());
            item->provenance = m_settingsFile ? m_settingsFile->provenance(fullPath) : QString{};
        }

        parent->children.append(item);
    }
}

void SettingsTreeModel::buildListChildren(SettingsTreeItem *parent,
                                          const QVariantList &list,
                                          const QString &prefix)
{
    for (int i = 0; i < list.size(); ++i) {
        const QString key      = QString::number(i);
        const QString fullPath = prefix + '.' + key;
        const QVariant &v      = list.at(i);
        auto *item             = new SettingsTreeItem(key, fullPath, parent);

        if (isMap(v)) {
            buildChildren(item, v.toMap(), fullPath);
        } else if (isList(v)) {
            item->typeName = typeName(v);
            buildListChildren(item, v.toList(), fullPath);
        } else {
            item->isLeaf   = true;
            item->rawValue = v;
            item->value    = displayString(v);
            item->typeName = typeName(v);
            // Provenance is not tracked for individual array elements.
        }

        parent->children.append(item);
    }
}

// ── QAbstractItemModel interface ──────────────────────────────────────────────

QModelIndex SettingsTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    const SettingsTreeItem *parentItem = parent.isValid() ? static_cast<SettingsTreeItem *>(
                                                                parent.internalPointer())
                                                          : m_root;
    if (row < 0 || row >= parentItem->children.size())
        return {};
    return createIndex(row, column, parentItem->children.at(row));
}

QModelIndex SettingsTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    const auto *item = static_cast<SettingsTreeItem *>(index.internalPointer());
    SettingsTreeItem *parentItem = item->parent;
    if (!parentItem || parentItem == m_root)
        return {};
    SettingsTreeItem *grandParent = parentItem->parent ? parentItem->parent : m_root;
    const int row = grandParent->children.indexOf(parentItem);
    return createIndex(row, 0, parentItem);
}

int SettingsTreeModel::rowCount(const QModelIndex &parent) const
{
    const SettingsTreeItem *item = parent.isValid()
                                       ? static_cast<SettingsTreeItem *>(parent.internalPointer())
                                       : m_root;
    return item->children.size();
}

int SettingsTreeModel::columnCount(const QModelIndex &) const
{
    return 3;
}

QVariant SettingsTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    const auto *item = static_cast<SettingsTreeItem *>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0: return item->key;
        case 1: return item->isLeaf ? item->value    : QString{};
        case 2: return item->typeName;
        }
        return {};
    case Qt::EditRole:
        return (item->isLeaf && index.column() == 1) ? item->rawValue : QVariant{};
    case Qt::DecorationRole:
        if (item->isLeaf && index.column() == 1
            && item->rawValue.metaType() == QMetaType::fromType<QColor>()) {
            QPixmap pix(14, 14);
            pix.fill(item->rawValue.value<QColor>());
            return pix;
        }
        return {};
    case Qt::ToolTipRole:
        // Show provenance for any item that has it (leaves AND list parent nodes).
        // List elements don't have per-element provenance, so they show nothing.
        return item->provenance.isEmpty() ? QVariant{} : item->provenance;
    case Qt::FontRole: {
        QFont font;
        font.setBold(item->hasOverride());
        return item->hasOverride() ? font : QVariant{};
    }
    // Extra roles — kept for QML consumers.
    case ValueRole:      return item->isLeaf ? item->value    : QString{};
    case IsLeafRole:     return item->isLeaf;
    case FullPathRole:   return item->fullPath;
    case ProvenanceRole: return item->provenance;
    case TypeRole:       return item->isLeaf ? item->typeName : QString{};
    case IsListRole:
        // A node is a list node when it is a non-leaf whose children carry
        // purely numeric keys ("0", "1", …) — i.e. it was built by buildListChildren.
        if (item->isLeaf || item->children.isEmpty())
            return false;
        return isNumericString(item->children.first()->key);
    default:
        return {};
    }
}

QVariant SettingsTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return "Key";
    case 1: return "Value";
    case 2: return "Type";
    }
    return {};
}

Qt::ItemFlags SettingsTreeModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    if (index.isValid() && index.column() == 1 && m_settingsFile) {
        const auto *item = static_cast<SettingsTreeItem *>(index.internalPointer());
        if (item->isLeaf)
            f |= Qt::ItemIsEditable;
    }
    return f;
}

bool SettingsTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.column() != 1)
        return false;
    const auto *item = static_cast<SettingsTreeItem *>(index.internalPointer());
    if (!item->isLeaf || !m_settingsFile)
        return false;

    // If the incoming value is already the right type (e.g. QColor from a dialog), use it directly.
    const QMetaType target = item->rawValue.metaType();
    QVariant converted;

    if (value.metaType() == target) {
        converted = value;
    } else {
        // Parse from the string the editor produced.
        const QString str = value.toString().trimmed();
        if (target == QMetaType::fromType<QColor>()) {
            const QColor c = QColor::fromString(str);
            if (!c.isValid()) return false;
            converted = QVariant::fromValue(c);
        } else if (target == QMetaType::fromType<bool>()) {
            converted = (str == "true" || str == "1");
        } else if (target == QMetaType::fromType<int>()) {
            bool ok; const int v = str.toInt(&ok);
            if (!ok) return false;
            converted = v;
        } else if (target == QMetaType::fromType<qlonglong>()) {
            bool ok; const qlonglong v = str.toLongLong(&ok);
            if (!ok) return false;
            converted = v;
        } else if (target == QMetaType::fromType<double>()
                   || target == QMetaType::fromType<float>()) {
            bool ok; const double v = str.toDouble(&ok);
            if (!ok) return false;
            converted = v;
        } else {
            converted = str;
        }
    }

    // If this item is a scalar element inside a list node, we must update the
    // whole list rather than calling setOverride on the dotted sub-path (e.g.
    // "myList.0"). The dot-split in setOverride/setInMap would store the value
    // under a nested QVariantMap key "0", and deepMerge would then replace the
    // original QVariantList with that one-entry map — wiping all sibling items.
    SettingsTreeItem *parentItem = item->parent;
    if (parentItem && parentItem != m_root && isNumericString(item->key)) {
        // Verify that the parent is genuinely a list node (all children numeric).
        bool parentIsList = !parentItem->children.isEmpty();
        for (const SettingsTreeItem *c : std::as_const(parentItem->children)) {
            if (!isNumericString(c->key)) { parentIsList = false; break; }
        }
        if (parentIsList) {
            QVariantList list;
            list.reserve(parentItem->children.size());
            for (const SettingsTreeItem *c : std::as_const(parentItem->children))
                list.append(c == item ? converted : c->rawValue);
            m_settingsFile->setOverride(parentItem->fullPath, QVariant::fromValue(list));
            return true;
        }
    }

    // Regular leaf edit — setOverride triggers settingsRebuilt → rebuild(), so
    // we don't need to update the item or emit dataChanged manually.
    m_settingsFile->setOverride(item->fullPath, converted);
    return true;
}

QHash<int, QByteArray> SettingsTreeModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();
    roles[ValueRole]      = "value";
    roles[IsLeafRole]     = "isLeaf";
    roles[FullPathRole]   = "fullPath";
    roles[ProvenanceRole] = "provenance";
    roles[TypeRole]       = "typeName";
    roles[IsListRole]     = "isList";
    return roles;
}

}
