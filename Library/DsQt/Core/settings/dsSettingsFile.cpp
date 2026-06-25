#include "settings/dsSettingsFile.h"
#include "settings/dsSettings.h"
#include "settings/dsSettingsUtils.h"

#include <algorithm>
#include <sstream>
#include <string_view>

#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QPointF>
#include <QQuaternion>
#include <QRectF>
#include <QRegularExpression>
#include <QSet>
#include <QSizeF>
#include <QTimeZone>
#include <QVector3D>
#include <QVector4D>

// TOML parsing is isolated here — never pulled into the header.
#include <toml++/toml.hpp>

namespace dsqt {

// ── TOML conversion ────────────────────────────────────────────────────────

// Mutually recursive with tableToMap()/arrayToList() below.
static QVariant nodeToVariant(const toml::node &node);
static QVariantMap tableToMap(const toml::table &tbl);

static QString metaString(const toml::table &meta, std::string_view key)
{
    if (const toml::node *node = meta.get(key)) {
        if (const auto value = node->value<std::string>())
            return QString::fromStdString(*value);
    }
    return {};
}

// Reads a numeric colour channel and normalises it to 0.0–1.0.
// "X%" divides by 100; a plain number is clamped to [0, scale] and divided by scale.
static double readNormalized(const QVariant &v, double scale)
{
    const QString s = v.toString().trimmed();
    if (s.endsWith('%'))
        return s.chopped(1).toDouble() / 100.0;
    return std::clamp(v.toDouble(), 0.0, scale) / scale;
}

// Detects well-known key-signature maps and converts them to typed Qt values.
// QVariantMap keys are always sorted (QMap), so they compare directly against
// sorted key lists.
static bool isUnitRangeColorMap(const QVariantMap &map)
{
    for (const QVariant &channel : map) {
        bool ok = false;
        const double value = channel.toDouble(&ok);
        if (!ok || value < 0.0 || value > 1.0)
            return false;
    }
    return true;
}

static QVariant tryInterpretMap(const QVariantMap &map)
{
    const QStringList keys = map.keys();

    // QColor RGB {r, g, b} / {r, g, b, a}  — r/g/b/a: 0–255, or "X%"
    if (keys == QStringList{"b", "g", "r"}) {
        const double scale = isUnitRangeColorMap(map) ? 1.0 : 255.0;
        return QColor::fromRgbF(readNormalized(map["r"], scale),
                                readNormalized(map["g"], scale),
                                readNormalized(map["b"], scale));
    }
    if (keys == QStringList{"a", "b", "g", "r"}) {
        const double scale = isUnitRangeColorMap(map) ? 1.0 : 255.0;
        return QColor::fromRgbF(readNormalized(map["r"], scale),
                                readNormalized(map["g"], scale),
                                readNormalized(map["b"], scale),
                                readNormalized(map["a"], scale));
    }

    // QColor HSV {h, s, v} / {h, s, v, a}  — h: 0–360, s/v: 0–100, a: 0–255, or "X%"
    if (keys == QStringList{"h", "s", "v"})
        return QColor::fromHsvF(readNormalized(map["h"], 360.0),
                                readNormalized(map["s"], 100.0),
                                readNormalized(map["v"], 100.0));
    if (keys == QStringList{"a", "h", "s", "v"})
        return QColor::fromHsvF(readNormalized(map["h"], 360.0),
                                readNormalized(map["s"], 100.0),
                                readNormalized(map["v"], 100.0),
                                readNormalized(map["a"], 255.0));

    // QColor HSL {h, l, s} / {h, l, s, a}  — h: 0–360, s/l: 0–100, a: 0–255, or "X%"
    if (keys == QStringList{"h", "l", "s"})
        return QColor::fromHslF(readNormalized(map["h"], 360.0),
                                readNormalized(map["s"], 100.0),
                                readNormalized(map["l"], 100.0));
    if (keys == QStringList{"a", "h", "l", "s"})
        return QColor::fromHslF(readNormalized(map["h"], 360.0),
                                readNormalized(map["s"], 100.0),
                                readNormalized(map["l"], 100.0),
                                readNormalized(map["a"], 255.0));

    // QColor CMYK {c, m, y, k} / {c, m, y, k, a}  — c/m/y/k: 0–100, a: 0–255, or "X%"
    if (keys == QStringList{"c", "k", "m", "y"})
        return QColor::fromCmykF(readNormalized(map["c"], 100.0),
                                 readNormalized(map["m"], 100.0),
                                 readNormalized(map["y"], 100.0),
                                 readNormalized(map["k"], 100.0));
    if (keys == QStringList{"a", "c", "k", "m", "y"})
        return QColor::fromCmykF(readNormalized(map["c"], 100.0),
                                 readNormalized(map["m"], 100.0),
                                 readNormalized(map["y"], 100.0),
                                 readNormalized(map["k"], 100.0),
                                 readNormalized(map["a"], 255.0));

    // QRectF {x, y, w, h} → sorted: h, w, x, y  (checked before point/size)
    if (keys == QStringList{"h", "w", "x", "y"})
        return QRectF(map["x"].toDouble(),
                      map["y"].toDouble(),
                      map["w"].toDouble(),
                      map["h"].toDouble());

    // QRectF {x, y, width, height} → sorted: height, width, x, y  (checked before point/size)
    if (keys == QStringList{"height", "width", "x", "y"})
        return QRectF(map["x"].toDouble(),
                      map["y"].toDouble(),
                      map["width"].toDouble(),
                      map["height"].toDouble());

    // QRectF {x1, y1, x2, y2} two-point format → sorted: x1, x2, y1, y2
    if (keys == QStringList{"x1", "x2", "y1", "y2"})
        return QRectF(QPointF(map["x1"].toDouble(), map["y1"].toDouble()),
                      QPointF(map["x2"].toDouble(), map["y2"].toDouble()));

    // QPointF {x, y}
    if (keys == QStringList{"x", "y"})
        return QPointF(map["x"].toDouble(), map["y"].toDouble());

    // QVector3D {x, y, z}
    if (keys == QStringList{"x", "y", "z"})
        return QVector3D(map["x"].toFloat(), map["y"].toFloat(), map["z"].toFloat());

    // QVector4D {w, x, y, z} — sorted: w, x, y, z
    if (keys == QStringList{"w", "x", "y", "z"})
        return QVector4D(map["x"].toFloat(),
                         map["y"].toFloat(),
                         map["z"].toFloat(),
                         map["w"].toFloat());

    // QQuaternion {scalar, x, y, z} — sorted: scalar, x, y, z
    if (keys == QStringList{"scalar", "x", "y", "z"})
        return QQuaternion(map["scalar"].toFloat(),
                           map["x"].toFloat(),
                           map["y"].toFloat(),
                           map["z"].toFloat());

    // QSizeF {w, h} → sorted: h, w
    if (keys == QStringList{"h", "w"})
        return QSizeF(map["w"].toDouble(), map["h"].toDouble());

    // QSizeF {width, height} → sorted: height, width
    if (keys == QStringList{"height", "width"})
        return QSizeF(map["width"].toDouble(), map["height"].toDouble());

    return {}; // no match — leave as QVariantMap
}

static QVariantMap tableToMap(const toml::table &tbl)
{
    QVariantMap map;
    for (auto &&[k, v] : tbl)
        map.insert(QString::fromStdString(std::string(k)), nodeToVariant(v));
    return map;
}

static QVariantList arrayToList(const toml::array &arr)
{
    QVariantList list;
    for (auto &&item : arr)
        list.append(nodeToVariant(item));
    return list;
}

static QVariant legacyColorFromList(const QVariantList &list,
                                    const QString &elementType,
                                    const QString &colorType)
{
    if (list.isEmpty())
        return {};

    const bool isFloat = elementType == QStringLiteral("float")
                         || (elementType != QStringLiteral("int")
                             && std::all_of(list.cbegin(), list.cend(), [](const QVariant &v) {
                                bool ok = false;
                                const double value = v.toDouble(&ok);
                                return ok && value >= 0.0 && value <= 1.0;
                            }));
    const auto channel = [&](int index, double intScale, double fallback = 1.0) {
        if (index >= list.size())
            return fallback;
        const double value = list[index].toDouble();
        return isFloat ? std::clamp(value, 0.0, 1.0)
                       : std::clamp(value, 0.0, intScale) / intScale;
    };

    const QString mode = colorType.isEmpty() ? QStringLiteral("rgb") : colorType;
    if (mode == QStringLiteral("cmyk") && list.size() >= 4)
        return QColor::fromCmykF(channel(0, isFloat ? 100.0 : 255.0),
                                 channel(1, isFloat ? 100.0 : 255.0),
                                 channel(2, isFloat ? 100.0 : 255.0),
                                 channel(3, isFloat ? 100.0 : 255.0),
                                 channel(4, 255.0));
    if (mode == QStringLiteral("hsv") && list.size() >= 3)
        return QColor::fromHsvF(channel(0, 360.0),
                                channel(1, isFloat ? 100.0 : 255.0),
                                channel(2, isFloat ? 100.0 : 255.0),
                                channel(3, 255.0));
    if (mode == QStringLiteral("hsl") && list.size() >= 3)
        return QColor::fromHslF(channel(0, 360.0),
                                channel(1, isFloat ? 100.0 : 255.0),
                                channel(2, isFloat ? 100.0 : 255.0),
                                channel(3, 255.0));

    if (list.size() == 1)
        return QColor::fromRgbF(channel(0, 255.0),
                                channel(0, 255.0),
                                channel(0, 255.0));
    if (list.size() == 2)
        return QColor::fromRgbF(channel(0, 255.0),
                                channel(0, 255.0),
                                channel(0, 255.0),
                                channel(1, 255.0));
    if (list.size() >= 3)
        return QColor::fromRgbF(channel(0, 255.0),
                                channel(1, 255.0),
                                channel(2, 255.0),
                                channel(3, 255.0));

    return {};
}

static QVariant legacyColorFromMap(const QVariantMap &map, const QString &elementType)
{
    const QStringList keys = map.keys();
    const bool isFloat = elementType == QStringLiteral("float")
                         || (elementType != QStringLiteral("int") && isUnitRangeColorMap(map));
    const auto channel = [&](const QString &key, double intScale, double fallback = 1.0) {
        if (!map.contains(key))
            return fallback;
        const double value = map[key].toDouble();
        return isFloat ? std::clamp(value, 0.0, 1.0)
                       : std::clamp(value, 0.0, intScale) / intScale;
    };

    if (keys == QStringList{"b", "g", "r"})
        return QColor::fromRgbF(channel("r", 255.0),
                                channel("g", 255.0),
                                channel("b", 255.0));
    if (keys == QStringList{"a", "b", "g", "r"})
        return QColor::fromRgbF(channel("r", 255.0),
                                channel("g", 255.0),
                                channel("b", 255.0),
                                channel("a", 255.0));
    if (keys == QStringList{"c", "k", "m", "y"})
        return QColor::fromCmykF(channel("c", isFloat ? 100.0 : 255.0),
                                 channel("m", isFloat ? 100.0 : 255.0),
                                 channel("y", isFloat ? 100.0 : 255.0),
                                 channel("k", isFloat ? 100.0 : 255.0));
    if (keys == QStringList{"a", "c", "k", "m", "y"})
        return QColor::fromCmykF(channel("c", isFloat ? 100.0 : 255.0),
                                 channel("m", isFloat ? 100.0 : 255.0),
                                 channel("y", isFloat ? 100.0 : 255.0),
                                 channel("k", isFloat ? 100.0 : 255.0),
                                 channel("a", 255.0));
    if (keys == QStringList{"h", "s", "v"})
        return QColor::fromHsvF(channel("h", 360.0),
                                channel("s", isFloat ? 100.0 : 255.0),
                                channel("v", isFloat ? 100.0 : 255.0));
    if (keys == QStringList{"a", "h", "s", "v"})
        return QColor::fromHsvF(channel("h", 360.0),
                                channel("s", isFloat ? 100.0 : 255.0),
                                channel("v", isFloat ? 100.0 : 255.0),
                                channel("a", 255.0));
    if (keys == QStringList{"h", "l", "s"})
        return QColor::fromHslF(channel("h", 360.0),
                                channel("s", isFloat ? 100.0 : 255.0),
                                channel("l", isFloat ? 100.0 : 255.0));
    if (keys == QStringList{"a", "h", "l", "s"})
        return QColor::fromHslF(channel("h", 360.0),
                                channel("s", isFloat ? 100.0 : 255.0),
                                channel("l", isFloat ? 100.0 : 255.0),
                                channel("a", 255.0));

    return {};
}

static QVariant legacyMetadataValue(const toml::array &arr)
{
    const toml::table *meta = arr[1].as_table();
    const toml::node &valueNode = arr[0];
    const QString type = metaString(*meta, "type");

    if (type == QStringLiteral("color")) {
        const QString elementType = metaString(*meta, "element_type");
        const QString colorType = metaString(*meta, "array_color_type");

        if (const toml::array *valueArray = valueNode.as_array()) {
            const QVariant color = legacyColorFromList(arrayToList(*valueArray),
                                                       elementType,
                                                       colorType);
            if (color.isValid())
                return color;
        }

        if (const toml::table *valueTable = valueNode.as_table()) {
            const QVariantMap map = tableToMap(*valueTable);
            const QVariant color = legacyColorFromMap(map, elementType);
            if (color.isValid())
                return color;
        }
    }

    if (type == QStringLiteral("rect")) {
        if (const toml::array *valueArray = valueNode.as_array()) {
            const QVariantList list = arrayToList(*valueArray);
            if (list.size() == 4)
                return QRectF(list[0].toDouble(),
                              list[1].toDouble(),
                              list[2].toDouble(),
                              list[3].toDouble());
        }
    }

    return nodeToVariant(valueNode);
}

static QVariant nodeToVariant(const toml::node &node)
{
    switch (node.type()) {
    case toml::node_type::table: {
        const QVariantMap map = tableToMap(*node.as_table());
        const QVariant interpreted = tryInterpretMap(map);
        return interpreted.isValid() ? interpreted : QVariant::fromValue(map);
    }

    case toml::node_type::array: {
        const toml::array &arr = *node.as_array();
        if (arr.size() == 2 && arr[1].is_table())
            return legacyMetadataValue(arr);
        if (arr.size() == 1 && arr[0].is_array())
            return nodeToVariant(arr[0]);
        return arrayToList(arr);
    }

    case toml::node_type::string: {
        const QString s = QString::fromStdString(*node.value<std::string>());
        const QColor color = QColor::fromString(s);
        return color.isValid() ? QVariant::fromValue(color) : s;
    }

    case toml::node_type::integer:
        return QVariant::fromValue(*node.value<int64_t>());

    case toml::node_type::floating_point:
        return node.value_or(0.0);

    case toml::node_type::boolean:
        return node.value_or(false);

    case toml::node_type::date: {
        auto d = *node.value<toml::date>();
        return QDate(d.year, d.month, d.day);
    }
    case toml::node_type::time: {
        auto t = *node.value<toml::time>();
        return QTime(t.hour, t.minute, t.second, t.nanosecond / 1'000'000);
    }
    case toml::node_type::date_time: {
        auto dt = *node.value<toml::date_time>();
        QDate date(dt.date.year, dt.date.month, dt.date.day);
        QTime time(dt.time.hour, dt.time.minute, dt.time.second, dt.time.nanosecond / 1'000'000);
        if (dt.offset)
            return QDateTime(date, time, QTimeZone::fromSecondsAheadOfUtc(dt.offset->minutes * 60));
        return QDateTime(date, time);
    }
    default:
        return {};
    }
}

// Reads and parses a TOML file from the filesystem or a Qt resource.
static toml::table parseFile(const QString &path)
{
    if (settings::isResourcePath(path)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            throw toml::parse_error("cannot open resource file", toml::source_region{});
        const QByteArray data = f.readAll();
        return toml::parse(std::string_view(data.constData(), data.size()));
    }
    return toml::parse_file(path.toStdString());
}

// ── QVariantMap helpers ────────────────────────────────────────────────────

static bool isMap(const QVariant &v)
{
    return v.metaType() == QMetaType::fromType<QVariantMap>();
}

// Deep-merges `overlay` into `base`: tables merge recursively, scalars overwrite.
// If fileIndex >= 0, records it in m_provenance for every leaf written.
void SettingsFile::deepMerge(QVariantMap &base,
                             const QVariantMap &overlay,
                             int fileIndex,
                             const QString &prefix)
{
    for (auto it = overlay.cbegin(); it != overlay.cend(); ++it) {
        const QString fullKey = prefix.isEmpty() ? it.key() : prefix + u'.' + it.key();

        if (isMap(it.value())) {
            // Always recurse into maps so leaf provenance is recorded at every depth,
            // even when the base has no prior entry for this key.
            QVariantMap nested = isMap(base.value(it.key())) ? base[it.key()].toMap()
                                                             : QVariantMap{};
            deepMerge(nested, it.value().toMap(), fileIndex, fullKey);
            base[it.key()] = nested;
        } else {
            base[it.key()] = it.value();
            if (fileIndex >= 0)
                m_provenance[fullKey] = fileIndex;
        }
    }
}

// Writes `value` at the dot-separated path, overwriting any existing value and
// creating intermediate maps as needed.
static void setInMap(QVariantMap &map, const QStringList &parts, const QVariant &value)
{
    const QString &key = parts.first();
    if (parts.size() == 1) {
        map.insert(key, value);
        return;
    }
    QVariantMap child = map.value(key).toMap();
    setInMap(child, parts.mid(1), value);
    map.insert(key, child);
}

// Inserts `defaultValue` at the dot-separated path only if the leaf is absent.
// Returns true if it inserted; on false the map is untouched (parents are not
// re-inserted, avoiding implicit-sharing detaches).
static bool ensureInMap(QVariantMap &map, const QStringList &parts, const QVariant &defaultValue)
{
    const QString &key = parts.first();
    if (parts.size() == 1) {
        if (map.contains(key))
            return false;
        map.insert(key, defaultValue);
        return true;
    }
    QVariantMap child = map.value(key).toMap();
    if (!ensureInMap(child, parts.mid(1), defaultValue))
        return false;
    map.insert(key, child);
    return true;
}

// Removes the value at the dot-separated path, pruning parent maps that become
// empty. Returns true if something was removed.
static bool removeInMap(QVariantMap &map, const QStringList &parts)
{
    const QString &key = parts.first();
    if (!map.contains(key))
        return false;

    if (parts.size() == 1)
        return map.remove(key) > 0;

    if (!isMap(map[key]))
        return false;

    QVariantMap child = map[key].toMap();
    const bool removed = removeInMap(child, parts.mid(1));
    if (child.isEmpty())
        map.remove(key);
    else
        map.insert(key, child);
    return removed;
}

// Returns the value at the dot-separated path, or an invalid QVariant if absent.
static QVariant traverseMap(const QVariant &map, const QStringList &parts)
{
    QVariant current = map;
    for (const QString &part : parts) {
        if (!isMap(current))
            return {};
        const QVariantMap map = current.toMap();
        if (!map.contains(part))
            return {};
        current = map.value(part);
    }
    return current;
}

// ── Reference resolution ───────────────────────────────────────────────────

// A reference is a string of the form "@a.b" — a bare TOML dotted key
// (at least one dot, no spaces) preceded by '@'.
static bool pruneOverrides(QVariantMap &overrides, const QVariantMap &baseline)
{
    bool changed = false;

    for (auto it = overrides.begin(); it != overrides.end();) {
        const QVariant baselineValue = baseline.value(it.key());

        if (isMap(it.value()) && isMap(baselineValue)) {
            QVariantMap child = it.value().toMap();
            if (pruneOverrides(child, baselineValue.toMap())) {
                changed = true;
                if (child.isEmpty()) {
                    it = overrides.erase(it);
                    continue;
                }
                it.value() = child;
            }
        } else if (baselineValue.isValid() && it.value() == baselineValue) {
            it = overrides.erase(it);
            changed = true;
            continue;
        }

        ++it;
    }

    return changed;
}

static bool isReference(const QVariant &v)
{
    static const QRegularExpression pattern(
        QStringLiteral("^@[A-Za-z0-9_-]+(?:\\.[A-Za-z0-9_-]+)+$"));
    return v.metaType() == QMetaType::fromType<QString>() && pattern.match(v.toString()).hasMatch();
}

// Follows a chain of references through `root`. Returns the final value, or
// an invalid QVariant on a missing target or a cycle.
static QVariant chaseReference(const QString &ref, const QVariantMap &root)
{
    QSet<QString> visited;
    QVariant current = ref;
    while (isReference(current)) {
        const QString path = current.toString().mid(1); // strip '@'
        if (visited.contains(path)) {
            qWarning("SettingsFile: reference cycle detected at '@%s'", qPrintable(path));
            return {};
        }
        visited.insert(path);
        current = traverseMap(root, path.split(u'.'));
        if (!current.isValid()) {
            qWarning("SettingsFile: reference target '@%s' not found", qPrintable(path));
            return {};
        }
    }
    return current;
}

// Recursively replaces every reference string in `map` with the value it
// resolves to in `root`. Unresolvable references are left as raw strings.
// References inside arrays are not resolved (dotted paths cannot address
// list elements).
static void resolveRefs(QVariantMap &map, const QVariantMap &root)
{
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (isMap(it.value())) {
            QVariantMap child = it.value().toMap();
            resolveRefs(child, root);
            it.value() = child;
        } else if (isReference(it.value())) {
            const QVariant resolved = chaseReference(it.value().toString(), root);
            if (resolved.isValid())
                it.value() = resolved;
        }
    }
}

