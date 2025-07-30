#include "dsPropertyMapDiff.h"
#include <QSet>
#include <QStringLiteral>
#include <QRegularExpression>
#include "model/dsQmlContentHelper.h"
namespace dsqt::model {
using namespace Qt::StringLiterals;
PropertyMapDiff::PropertyMapDiff(const QmlContentModel &from, const QmlContentModel &to)
{
    m_visited.clear();
    diffMaps(from, to, {});
}

PropertyMapDiff::PropertyMapDiff() {}

void PropertyMapDiff::diffMaps(const QmlContentModel &from,
                                   const QmlContentModel &to,
                                   const QStringList    &path)
{
    auto id = from.value("uid").toString();
    if(id.isEmpty()){
        id = from.value("name").toString();
    }
    if(m_visited.contains(id)){
        return;
    }
    m_visited.insert(id);
    //qDebug().noquote().nospace()<<path.count()<<"-"<<"From "<<from.value("name").toString()<<"("<<from.value("uid").toString()
    //                            <<") and "<<to.value("name").toString()<<"("<<to.value("id").toString()<<")";
    // 1) Diff simple properties (skip "children" & "parent")
    auto filterKeys = [&](const QStringList &all) {
        QRegularExpression re("^(?!children|parent).*");
        return all.filter(re);
    };
    auto fromKeysList = filterKeys(from.keys());
    auto toKeysList = filterKeys(to.keys());
    QSet<QString> fromKeys(fromKeysList.begin(),fromKeysList.end());
    QSet<QString> toKeys(toKeysList.begin(),toKeysList.end());

    // RemovedProperty
    auto remove = fromKeys - toKeys;
    for (const QString &k : remove) {
        Change c;
        c.type     = RemovedProperty;
        c.path     = path;
        c.key      = k;
        c.oldValue = from.value(k);
        m_changes.append(c);
    }

    // AddedProperty
    auto added = toKeys - fromKeys;
    for (const QString &k : added) {
        Change c;
        c.type     = AddedProperty;
        c.path     = path;
        c.key      = k;
        c.newValue = to.value(k);
        m_changes.append(c);
    }

    // ModifiedProperty
    auto mods = fromKeys & toKeys;
    for (const QString &k : fromKeys & toKeys) {
        QVariant v1 = from.value(k), v2 = to.value(k);
        if (v1 != v2) {
            Change c;
            c.type     = ModifiedProperty;
            c.path     = path;
            c.key      = k;
            c.oldValue = v1;
            c.newValue = v2;
            m_changes.append(c);
        }
    }

    // 2) Diff children-lists
    QVariantList fromChildren = from.value(u"children"_s).toList();
    QVariantList   toChildren = to.value(u"children"_s).toList();
    int fCount = fromChildren.size(), tCount = toChildren.size();
    int common = qMin(fCount, tCount);

    // Recurse on matching indices

    for (int i = 0; i < common; ++i) {
        // assume each QVariant holds a QmlContentModel* as a QObject*
        auto *fObj = fromChildren[i].value<QObject*>();
        auto *tObj = toChildren  [i].value<QObject*>();
        auto *fMap = qobject_cast<QmlContentModel*>(fObj);
        auto *tMap = qobject_cast<QmlContentModel*>(tObj);
        if (fMap && tMap) {

            diffMaps(*fMap, *tMap, path + QStringList(QString::number(i)));
        }
    }
    // RemovedNode
    QmlContentModel emptyMap(ContentModelRef(),nullptr);
    for (int i = common; i < fCount; ++i) {
        auto *oldMap = qobject_cast<QmlContentModel*>(fromChildren[i].value<QObject*>());
        Change c;
        c.type     = RemovedNode;
        c.path     = path + QStringList(QString::number(i));
        c.oldValue = fromChildren[i];
        m_changes.append(c);
        diffMaps(*oldMap,emptyMap, path + QStringList(QString::number(i)));
    }
    // AddedNode
    for (int i = common; i < tCount; ++i) {
        auto *newMap = qobject_cast<QmlContentModel*>(toChildren[i].value<QObject*>());
        Change c;
        c.type     = AddedNode;
        c.path     = path + QStringList(QString::number(i));
        c.newValue = toChildren[i];
        m_changes.append(c);
        diffMaps(emptyMap, *newMap, path + QStringList(QString::number(i)));
    }
}

QmlContentModel* PropertyMapDiff::nodeAtPath(QmlContentModel &root,
                                                 const QStringList &path) const
{
    QmlContentModel *current = &root;
    for (const QString &seg : path) {
        bool ok = false;
        int idx = seg.toInt(&ok);
        if (!ok) break;
        QVariantList kids = current->value(u"children"_s).toList();
        if (idx < 0 || idx >= kids.size()) break;
        QObject *obj = kids[idx].value<QObject*>();
        current = qobject_cast<QmlContentModel*>(obj);
        if (!current) break;
    }
    return current;
}

const QmlContentModel* PropertyMapDiff::nodeAtPath(
    const QmlContentModel &root,
    const QStringList      &path) const
{
    // same as above, but const
    const QmlContentModel *current = &root;
    for (const QString &seg : path) {
        bool ok = false;
        int idx = seg.toInt(&ok);
        if (!ok) break;
        QVariantList kids = current->value(u"children"_s).toList();
        if (idx < 0 || idx >= kids.size()) break;
        QObject *obj = kids[idx].value<QObject*>();
        current = qobject_cast<QmlContentModel*>(obj);
        if (!current) break;
    }
    return current;
}

void PropertyMapDiff::dumpChanges()
{
    qDebug()<<"DUMP##############";
    for (const Change &c : m_changes){
        QString typeString;
        QMap<ChangeType,QString> typeStr = {
            {AddedProperty,"AddedProperty"},
            {RemovedProperty,"RemovedProperty"},
            {ModifiedProperty,"ModifiedProperty"},
            {AddedNode,"AddedNode"},
            {RemovedNode,"RemovedNode"}
            };
        auto pathStr = c.path.join(".");

        qDebug().noquote()<<typeStr[c.type]<<pathStr<<c.key<<c.newValue.toString()<<c.oldValue.toString();
    }
    qDebug()<<"DUMP##############";
}

void PropertyMapDiff::apply(QmlContentModel &map,ReferenceMap* itemMap) const
{
    qDebug()<<"Apply changes to qml model";
    for (const Change &c : m_changes) {
        QmlContentModel *node = nullptr;
        switch (c.type) {
        case AddedProperty:
        case RemovedProperty:
        case ModifiedProperty:
            node = nodeAtPath(map, c.path);
            if (!node) continue;
            if (c.type == RemovedProperty) {
                node->insert(c.key,QVariant());
                emit node->valueChanged(c.key,QVariant());
            } else {
                node->insert(c.key, c.newValue);
                emit node->valueChanged(c.key,c.newValue);
            }
            break;

        case AddedNode: {
            node = qobject_cast<QmlContentModel*>(c.newValue.value<QObject*>());
            auto *parent = nodeAtPath(map, c.path.mid(0, c.path.size()-1));
            if (!parent) break;
            QVariantList kids = parent->value(u"children"_s).toList();
            int idx = c.path.last().toInt();
            kids.insert(idx, c.newValue);
            auto id = node->value("id").toString();
            if(id.isEmpty()){
                id = node->value("name").toString();
            }
            itemMap->insert(id,node);
            parent->insert(u"children"_s, kids);
            emit parent->valueChanged(u"children"_s,kids);

            break;
        }

        case RemovedNode: {
            node = qobject_cast<QmlContentModel*>(c.oldValue.value<QObject*>());
            auto *parent = nodeAtPath(map, c.path.mid(0, c.path.size()-1));
            if (!parent) break;
            QVariantList kids = parent->value(u"children"_s).toList();
            auto id = node->value("id").toString();
            if(id.isEmpty()){
                id = node->value("name").toString();
            }
            itemMap->remove(id);
            int idx = c.path.last().toInt();
            if (idx >= 0 && idx < kids.size())
                kids.removeAt(idx);
            parent->insert(u"children"_s, kids);
            emit parent->valueChanged(u"children"_s,kids);
            break;
        }
        }
    }
}

PropertyMapDiff PropertyMapDiff::inverted() const
{
    PropertyMapDiff inv;
    // Walk in reverse to undo in correct order
    for (auto it = m_changes.crbegin(); it != m_changes.crend(); ++it) {
        Change c;
        c.path = it->path;
        switch (it->type) {
        case AddedProperty:
            c.type     = RemovedProperty;
            c.key      = it->key;
            c.oldValue = it->newValue;
            break;
        case RemovedProperty:
            c.type     = AddedProperty;
            c.key      = it->key;
            c.newValue = it->oldValue;
            break;
        case ModifiedProperty:
            c.type     = ModifiedProperty;
            c.path     = it->path;
            c.key      = it->key;
            c.oldValue = it->newValue;
            c.newValue = it->oldValue;
            break;
        case AddedNode:
            c.type     = RemovedNode;
            c.oldValue = it->newValue;
            break;
        case RemovedNode:
            c.type     = AddedNode;
            c.newValue = it->oldValue;
            break;
        }
        inv.m_changes.append(c);
    }
    return inv;
}

}