// ── QQmlPropertyMap helpers ────────────────────────────────────────────────

// Returns the child QQmlPropertyMap at `key` of `parent`, creating and
// inserting it if absent.
static QQmlPropertyMap *getOrCreateChildMap(QQmlPropertyMap *parent, const QString &key)
{
    auto *child = qvariant_cast<QQmlPropertyMap *>(parent->value(key));
    if (!child) {
        child = new QQmlPropertyMap(parent);
        parent->insert(key, QVariant::fromValue(child));
    }
    return child;
}

// ── SettingsFile ───────────────────────────────────────────────────────────

SettingsFile::SettingsFile(QObject *parent, const QStringList &searchPaths)
    : QQmlPropertyMap(this, parent)
{
    connect(this, &SettingsFile::managerChanged, this, &SettingsFile::reload);
    connect(this, &SettingsFile::fileNameChanged, this, &SettingsFile::reload);
    connect(this, &SettingsFile::extraFilesChanged, this, &SettingsFile::reload);
    connect(this, &SettingsFile::searchPathsChanged, this, &SettingsFile::reload);
    setSearchPaths(searchPaths);
}

Settings *SettingsFile::manager() const
{
    return m_manager;
}

void SettingsFile::setManager(Settings *manager)
{
    if (m_manager == manager)
        return;

    if (m_manager)
        m_manager->unregisterSettingsFile(this);

    m_manager = manager;

    if (m_manager)
        m_manager->registerSettingsFile(this);

    emit managerChanged();
}

QString SettingsFile::fileName() const
{
    return m_fileName;
}

void SettingsFile::setFileName(const QString &fileName)
{
    if (m_fileName == fileName)
        return;
    m_fileName = fileName;
    emit fileNameChanged();
}

QStringList SettingsFile::extraFiles() const
{
    return m_extraFiles;
}

void SettingsFile::setExtraFiles(const QStringList &fileNames)
{
    if (m_extraFiles == fileNames)
        return;
    m_extraFiles = fileNames;
    emit extraFilesChanged();
}

QStringList SettingsFile::searchPaths() const
{
    return m_searchPaths;
}

void SettingsFile::setSearchPaths(const QStringList &paths)
{
    if (m_searchPaths == paths)
        return;
    m_searchPaths = paths;
    emit searchPathsChanged();
}

QStringList SettingsFile::resolvedFilePaths() const
{
    QStringList result;
    const auto resolve = [this](const QString &fileName) {
        return m_manager ? m_manager->resolveFilePathsImpl(fileName)
                         : settings::resolveFilePaths(fileName, m_searchPaths);
    };

    result.append(resolve(m_fileName));
    for (const QString &fileName : m_extraFiles)
        result.append(resolve(fileName));

    return result;
}

void SettingsFile::reload()
{
    settings::checkThread(this, Q_FUNC_INFO);
    m_settings.clear();
    m_files.clear();
    m_provenance.clear();

    for (const QString &path : resolvedFilePaths()) {
        try {
            const int fileIndex = m_files.size();
            m_files.append(path);
            deepMerge(m_settings, tableToMap(parseFile(path)), fileIndex);
        } catch (const toml::parse_error &e) {
            qWarning("SettingsFile: failed to parse '%s': %s",
                     qPrintable(path),
                     e.description().data());
        }
    }

    QVariantMap baseline = m_defaults;
    deepMerge(baseline, m_settings);
    pruneOverrides(m_overrides, baseline);

    rebuild();
}

// Re-merges all layers, resolves references, and pushes the result into the
// QQmlPropertyMap tree. Merge order (lowest priority first):
// defaults → file data → runtime overrides.
void SettingsFile::rebuild()
{
    QVariantMap merged = m_defaults;
    deepMerge(merged, m_settings);
    deepMerge(merged, m_overrides);

    // Build the fully-resolved view before acquiring the write lock so
    // background readers are blocked as briefly as possible.
    QVariantMap resolved = merged;
    resolveRefs(resolved, merged);

    {
        QWriteLocker locker(&m_mergedLock);
        m_merged = resolved;
    }

    // syncMap always runs on the main thread; use the local copy directly
    // so we don't need to re-acquire the lock.
    syncMap(this, resolved);
    emit settingsRebuilt();
}

void SettingsFile::setOverride(const QString &key, const QVariant &value)
{
    settings::checkThread(this, Q_FUNC_INFO);
    setInMap(m_overrides, key.split(u'.'), value);
    rebuild();
}

void SettingsFile::resetOverride(const QString &key)
{
    settings::checkThread(this, Q_FUNC_INFO);
    if (removeInMap(m_overrides, key.split(u'.')))
        rebuild();
}

void SettingsFile::resetOverrides()
{
    settings::checkThread(this, Q_FUNC_INFO);
    if (m_overrides.isEmpty())
        return;
    m_overrides.clear();
    rebuild();
}

void SettingsFile::ensurePath(const QString &key, const QVariant &defaultValue)
{
    settings::checkThread(this, Q_FUNC_INFO);
    const QStringList parts = key.split(u'.');

    // Register the raw default so it participates in every future rebuild.
    ensureInMap(m_defaults, parts, defaultValue);

    // Apply it incrementally instead of doing a full rebuild: defaults only
    // fill holes, so an if-absent insert reproduces exactly what the next
    // rebuild would produce. m_merged is already resolved, so a reference
    // default can be chased against it directly.
    QVariant resolved = defaultValue;
    if (isReference(defaultValue)) {
        const QVariant chased = chaseReference(defaultValue.toString(), m_merged);
        if (chased.isValid())
            resolved = chased;
    }
    if (!ensureInMap(m_merged, parts, resolved))
        return; // already present — the QQmlPropertyMap tree mirrors it

    // Mirror the path into the QQmlPropertyMap tree so bind()'s walk and QML
    // bindings see it immediately.
    QQmlPropertyMap *map = this;
    for (int i = 0; i < parts.size() - 1; ++i)
        map = getOrCreateChildMap(map, parts[i]);
    if (!map->value(parts.last()).isValid())
        map->insert(parts.last(), resolved);
}

void SettingsFile::syncMap(QQmlPropertyMap *target, const QVariantMap &newData)
{
    // QQmlPropertyMap cannot delete keys; clear vanished ones to invalid.
    const QStringList currentKeys = target->keys();
    for (const QString &key : currentKeys) {
        if (!newData.contains(key))
            target->insert(key, QVariant{});
    }

    // Insert, update, or recurse into existing children.
    for (auto it = newData.cbegin(); it != newData.cend(); ++it) {
        if (isMap(it.value()))
            syncMap(getOrCreateChildMap(target, it.key()), it.value().toMap());
        else
            target->insert(it.key(), it.value()); // scalar: just update
    }
}

QString SettingsFile::provenance(const QString &key) const
{
    settings::checkThread(this, Q_FUNC_INFO);
    const QStringList parts = key.split(u'.');
    if (traverseMap(m_overrides, parts).isValid())
        return "override";
    const int fileIndex = m_provenance.value(key, -1);
    if (fileIndex >= 0)
        return m_files.value(fileIndex);
    if (traverseMap(m_defaults, parts).isValid())
        return "default";
    return {};
}

QVariant SettingsFile::value(const QString &key, const QVariant &defaultValue) const
{
    // m_merged already encodes the layer priority (defaults → file → overrides)
    // and has references resolved, so a single lookup suffices.
    // QReadLocker allows concurrent readers; QWriteLocker in rebuild() is the
    // sole writer. Safe to call from any thread.
    QReadLocker locker(&m_mergedLock);
    const QVariant current = traverseMap(m_merged, key.split(u'.'));
    return current.isValid() ? current : defaultValue;
}

QVariantMap SettingsFile::allSettings() const
{
    QReadLocker locker(&m_mergedLock);
    return m_merged;
}

// ── TOML output helpers ────────────────────────────────────────────────────
// Convert Qt types back to toml++ nodes for serialisation.
// applyVariantToTable handles all leaf QVariant types (scalars and collections).
// applyOverridesToTable walks a QVariantMap recursively and writes every entry
// into an existing (or freshly created) toml::table.

static void appendVariantToArray(toml::array &arr, const QVariant &v);

static void applyVariantToTable(toml::table &tbl,
                                const std::string &key,
                                const QVariant &v)
{
    const QMetaType mt = v.metaType();

    // ── Scalars ──────────────────────────────────────────────────────────────
    if (mt == QMetaType::fromType<bool>()) {
        tbl.insert_or_assign(key, v.toBool());
    } else if (mt == QMetaType::fromType<int>() || mt == QMetaType::fromType<qint32>()) {
        tbl.insert_or_assign(key, static_cast<int64_t>(v.toInt()));
    } else if (mt == QMetaType::fromType<qlonglong>() || mt == QMetaType::fromType<qint64>()) {
        tbl.insert_or_assign(key, static_cast<int64_t>(v.toLongLong()));
    } else if (mt == QMetaType::fromType<double>()) {
        tbl.insert_or_assign(key, v.toDouble());
    } else if (mt == QMetaType::fromType<float>()) {
        tbl.insert_or_assign(key, static_cast<double>(v.toFloat()));

    // ── Colour → hex string ──────────────────────────────────────────────────
    } else if (mt == QMetaType::fromType<QColor>()) {
        const QColor c = v.value<QColor>();
        // Omit alpha component when fully opaque for cleaner output.
        const QString hex = (c.alpha() == 255)
                                ? c.name(QColor::HexRgb)
                                : c.name(QColor::HexArgb);
        tbl.insert_or_assign(key, hex.toStdString());

    // ── Compound geometric types → inline tables ─────────────────────────────
    } else if (mt == QMetaType::fromType<QPointF>()) {
        const QPointF p = v.value<QPointF>();
        toml::table t; t.is_inline(true);
        t.insert_or_assign("x", p.x()); t.insert_or_assign("y", p.y());
        tbl.insert_or_assign(key, std::move(t));
    } else if (mt == QMetaType::fromType<QSizeF>()) {
        const QSizeF s = v.value<QSizeF>();
        toml::table t; t.is_inline(true);
        t.insert_or_assign("w", s.width()); t.insert_or_assign("h", s.height());
        tbl.insert_or_assign(key, std::move(t));
    } else if (mt == QMetaType::fromType<QRectF>()) {
        const QRectF r = v.value<QRectF>();
        toml::table t; t.is_inline(true);
        t.insert_or_assign("x", r.x()); t.insert_or_assign("y", r.y());
        t.insert_or_assign("w", r.width()); t.insert_or_assign("h", r.height());
        tbl.insert_or_assign(key, std::move(t));
    } else if (mt == QMetaType::fromType<QVector2D>()) {
        const QVector2D vec = v.value<QVector2D>();
        toml::table t; t.is_inline(true);
        t.insert_or_assign("x", static_cast<double>(vec.x()));
        t.insert_or_assign("y", static_cast<double>(vec.y()));
        tbl.insert_or_assign(key, std::move(t));
    } else if (mt == QMetaType::fromType<QVector3D>()) {
        const QVector3D vec = v.value<QVector3D>();
        toml::table t; t.is_inline(true);
        t.insert_or_assign("x", static_cast<double>(vec.x()));
        t.insert_or_assign("y", static_cast<double>(vec.y()));
        t.insert_or_assign("z", static_cast<double>(vec.z()));
        tbl.insert_or_assign(key, std::move(t));
    } else if (mt == QMetaType::fromType<QVector4D>()) {
        const QVector4D vec = v.value<QVector4D>();
        toml::table t; t.is_inline(true);
        t.insert_or_assign("x", static_cast<double>(vec.x()));
        t.insert_or_assign("y", static_cast<double>(vec.y()));
        t.insert_or_assign("z", static_cast<double>(vec.z()));
        t.insert_or_assign("w", static_cast<double>(vec.w()));
        tbl.insert_or_assign(key, std::move(t));
    } else if (mt == QMetaType::fromType<QQuaternion>()) {
        const QQuaternion q = v.value<QQuaternion>();
        toml::table t; t.is_inline(true);
        t.insert_or_assign("scalar", static_cast<double>(q.scalar()));
        t.insert_or_assign("x", static_cast<double>(q.x()));
        t.insert_or_assign("y", static_cast<double>(q.y()));
        t.insert_or_assign("z", static_cast<double>(q.z()));
        tbl.insert_or_assign(key, std::move(t));

    // ── Date / time ──────────────────────────────────────────────────────────
    } else if (mt == QMetaType::fromType<QDate>()) {
        const QDate d = v.value<QDate>();
        tbl.insert_or_assign(key, toml::date{
            static_cast<uint16_t>(d.year()),
            static_cast<uint8_t>(d.month()),
            static_cast<uint8_t>(d.day())});
    } else if (mt == QMetaType::fromType<QTime>()) {
        const QTime t2 = v.value<QTime>();
        tbl.insert_or_assign(key, toml::time{
            static_cast<uint8_t>(t2.hour()),
            static_cast<uint8_t>(t2.minute()),
            static_cast<uint8_t>(t2.second()),
            static_cast<uint32_t>(t2.msec()) * 1'000'000u});
    } else if (mt == QMetaType::fromType<QDateTime>()) {
        const QDateTime dt = v.value<QDateTime>();
        const QDate d = dt.date();
        const QTime t2 = dt.time();
        tbl.insert_or_assign(key, toml::date_time{
            toml::date{static_cast<uint16_t>(d.year()),
                       static_cast<uint8_t>(d.month()),
                       static_cast<uint8_t>(d.day())},
            toml::time{static_cast<uint8_t>(t2.hour()),
                       static_cast<uint8_t>(t2.minute()),
                       static_cast<uint8_t>(t2.second()),
                       static_cast<uint32_t>(t2.msec()) * 1'000'000u}});

    // ── Collections ──────────────────────────────────────────────────────────
    } else if (mt == QMetaType::fromType<QVariantList>()) {
        toml::array a;
        for (const QVariant &elem : v.toList())
            appendVariantToArray(a, elem);
        tbl.insert_or_assign(key, std::move(a));
    } else if (mt == QMetaType::fromType<QVariantMap>()) {
        toml::table t;
        const QVariantMap m = v.toMap();
        for (auto it = m.cbegin(); it != m.cend(); ++it)
            applyVariantToTable(t, it.key().toStdString(), it.value());
        tbl.insert_or_assign(key, std::move(t));

    // ── Fallback: string representation ─────────────────────────────────────
    } else {
        tbl.insert_or_assign(key, v.toString().toStdString());
    }
}

static void appendVariantToArray(toml::array &arr, const QVariant &v)
{
    const QMetaType mt = v.metaType();
    if (mt == QMetaType::fromType<bool>())
        arr.push_back(v.toBool());
    else if (mt == QMetaType::fromType<int>() || mt == QMetaType::fromType<qint32>())
        arr.push_back(static_cast<int64_t>(v.toInt()));
    else if (mt == QMetaType::fromType<qlonglong>())
        arr.push_back(static_cast<int64_t>(v.toLongLong()));
    else if (mt == QMetaType::fromType<double>() || mt == QMetaType::fromType<float>())
        arr.push_back(v.toDouble());
    else if (mt == QMetaType::fromType<QColor>()) {
        const QColor c = v.value<QColor>();
        const QString hex = (c.alpha() == 255) ? c.name(QColor::HexRgb)
                                               : c.name(QColor::HexArgb);
        arr.push_back(hex.toStdString());
    } else if (mt == QMetaType::fromType<QVariantList>()) {
        toml::array nested;
        for (const QVariant &elem : v.toList())
            appendVariantToArray(nested, elem);
        arr.push_back(std::move(nested));
    } else if (mt == QMetaType::fromType<QVariantMap>()) {
        toml::table t;
        const QVariantMap m = v.toMap();
        for (auto it = m.cbegin(); it != m.cend(); ++it)
            applyVariantToTable(t, it.key().toStdString(), it.value());
        arr.push_back(std::move(t));
    } else {
        arr.push_back(v.toString().toStdString());
    }
}

// Walks overrides (a QVariantMap) and writes every entry into tbl.
// For sub-maps, recurses into the corresponding toml::table (creating it if absent).
// For everything else, delegates to applyVariantToTable.
static void applyOverridesToTable(toml::table &tbl, const QVariantMap &overrides)
{
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        const std::string key = it.key().toStdString();
        if (it.value().metaType() == QMetaType::fromType<QVariantMap>()) {
            // Ensure the sub-table exists.
            toml::node *existing = tbl.get(key);
            if (!existing || !existing->is_table())
                tbl.insert_or_assign(key, toml::table{});
            applyOverridesToTable(*tbl.get(key)->as_table(), it.value().toMap());
        } else {
            applyVariantToTable(tbl, key, it.value());
        }
    }
}

QString SettingsFile::saveOverridesTo(const QString &filePath) const
{
    settings::checkThread(this, Q_FUNC_INFO);

    if (m_overrides.isEmpty())
        return {}; // nothing to do — not an error

    // Read the existing file (if any) so we can preserve unmodified content.
    toml::table result;
    if (QFile::exists(filePath)) {
        try {
            result = parseFile(filePath);
        } catch (const toml::parse_error &e) {
            return QString("Failed to parse existing file '%1': %2")
                       .arg(filePath,
                            QString::fromUtf8(e.description().data()));
        }
    }

    // Apply overrides into the (possibly empty) table.
    applyOverridesToTable(result, m_overrides);

    // Serialise to a string, then write atomically via QSaveFile.
    std::ostringstream oss;
    oss << result;
    if (!oss)
        return "TOML serialisation failed.";

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString("Cannot open '%1' for writing: %2")
                   .arg(filePath, file.errorString());

    file.write(QByteArray::fromStdString(oss.str()));

    if (!file.commit())
        return QString("Write failed for '%1': %2")
                   .arg(filePath, file.errorString());

    return {}; // success
}

}
