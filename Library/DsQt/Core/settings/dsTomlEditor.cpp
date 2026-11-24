#include "settings/dsTomlEditor.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QQuaternion>
#include <QRect>
#include <QRectF>
#include <QRegularExpression>
#include <QSet>
#include <QSize>
#include <QSizeF>
#include <QTime>
#include <QTimeZone>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <toml++/toml.hpp>

// A targeted text editor for TOML documents. toml++ parses a document into a
// tree and serialises the tree anew, which drops every comment and blank line,
// so it cannot write a hand-written settings file back. This file scans the
// text just far enough to know where each key, value and table is, and splices
// new values into those spans. toml++ is still used to check that the input
// and the output are valid TOML.

namespace dsqt::settings {

namespace {

// ── Formatting ──────────────────────────────────────────────────────────────

bool isIntegral(const QMetaType &mt)
{
    switch (mt.id()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
        return true;
    default:
        return false;
    }
}

bool isFloating(const QMetaType &mt)
{
    return mt.id() == QMetaType::Double || mt.id() == QMetaType::Float;
}

bool isNumber(const QVariant &v)
{
    return isIntegral(v.metaType()) || isFloating(v.metaType());
}

// Shortest text that reads back as the same number. With `asInteger`, a whole
// number is written without a fraction ("3" rather than "3.0").
template<typename T>
QString formatFloat(T d, bool asInteger)
{
    if (std::isnan(d))
        return QStringLiteral("nan");
    if (std::isinf(d))
        return d < 0 ? QStringLiteral("-inf") : QStringLiteral("inf");
    if (asInteger && std::floor(d) == d && std::abs(double(d)) < 9.0e15)
        return QString::number(static_cast<qlonglong>(d));
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), d);
    QString text = QString::fromLatin1(buffer, result.ptr - buffer);
    if (!text.contains(u'.') && !text.contains(u'e') && !text.contains(u'E'))
        text += QStringLiteral(".0");
    return text;
}

QString formatNumber(const QVariant &v, bool asInteger)
{
    if (isIntegral(v.metaType())) {
        if (!asInteger)
            return formatFloat(v.toDouble(), false);
        if (v.metaType().id() == QMetaType::ULongLong || v.metaType().id() == QMetaType::ULong)
            return QString::number(v.toULongLong());
        return QString::number(v.toLongLong());
    }
    if (v.metaType().id() == QMetaType::Float)
        return formatFloat(v.toFloat(), asInteger);
    return formatFloat(v.toDouble(), asInteger);
}

QString escapeBasic(const QString &s, bool multiline)
{
    QString out;
    out.reserve(s.size() + 2);
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '\\': out += QStringLiteral("\\\\"); break;
        case '"': out += QStringLiteral("\\\""); break;
        case '\b': out += QStringLiteral("\\b"); break;
        case '\t': out += QStringLiteral("\\t"); break;
        case '\f': out += QStringLiteral("\\f"); break;
        case '\n': out += multiline ? QStringLiteral("\n") : QStringLiteral("\\n"); break;
        case '\r': out += multiline ? QStringLiteral("\r") : QStringLiteral("\\r"); break;
        default:
            if (c.unicode() < 0x20 || c.unicode() == 0x7f)
                out += QStringLiteral("\\u%1").arg(int(c.unicode()), 4, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    return out;
}

QString basicString(const QString &s)
{
    return u'"' + escapeBasic(s, false) + u'"';
}

bool fitsLiteral(const QString &s, bool multiline)
{
    for (const QChar c : s) {
        if (c == u'\n' || c == u'\r') {
            if (!multiline)
                return false;
        } else if ((c.unicode() < 0x20 && c != u'\t') || c.unicode() == 0x7f) {
            return false;
        }
    }
    return multiline ? !s.contains(QStringLiteral("'''")) : !s.contains(u'\'');
}

// Quotes `s` the way `original` (a string literal) is quoted.
QString quoteLike(const QString &s, const QString &original)
{
    const auto openingNewline = [&original]() -> QString {
        if (original.mid(3, 2) == QStringLiteral("\r\n"))
            return QStringLiteral("\r\n");
        if (original.mid(3, 1) == QStringLiteral("\n"))
            return QStringLiteral("\n");
        return {};
    };
    if (original.startsWith(QStringLiteral("'''")) && fitsLiteral(s, true))
        return QStringLiteral("'''") + openingNewline() + s + QStringLiteral("'''");
    if (original.startsWith(QStringLiteral("\"\"\"")))
        return QStringLiteral("\"\"\"") + openingNewline() + escapeBasic(s, true)
               + QStringLiteral("\"\"\"");
    if (original.startsWith(u'\'') && fitsLiteral(s, false))
        return u'\'' + s + u'\'';
    return basicString(s);
}

QString colorName(const QColor &c)
{
    return c.alpha() == 255 ? c.name(QColor::HexRgb) : c.name(QColor::HexArgb);
}

QString formatKeyPart(const QString &part)
{
    static const QRegularExpression bare(QStringLiteral("^[A-Za-z0-9_-]+$"));
    return bare.match(part).hasMatch() ? part : basicString(part);
}

QString formatKey(const QStringList &parts)
{
    QStringList out;
    for (const QString &part : parts)
        out.append(formatKeyPart(part));
    return out.join(u'.');
}

QString inlineTable(const QList<std::pair<QString, QString>> &members)
{
    if (members.isEmpty())
        return QStringLiteral("{}");
    QStringList parts;
    for (const auto &[key, value] : members)
        parts.append(formatKeyPart(key) + QStringLiteral(" = ") + value);
    return QStringLiteral("{ ") + parts.join(QStringLiteral(", ")) + QStringLiteral(" }");
}

QString num(double d)
{
    return formatFloat(d, true);
}

QString formatDate(const QDate &d)
{
    return d.toString(QStringLiteral("yyyy-MM-dd"));
}

QString formatTime(const QTime &t)
{
    return t.msec() ? t.toString(QStringLiteral("HH:mm:ss.zzz")) : t.toString(QStringLiteral("HH:mm:ss"));
}

QString formatDateTime(const QDateTime &dt)
{
    QString text = formatDate(dt.date()) + u'T' + formatTime(dt.time());
    switch (dt.timeSpec()) {
    case Qt::UTC:
        return text + u'Z';
    case Qt::OffsetFromUTC:
    case Qt::TimeZone: {
        const int offset = dt.offsetFromUtc();
        const int minutes = std::abs(offset) / 60;
        return text + (offset < 0 ? u'-' : u'+')
               + QStringLiteral("%1:%2").arg(minutes / 60, 2, 10, QLatin1Char('0'))
                     .arg(minutes % 60, 2, 10, QLatin1Char('0'));
    }
    default:
        return text;
    }
}

// The value as text for a string setting: what nodeToVariant() would read back.
std::optional<QString> asStringContent(const QVariant &v)
{
    switch (v.metaType().id()) {
    case QMetaType::QString:
        return v.toString();
    case QMetaType::QColor:
        return colorName(v.value<QColor>());
    case QMetaType::QDate:
        return v.toDate().toString(Qt::ISODate);
    case QMetaType::QTime:
        return v.toTime().toString(Qt::ISODateWithMs);
    case QMetaType::QDateTime:
        return v.toDateTime().toString(Qt::ISODateWithMs);
    case QMetaType::Bool:
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    default:
        if (isNumber(v))
            return formatNumber(v, true);
        if (v.canConvert<QString>() && v.metaType().id() != QMetaType::QVariantList
            && v.metaType().id() != QMetaType::QVariantMap)
            return v.toString();
        return std::nullopt;
    }
}

bool isStringLiteral(const QString &text)
{
    return text.startsWith(u'"') || text.startsWith(u'\'');
}

bool looksInteger(const QString &text)
{
    const QString t = text.trimmed();
    if (t.startsWith(QStringLiteral("0x")) || t.startsWith(QStringLiteral("0o"))
        || t.startsWith(QStringLiteral("0b")))
        return true;
    static const QRegularExpression integer(QStringLiteral("^[+-]?[0-9_]+$"));
    return integer.match(t).hasMatch();
}

} // namespace

QString formatTomlValue(const QVariant &v)
{
    const int id = v.metaType().id();
    if (id == QMetaType::Bool)
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (isNumber(v))
        return formatNumber(v, isIntegral(v.metaType()));
    switch (id) {
    case QMetaType::QString:
        return basicString(v.toString());
    case QMetaType::QColor:
        return basicString(colorName(v.value<QColor>()));
    case QMetaType::QPointF:
    case QMetaType::QPoint: {
        const QPointF p = v.toPointF();
        return inlineTable({{"x", num(p.x())}, {"y", num(p.y())}});
    }
    case QMetaType::QSizeF:
    case QMetaType::QSize: {
        const QSizeF s = v.toSizeF();
        return inlineTable({{"w", num(s.width())}, {"h", num(s.height())}});
    }
    case QMetaType::QRectF:
    case QMetaType::QRect: {
        const QRectF r = v.toRectF();
        return inlineTable({{"x", num(r.x())}, {"y", num(r.y())},
                            {"w", num(r.width())}, {"h", num(r.height())}});
    }
    case QMetaType::QVector2D: {
        const QVector2D p = v.value<QVector2D>();
        return inlineTable({{"x", num(p.x())}, {"y", num(p.y())}});
    }
    case QMetaType::QVector3D: {
        const QVector3D p = v.value<QVector3D>();
        return inlineTable({{"x", num(p.x())}, {"y", num(p.y())}, {"z", num(p.z())}});
    }
    case QMetaType::QVector4D: {
        const QVector4D p = v.value<QVector4D>();
        return inlineTable({{"x", num(p.x())}, {"y", num(p.y())}, {"z", num(p.z())},
                            {"w", num(p.w())}});
    }
    case QMetaType::QQuaternion: {
        const QQuaternion q = v.value<QQuaternion>();
        return inlineTable({{"scalar", num(q.scalar())}, {"x", num(q.x())},
                            {"y", num(q.y())}, {"z", num(q.z())}});
    }
    case QMetaType::QDate:
        return formatDate(v.toDate());
    case QMetaType::QTime:
        return formatTime(v.toTime());
    case QMetaType::QDateTime:
        return formatDateTime(v.toDateTime());
    case QMetaType::QVariantList:
    case QMetaType::QStringList: {
        QStringList items;
        for (const QVariant &item : v.toList())
            items.append(formatTomlValue(item));
        return u'[' + items.join(QStringLiteral(", ")) + u']';
    }
    case QMetaType::QVariantMap: {
        const QVariantMap map = v.toMap();
        QList<std::pair<QString, QString>> members;
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            members.append({it.key(), formatTomlValue(it.value())});
        return inlineTable(members);
    }
    default:
        return basicString(v.toString());
    }
}

namespace {

// ── Scanning ────────────────────────────────────────────────────────────────

// A value in the text: its span, and for arrays and inline tables their parts.
struct Node {
    enum Kind { Scalar, Array, Inline } kind = Scalar;
    qsizetype begin = 0;
    qsizetype end = 0;
    std::vector<Node> elements;          // Array
    std::vector<QStringList> memberKeys; // Inline: each member's key, as written
    std::vector<qsizetype> memberBegins; // Inline: where each member's key starts
    std::vector<Node> members;           // Inline
};

// A key = value line.
struct Entry {
    QStringList path;     // absolute
    QStringList relative; // as written, relative to its section
    Node value;
    int section = 0;
    qsizetype lineStart = 0;
    qsizetype lineEnd = 0; // just past the line's newline
};

// A [table] or [[array of tables]] header; section 0 is the root table.
struct Section {
    QStringList path;
    bool arrayOfTables = false;
    qsizetype headerStart = 0;
    qsizetype headerLineEnd = 0;
};

class Scanner {
  public:
    explicit Scanner(const QString &text) : s(text), n(text.size()) {}

    bool scan()
    {
        sections.push_back({});
        int current = 0;
        if (n > 0 && s[0] == QChar(0xfeff))
            p = 1; // byte order mark
        while (true) {
            skipWs();
            if (p >= n)
                break;
            if (s[p] == u'#') {
                skipComment();
                continue;
            }
            if (s[p] == u'\n' || s[p] == u'\r') {
                ++p;
                continue;
            }
            const qsizetype lineStart = startOfLine(p);
            if (s[p] == u'[') {
                Section section;
                section.arrayOfTables = at(p + 1) == u'[';
                section.headerStart = lineStart;
                p += section.arrayOfTables ? 2 : 1;
                if (!parseKey(section.path))
                    return false;
                skipWs();
                if (at(p) != u']' || (section.arrayOfTables && at(p + 1) != u']'))
                    return fail(QStringLiteral("expected ] after the table name"));
                p += section.arrayOfTables ? 2 : 1;
                if (!finishLine())
                    return false;
                section.headerLineEnd = p;
                sections.push_back(section);
                current = int(sections.size()) - 1;
                continue;
            }
            Entry entry;
            entry.section = current;
            entry.lineStart = lineStart;
            if (!parseKey(entry.relative))
                return false;
            skipWs();
            if (at(p) != u'=')
                return fail(QStringLiteral("expected = after the key"));
            ++p;
            skipWs();
            if (!parseValue(entry.value))
                return false;
            if (!finishLine())
                return false;
            entry.lineEnd = p;
            entry.path = sections[current].path + entry.relative;
            entries.push_back(std::move(entry));
        }
        return true;
    }

    QString error;
    std::vector<Section> sections;
    std::vector<Entry> entries;

  private:
    QChar at(qsizetype i) const { return i < n ? s[i] : QChar(); }

    bool fail(const QString &message)
    {
        const qsizetype line = s.left(p).count(u'\n') + 1;
        error = QStringLiteral("%1 on line %2").arg(message).arg(line);
        return false;
    }

    qsizetype startOfLine(qsizetype i) const
    {
        while (i > 0 && s[i - 1] != u'\n')
            --i;
        return i;
    }

    void skipWs()
    {
        while (p < n && (s[p] == u' ' || s[p] == u'\t'))
            ++p;
    }

    void skipComment()
    {
        if (at(p) == u'#')
            while (p < n && s[p] != u'\n' && !(s[p] == u'\r' && at(p + 1) == u'\n'))
                ++p;
    }

    // Spaces, comments and newlines, as allowed inside arrays.
    void skipBlank()
    {
        while (p < n) {
            skipWs();
            if (at(p) == u'#')
                skipComment();
            else if (at(p) == u'\n' || at(p) == u'\r')
                ++p;
            else
                break;
        }
    }

    // After a header or value: optional spaces and comment, then a newline or the end.
    bool finishLine()
    {
        skipWs();
        skipComment();
        if (p >= n)
            return true;
        if (s[p] == u'\r' && at(p + 1) == u'\n') {
            p += 2;
            return true;
        }
        if (s[p] == u'\n') {
            ++p;
            return true;
        }
        return fail(QStringLiteral("unexpected text after a value"));
    }

    bool parseQuotedKeyPart(QString &out)
    {
        const QChar quote = s[p++];
        while (p < n && s[p] != quote) {
            if (s[p] == u'\n')
                return fail(QStringLiteral("unterminated key"));
            if (quote == u'"' && s[p] == u'\\') {
                const QChar c = at(p + 1);
                p += 2;
                switch (c.unicode()) {
                case 'b': out += u'\b'; break;
                case 't': out += u'\t'; break;
                case 'n': out += u'\n'; break;
                case 'f': out += u'\f'; break;
                case 'r': out += u'\r'; break;
                case 'u':
                case 'U': {
                    const int digits = c == u'u' ? 4 : 8;
                    bool ok = false;
                    const char32_t code = s.mid(p, digits).toUInt(&ok, 16);
                    if (!ok)
                        return fail(QStringLiteral("bad escape in a key"));
                    out += QString::fromUcs4(&code, 1);
                    p += digits;
                    break;
                }
                default: out += c;
                }
                continue;
            }
            out += s[p++];
        }
        if (p >= n)
            return fail(QStringLiteral("unterminated key"));
        ++p;
        return true;
    }

    bool parseKey(QStringList &parts)
    {
        while (true) {
            skipWs();
            QString part;
            if (at(p) == u'"' || at(p) == u'\'') {
                if (!parseQuotedKeyPart(part))
                    return false;
            } else {
                const qsizetype begin = p;
                while (p < n && (s[p].isLetterOrNumber() || s[p] == u'_' || s[p] == u'-'))
                    ++p;
                if (p == begin)
                    return fail(QStringLiteral("expected a key"));
                part = s.mid(begin, p - begin);
            }
            parts.append(part);
            skipWs();
            if (at(p) != u'.')
                return true;
            ++p;
        }
    }

    bool parseValue(Node &node)
    {
        node.begin = p;
        const QStringView rest = QStringView(s).mid(p);
        if (rest.startsWith(u"\"\"\"") || rest.startsWith(u"'''")) {
            const QChar quote = s[p];
            const QString delimiter(3, quote);
            p += 3;
            while (true) {
                if (p >= n)
                    return fail(QStringLiteral("unterminated string"));
                if (quote == u'"' && s[p] == u'\\') {
                    p += 2;
                } else if (QStringView(s).mid(p).startsWith(delimiter)) {
                    p += 3;
                    // Up to two quotes of the content may directly precede the delimiter.
                    for (int extra = 0; extra < 2 && at(p) == quote; ++extra)
                        ++p;
                    break;
                } else {
                    ++p;
                }
            }
        } else if (at(p) == u'"' || at(p) == u'\'') {
            const QChar quote = s[p++];
            while (p < n && s[p] != quote) {
                if (s[p] == u'\n')
                    return fail(QStringLiteral("unterminated string"));
                p += (quote == u'"' && s[p] == u'\\') ? 2 : 1;
            }
            if (p >= n)
                return fail(QStringLiteral("unterminated string"));
            ++p;
        } else if (at(p) == u'[') {
            node.kind = Node::Array;
            ++p;
            while (true) {
                skipBlank();
                if (at(p) == u']')
                    break;
                Node element;
                if (!parseValue(element))
                    return false;
                node.elements.push_back(std::move(element));
                skipBlank();
                if (at(p) == u',') {
                    ++p;
                    continue;
                }
                if (at(p) != u']')
                    return fail(QStringLiteral("expected , or ] in an array"));
            }
            ++p;
        } else if (at(p) == u'{') {
            node.kind = Node::Inline;
            ++p;
            while (true) {
                skipBlank();
                if (at(p) == u'}')
                    break;
                QStringList key;
                const qsizetype keyBegin = p;
                if (!parseKey(key))
                    return false;
                skipWs();
                if (at(p) != u'=')
                    return fail(QStringLiteral("expected = in an inline table"));
                ++p;
                skipWs();
                Node member;
                if (!parseValue(member))
                    return false;
                node.memberKeys.push_back(key);
                node.memberBegins.push_back(keyBegin);
                node.members.push_back(std::move(member));
                skipBlank();
                if (at(p) == u',') {
                    ++p;
                    continue;
                }
                if (at(p) != u'}')
                    return fail(QStringLiteral("expected , or } in an inline table"));
            }
            ++p;
        } else {
            // A number, boolean or date: up to a delimiter, less trailing spaces.
            static const QString stops = QStringLiteral(",]}#\r\n");
            while (p < n && !stops.contains(s[p]))
                ++p;
            qsizetype end = p;
            while (end > node.begin && (s[end - 1] == u' ' || s[end - 1] == u'\t'))
                --end;
            if (end == node.begin)
                return fail(QStringLiteral("expected a value"));
            node.end = end;
            return true;
        }
        node.end = p;
        return true;
    }

    const QString &s;
    const qsizetype n;
    qsizetype p = 0;
};

// ── Editing ─────────────────────────────────────────────────────────────────

// What the document says about a dotted path.
struct Definition {
    enum Kind {
        Value,        // key = value (a value inside an inline table included)
        Header,       // [path]
        Implicit,     // a table that only exists because [path.child] does
        Dotted,       // a table defined by dotted keys (a.b = 1 defines a)
        InlineDotted, // the same inside an inline table ({ a.b = 1 })
    } kind = Value;
    const Node *node = nullptr; // Value: the value; InlineDotted: the inline table
    int section = 0;            // Header, Dotted
    QStringList relative;       // Dotted, InlineDotted: the key prefix as written
};

QString pathKey(const QStringList &path)
{
    return path.join(QChar(0x1f));
}

struct Insertion {
    qsizetype pos;
    QString text;
};

class Editor {
  public:
    Editor(const QString &text, const Scanner &scanner)
        : s(text), sc(scanner)
    {
        const qsizetype newline = s.indexOf(u'\n');
        eol = newline > 0 && s[newline - 1] == u'\r' ? QStringLiteral("\r\n") : QStringLiteral("\n");
        index();
    }

    bool apply(const QVariantMap &values)
    {
        walk(values, {});
        if (!error.isEmpty())
            return false;
        collectInsertions();
        return true;
    }

    // Removes the dotted `keys` the document sets; a key it does not set is
    // skipped. A key = value line goes with its own trailing comment; comments
    // on lines of their own stay. A member of an inline table goes with its
    // comma. A table's [header] stays when its keys go.
    bool remove(const QStringList &keys)
    {
        m_verb = QStringLiteral("remove");
        for (const QString &key : keys) {
            removePath(key.split(u'.'));
            if (!error.isEmpty())
                return false;
        }
        collectRemovals();
        return true;
    }

    // Which of the dotted `keys` the document sets, each with its value as
    // written ("{…}" for a table), in the order given.
    QList<std::pair<QString, QString>> present(const QStringList &keys) const
    {
        QList<std::pair<QString, QString>> found;
        for (const QString &key : keys) {
            const QStringList path = key.split(u'.');
            if (inArrayOfTables(path))
                continue; // cannot be addressed one by one
            const auto it = defs.constFind(pathKey(path));
            if (it == defs.cend())
                continue;
            QString value = QStringLiteral("{…}");
            if (it->kind == Definition::Value) {
                value = text(*it->node).simplified();
                if (value.size() > 60)
                    value = value.left(59) + QChar(0x2026);
            }
            found.append({key, value});
        }
        return found;
    }

    QString result() const
    {
        // Apply from the back so earlier offsets stay valid. Edits at the same
        // position keep the order they were made in.
        std::vector<std::pair<int, const Insertion *>> order;
        for (int i = 0; i < int(edits.size()); ++i)
            order.push_back({i, &edits[i]});
        std::sort(order.begin(), order.end(), [](const auto &a, const auto &b) {
            if (a.second->pos != b.second->pos)
                return a.second->pos > b.second->pos;
            return a.first > b.first;
        });
        QString out = s;
        for (const auto &[i, edit] : order) {
            const qsizetype length = replaceLength.at(i);
            out.replace(edit->pos, length, edit->text);
        }
        return out;
    }

    QStringList changed;
    QString error;

  private:
    void index()
    {
        defs.insert(pathKey({}), {Definition::Header, nullptr, 0, {}});
        for (int i = 1; i < int(sc.sections.size()); ++i) {
            const Section &section = sc.sections[i];
            if (section.arrayOfTables) {
                arrays.insert(pathKey(section.path));
            } else {
                defs.insert(pathKey(section.path), {Definition::Header, nullptr, i, {}});
            }
            for (int j = 1; j < section.path.size(); ++j) {
                const QString key = pathKey(section.path.mid(0, j));
                if (!defs.contains(key))
                    defs.insert(key, {Definition::Implicit, nullptr, 0, {}});
            }
        }
        for (const Entry &entry : sc.entries) {
            const QStringList base = sc.sections[entry.section].path;
            for (int j = 1; j < entry.relative.size(); ++j) {
                const QString key = pathKey(base + entry.relative.mid(0, j));
                const auto it = defs.constFind(key);
                if (it == defs.cend() || it->kind == Definition::Implicit)
                    defs.insert(key, {Definition::Dotted, nullptr, entry.section,
                                      entry.relative.mid(0, j)});
            }
            entryOf.insert(&entry.value, &entry);
            indexValue(entry.path, entry.value);
        }
    }

    void indexValue(const QStringList &path, const Node &node)
    {
        defs.insert(pathKey(path), {Definition::Value, &node, 0, {}});
        if (node.kind != Node::Inline)
            return;
        for (size_t i = 0; i < node.members.size(); ++i) {
            const QStringList &key = node.memberKeys[i];
            for (int j = 1; j < key.size(); ++j) {
                const QString prefix = pathKey(path + key.mid(0, j));
                if (!defs.contains(prefix))
                    defs.insert(prefix, {Definition::InlineDotted, &node, 0, key.mid(0, j)});
            }
            memberOf.insert(&node.members[i], {&node, int(i)});
            indexValue(path + key, node.members[i]);
        }
    }

    bool inArrayOfTables(const QStringList &path) const
    {
        for (int j = 1; j <= path.size(); ++j) {
            if (arrays.contains(pathKey(path.mid(0, j))))
                return true;
        }
        return false;
    }

    QString text(const Node &node) const { return s.mid(node.begin, node.end - node.begin); }

    void fail(const QStringList &path, const QString &why)
    {
        if (error.isEmpty())
            error = QStringLiteral("cannot %1 %2: %3").arg(m_verb, path.join(u'.'), why);
    }

    void erase(qsizetype begin, qsizetype end)
    {
        edits.push_back({begin, QString()});
        replaceLength.push_back(end - begin);
    }

    void removePath(const QStringList &path)
    {
        if (inArrayOfTables(path))
            return fail(path, QStringLiteral("it is inside an array of tables ([[...]])"));
        const auto it = defs.constFind(pathKey(path));
        if (it == defs.cend())
            return; // not set in this document
        if (it->kind != Definition::Value) {
            // A table: remove what it holds. Its [header] stays, with any comments.
            for (const QString &child : childKeys(path))
                removePath(path + QStringList{child});
            return;
        }
        if (const Entry *entry = entryOf.value(it->node)) {
            if (!erasedLines.contains(entry)) {
                erasedLines.insert(entry);
                erase(entry->lineStart, entry->lineEnd);
            }
        } else if (const auto member = memberOf.constFind(it->node); member != memberOf.cend()) {
            memberRemovals[member->first].insert(member->second);
        }
        changed.append(path.join(u'.'));
    }

    void collectRemovals()
    {
        for (auto it = memberRemovals.cbegin(); it != memberRemovals.cend(); ++it) {
            const Node &node = *it.key();
            if (const Entry *entry = entryOf.value(&node); entry && erasedLines.contains(entry))
                continue; // the whole line goes anyway
            QStringList kept;
            for (size_t i = 0; i < node.members.size(); ++i) {
                if (!it.value().contains(int(i)))
                    kept.append(s.mid(node.memberBegins[i], node.members[i].end - node.memberBegins[i]));
            }
            if (kept.isEmpty()) {
                replace(node, QStringLiteral("{}"));
                continue;
            }
            // Keep the spacing inside the braces: "{ a = 1 }" stays "{ a = 1 }".
            const QString open = s.mid(node.begin + 1, node.memberBegins.front() - node.begin - 1);
            const qsizetype lastEnd = node.members.back().end;
            const QString close = s.mid(lastEnd, node.end - 1 - lastEnd).remove(u',');
            replace(node, u'{' + open + kept.join(QStringLiteral(", ")) + close + u'}');
        }
    }

    void replace(const Node &node, const QString &newText)
    {
        edits.push_back({node.begin, newText});
        replaceLength.push_back(node.end - node.begin);
    }

    void insert(qsizetype pos, const QString &newText)
    {
        edits.push_back({pos, newText});
        replaceLength.push_back(0);
    }

    void walk(const QVariantMap &values, const QStringList &prefix)
    {
        for (auto it = values.cbegin(); it != values.cend() && error.isEmpty(); ++it) {
            const QStringList path = prefix + QStringList{it.key()};
            if (it.value().metaType().id() == QMetaType::QVariantMap) {
                walk(it.value().toMap(), path);
            } else {
                writeLeaf(path, it.value());
            }
        }
    }

    // The direct child keys the document defines under a table path.
    QStringList childKeys(const QStringList &path) const
    {
        const QString prefix = path.isEmpty() ? QString() : pathKey(path) + QChar(0x1f);
        QStringList keys;
        for (auto it = defs.cbegin(); it != defs.cend(); ++it) {
            if (it.key().isEmpty() || !it.key().startsWith(prefix))
                continue;
            const QString child = it.key().mid(prefix.size()).section(QChar(0x1f), 0, 0);
            if (!keys.contains(child))
                keys.append(child);
        }
        keys.sort();
        return keys;
    }

    QHash<QString, QString> childTexts(const QStringList &path, const QStringList &keys) const
    {
        QHash<QString, QString> texts;
        for (const QString &key : keys) {
            const Definition d = defs.value(pathKey(path + QStringList{key}));
            if (d.kind == Definition::Value && d.node)
                texts.insert(key, text(*d.node));
        }
        return texts;
    }

    // Splits a compound value into the components a table in the document
    // holds (`keys`, sorted), e.g. QPointF -> {x, y}.
    static std::optional<QVariantMap> decompose(const QVariant &v,
                                                const QStringList &keys,
                                                const QHash<QString, QString> &texts)
    {
        const auto is = [&keys](std::initializer_list<const char *> names) {
            QStringList list;
            for (const char *name : names)
                list.append(QString::fromLatin1(name));
            return keys == list; // both sorted
        };
        switch (v.metaType().id()) {
        case QMetaType::QVariantMap: {
            const QVariantMap map = v.toMap();
            if (map.keys() == keys)
                return map;
            return std::nullopt;
        }
        case QMetaType::QPointF:
        case QMetaType::QPoint:
            if (is({"x", "y"})) {
                const QPointF p = v.toPointF();
                return QVariantMap{{"x", p.x()}, {"y", p.y()}};
            }
            break;
        case QMetaType::QVector2D:
            if (is({"x", "y"})) {
                const QVector2D p = v.value<QVector2D>();
                return QVariantMap{{"x", p.x()}, {"y", p.y()}};
            }
            break;
        case QMetaType::QSizeF:
        case QMetaType::QSize: {
            const QSizeF sz = v.toSizeF();
            if (is({"h", "w"}))
                return QVariantMap{{"w", sz.width()}, {"h", sz.height()}};
            if (is({"height", "width"}))
                return QVariantMap{{"width", sz.width()}, {"height", sz.height()}};
            break;
        }
        case QMetaType::QRectF:
        case QMetaType::QRect: {
            const QRectF r = v.toRectF();
            if (is({"h", "w", "x", "y"}))
                return QVariantMap{{"x", r.x()}, {"y", r.y()}, {"w", r.width()}, {"h", r.height()}};
            if (is({"height", "width", "x", "y"}))
                return QVariantMap{{"x", r.x()}, {"y", r.y()},
                                   {"width", r.width()}, {"height", r.height()}};
            if (is({"x1", "x2", "y1", "y2"}))
                return QVariantMap{{"x1", r.left()}, {"y1", r.top()},
                                   {"x2", r.right()}, {"y2", r.bottom()}};
            break;
        }
        case QMetaType::QVector3D:
            if (is({"x", "y", "z"})) {
                const QVector3D p = v.value<QVector3D>();
                return QVariantMap{{"x", p.x()}, {"y", p.y()}, {"z", p.z()}};
            }
            break;
        case QMetaType::QVector4D:
            if (is({"w", "x", "y", "z"})) {
                const QVector4D p = v.value<QVector4D>();
                return QVariantMap{{"x", p.x()}, {"y", p.y()}, {"z", p.z()}, {"w", p.w()}};
            }
            break;
        case QMetaType::QQuaternion:
            if (is({"scalar", "x", "y", "z"})) {
                const QQuaternion q = v.value<QQuaternion>();
                return QVariantMap{{"scalar", q.scalar()}, {"x", q.x()}, {"y", q.y()}, {"z", q.z()}};
            }
            break;
        case QMetaType::QColor: {
            const QColor c = v.value<QColor>();
            const bool hasAlpha = is({"a", "b", "g", "r"});
            if (!hasAlpha && !(is({"b", "g", "r"}) && c.alpha() == 255))
                break;
            // Keep the file's scale: all channels within 0-1 means unit floats.
            bool unit = true;
            for (const QString &key : keys) {
                bool ok = false;
                const double value = texts.value(key).toDouble(&ok);
                if (!ok || value < 0.0 || value > 1.0)
                    unit = false;
            }
            const auto channel = [unit](int value8, float valueF) -> QVariant {
                if (unit)
                    return std::round(double(valueF) * 1000.0) / 1000.0;
                return value8;
            };
            QVariantMap map{{"r", channel(c.red(), c.redF())},
                            {"g", channel(c.green(), c.greenF())},
                            {"b", channel(c.blue(), c.blueF())}};
            if (hasAlpha)
                map.insert("a", channel(c.alpha(), c.alphaF()));
            return map;
        }
        default:
            break;
        }
        return std::nullopt;
    }

    void writeLeaf(const QStringList &path, const QVariant &value)
    {
        if (inArrayOfTables(path))
            return fail(path, QStringLiteral("it is inside an array of tables ([[...]])"));
        const auto it = defs.constFind(pathKey(path));
        if (it == defs.cend())
            return insertMissing(path, value);
        if (it->kind == Definition::Value) {
            const int before = int(changed.size());
            replaceValue(*it->node, value, path);
            if (changed.size() > before + 1)
                changed.erase(changed.begin() + before + 1, changed.end());
            return;
        }
        // The document defines a table here: write the value key by key.
        const QStringList keys = childKeys(path);
        const auto parts = decompose(value, keys, childTexts(path, keys));
        if (!parts)
            return fail(path, QStringLiteral("the file holds a table there"));
        for (auto p = parts->cbegin(); p != parts->cend() && error.isEmpty(); ++p)
            writeLeaf(path + QStringList{p.key()}, p.value());
    }

    static bool isLegacyMetadata(const Node &node)
    {
        if (node.kind != Node::Array || node.elements.size() != 2
            || node.elements[1].kind != Node::Inline)
            return false;
        for (const QStringList &key : node.elements[1].memberKeys) {
            if (key == QStringList{QStringLiteral("type")})
                return true;
        }
        return false;
    }

    // Writes `value` over `node`, as little of it as needed. Records `path` in
    // `changed` if anything is written (nested calls may add more; writeLeaf
    // keeps only the first).
    void replaceValue(const Node &node, const QVariant &value, const QStringList &path)
    {
        const QString old = text(node);
        const QVariant current = parseTomlValue(old);
        if (current.isValid() && current == value)
            return;
        changed.append(path.join(u'.'));

        if (node.kind == Node::Inline) {
            QStringList keys;
            QHash<QString, QString> texts;
            bool simple = true;
            for (size_t i = 0; i < node.members.size(); ++i) {
                if (node.memberKeys[i].size() != 1)
                    simple = false;
                else {
                    keys.append(node.memberKeys[i].first());
                    texts.insert(keys.last(), text(node.members[i]));
                }
            }
            keys.sort();
            if (simple && !keys.isEmpty()) {
                if (const auto parts = decompose(value, keys, texts)) {
                    for (size_t i = 0; i < node.members.size(); ++i) {
                        const QString key = node.memberKeys[i].first();
                        replaceValue(node.members[i], parts->value(key), path + QStringList{key});
                    }
                    return;
                }
            }
            return replace(node, formatTomlValue(value));
        }

        if (node.kind == Node::Array) {
            const bool isList = value.metaType().id() == QMetaType::QVariantList
                                || value.metaType().id() == QMetaType::QStringList;
            if (isLegacyMetadata(node)) {
                // [value, {type = ...}]: the value slot carries it, unless it is a
                // colour or rect, which is written in today's form instead.
                const int id = value.metaType().id();
                if (id == QMetaType::QColor || id == QMetaType::QRectF)
                    return replace(node, formatTomlValue(value));
                return replaceValue(node.elements[0], value, path);
            }
            if (!isList)
                return replace(node, formatTomlValue(value));
            const QVariantList list = value.toList();
            // [[1, 2]] is read as [1, 2]: write into the inner array.
            if (node.elements.size() == 1 && node.elements[0].kind == Node::Array
                && !(list.size() == 1 && list[0].metaType().id() == QMetaType::QVariantList))
                return replaceValue(node.elements[0], value, path);
            if (list.size() == qsizetype(node.elements.size())) {
                for (qsizetype i = 0; i < list.size(); ++i)
                    replaceValue(node.elements[size_t(i)], list[i],
                                 path + QStringList{QString::number(i)});
                return;
            }
            return replace(node, formatArrayLike(node, list));
        }

        replace(node, formatScalarLike(old, value));
    }

    // A scalar in the style of `old`: its quotes, its integer or float form.
    static QString formatScalarLike(const QString &old, const QVariant &value)
    {
        if (isStringLiteral(old)) {
            if (const auto content = asStringContent(value))
                return quoteLike(*content, old);
            return formatTomlValue(value);
        }
        if (isNumber(value)) {
            if (isFloating(value.metaType()))
                return formatNumber(value, looksInteger(old));
            return formatNumber(value, looksInteger(old) || !old.contains(u'.'));
        }
        return formatTomlValue(value);
    }

    QString indentBefore(qsizetype pos) const
    {
        qsizetype start = pos;
        while (start > 0 && (s[start - 1] == u' ' || s[start - 1] == u'\t'))
            --start;
        if (start > 0 && s[start - 1] != u'\n')
            return {}; // something else precedes pos on its line
        return s.mid(start, pos - start);
    }

    QString lineIndent(qsizetype pos) const
    {
        qsizetype start = pos;
        while (start > 0 && s[start - 1] != u'\n')
            --start;
        qsizetype end = start;
        while (end < s.size() && (s[end] == u' ' || s[end] == u'\t'))
            ++end;
        return s.mid(start, end - start);
    }

    // An array of another length, laid out like `node`.
    QString formatArrayLike(const Node &node, const QVariantList &list) const
    {
        QStringList items;
        for (qsizetype i = 0; i < list.size(); ++i) {
            const Node *style = nullptr;
            if (i < qsizetype(node.elements.size()))
                style = &node.elements[size_t(i)];
            else if (!node.elements.empty())
                style = &node.elements.front();
            const bool sameKind = style && style->kind == Node::Scalar
                                  && isStringLiteral(text(*style))
                                         == (list[i].metaType().id() == QMetaType::QString);
            items.append(sameKind ? formatScalarLike(text(*style), list[i])
                                  : formatTomlValue(list[i]));
        }
        if (items.isEmpty())
            return QStringLiteral("[]");

        const QString old = text(node);
        if (!old.contains(u'\n')) {
            const bool padded = old.startsWith(QStringLiteral("[ "));
            return padded ? QStringLiteral("[ ") + items.join(QStringLiteral(", ")) + QStringLiteral(" ]")
                          : u'[' + items.join(QStringLiteral(", ")) + u']';
        }
        QString indent = node.elements.empty() ? QString() : indentBefore(node.elements.front().begin);
        if (indent.isEmpty())
            indent = lineIndent(node.begin) + QStringLiteral("    ");
        const QString closeIndent = indentBefore(node.end - 1);
        bool trailingComma = false;
        if (!node.elements.empty()) {
            const QString tail = s.mid(node.elements.back().end,
                                       node.end - 1 - node.elements.back().end);
            trailingComma = tail.trimmed().startsWith(u',');
        }
        QString out = u'[' + eol;
        for (qsizetype i = 0; i < items.size(); ++i) {
            out += indent + items[i];
            if (i + 1 < items.size() || trailingComma)
                out += u',';
            out += eol;
        }
        return out + closeIndent + u']';
    }

    void insertMissing(const QStringList &path, const QVariant &value)
    {
        const QStringList parent = path.mid(0, path.size() - 1);
        const QString line = QStringLiteral(" = ") + formatTomlValue(value);
        for (qsizetype length = parent.size(); length >= 0; --length) {
            const QStringList ancestor = parent.mid(0, length);
            const QStringList rest = path.mid(length); // key relative to ancestor
            const auto it = defs.constFind(pathKey(ancestor));
            if (it == defs.cend())
                continue;
            changed.append(path.join(u'.'));
            switch (it->kind) {
            case Definition::Value:
                if (it->node->kind != Node::Inline)
                    return fail(path, QStringLiteral("%1 is a value, not a table")
                                          .arg(ancestor.join(u'.')));
                inlineAppends[it->node].append(formatKey(rest) + line);
                return;
            case Definition::InlineDotted:
                inlineAppends[it->node].append(formatKey(it->relative + rest) + line);
                return;
            case Definition::Dotted: {
                // Next to the last dotted key that defines this table.
                const Entry *last = nullptr;
                for (const Entry &entry : sc.entries) {
                    if (entry.section == it->section
                        && entry.relative.mid(0, it->relative.size()) == it->relative)
                        last = &entry;
                }
                entryAppends[last].append(formatKey(it->relative + rest) + line);
                return;
            }
            case Definition::Header:
                if (length == parent.size()) {
                    sectionAppends[it->section].append(formatKey(rest) + line);
                    return;
                }
                break;
            case Definition::Implicit:
                break;
            }
            // The table itself is missing: add it at the end of the file.
            const QString name = pathKey(parent);
            if (!newTables.contains(name))
                newTableOrder.append(parent);
            newTables[name].append(formatKeyPart(path.last()) + line);
            return;
        }
    }

    bool endsWithNewline(qsizetype pos) const { return pos == 0 || s[pos - 1] == u'\n'; }

    void collectInsertions()
    {
        // Keys appended to existing tables, after each table's last key.
        for (auto it = sectionAppends.cbegin(); it != sectionAppends.cend(); ++it) {
            const int section = it.key();
            const Entry *last = nullptr;
            for (const Entry &entry : sc.entries) {
                if (entry.section == section)
                    last = &entry;
            }
            QString indent;
            qsizetype pos = 0;
            bool separate = false; // add a blank line after (root table before the first header)
            if (last) {
                pos = last->lineEnd;
                indent = lineIndent(last->lineStart);
            } else if (section > 0) {
                pos = sc.sections[size_t(section)].headerLineEnd;
            } else if (sc.sections.size() == 1) {
                pos = s.size();
            } else {
                // A root table with no keys: before the first header and the
                // comment lines directly above it.
                pos = sc.sections[1].headerStart;
                while (pos > 0) {
                    qsizetype previous = pos - 1;
                    while (previous > 0 && s[previous - 1] != u'\n')
                        --previous;
                    if (previous == 0 || !s.mid(previous, pos - previous).trimmed().startsWith(u'#'))
                        break;
                    pos = previous;
                }
                separate = true;
            }
            QString block = endsWithNewline(pos) ? QString() : eol;
            for (const QString &line : it.value())
                block += indent + line + eol;
            if (separate)
                block += eol;
            if (pos == s.size())
                insertedAtEnd = true;
            insert(pos, block);
        }

        // Dotted keys appended after their siblings.
        for (auto it = entryAppends.cbegin(); it != entryAppends.cend(); ++it) {
            const Entry *entry = it.key();
            const QString indent = lineIndent(entry->lineStart);
            QString block = endsWithNewline(entry->lineEnd) ? QString() : eol;
            for (const QString &line : it.value())
                block += indent + line + eol;
            insert(entry->lineEnd, block);
        }

        // Keys appended to inline tables, before the closing brace.
        for (auto it = inlineAppends.cbegin(); it != inlineAppends.cend(); ++it) {
            const Node &node = *it.key();
            if (node.members.empty()) {
                replace(node, QStringLiteral("{ ") + it.value().join(QStringLiteral(", "))
                                  + QStringLiteral(" }"));
            } else {
                insert(node.members.back().end, QStringLiteral(", ") + it.value().join(QStringLiteral(", ")));
            }
        }

        // New tables at the end of the file.
        if (!newTableOrder.isEmpty()) {
            std::sort(newTableOrder.begin(), newTableOrder.end(),
                      [](const QStringList &a, const QStringList &b) { return a.join(u'.') < b.join(u'.'); });
            QString block;
            const bool empty = s.trimmed().isEmpty();
            if (!empty) {
                if (!endsWithNewline(s.size()))
                    block += eol;
                if (!s.endsWith(QStringLiteral("\n\n")) && !s.endsWith(QStringLiteral("\n\r\n")))
                    block += eol;
            } else if (insertedAtEnd) {
                block += eol; // a blank line after the root keys just added
            }
            for (int i = 0; i < newTableOrder.size(); ++i) {
                if (i > 0)
                    block += eol;
                const QStringList &table = newTableOrder[i];
                block += u'[' + formatKey(table) + u']' + eol;
                for (const QString &line : newTables.value(pathKey(table)))
                    block += line + eol;
            }
            insert(s.size(), block);
        }
    }

    const QString &s;
    const Scanner &sc;
    QString eol;
    QHash<QString, Definition> defs;
    QHash<const Node *, const Entry *> entryOf;                 // a key = value line's value
    QHash<const Node *, std::pair<const Node *, int>> memberOf; // inline table and index
    QSet<const Entry *> erasedLines;
    QHash<const Node *, QSet<int>> memberRemovals;
    QString m_verb = QStringLiteral("write");
    QSet<QString> arrays;
    std::vector<Insertion> edits;
    std::vector<qsizetype> replaceLength;
    QMap<int, QStringList> sectionAppends;
    QMap<const Entry *, QStringList> entryAppends;
    bool insertedAtEnd = false;
    QMap<const Node *, QStringList> inlineAppends;
    QHash<QString, QStringList> newTables;
    QList<QStringList> newTableOrder;
};

QString tomlParseError(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    try {
        (void)toml::parse(std::string_view(utf8.constData(), size_t(utf8.size())));
    } catch (const toml::parse_error &e) {
        return QStringLiteral("%1 (line %2)")
            .arg(QString::fromUtf8(e.description().data(), qsizetype(e.description().size())))
            .arg(e.source().begin.line);
    }
    return {};
}

} // namespace

TomlEditResult editToml(const QString &text, const QVariantMap &values)
{
    TomlEditResult result;
    result.text = text;

    if (const QString parseError = tomlParseError(text); !parseError.isEmpty()) {
        result.error = QStringLiteral("the file is not valid TOML: %1").arg(parseError);
        return result;
    }
    Scanner scanner(text);
    if (!scanner.scan()) {
        result.error = QStringLiteral("could not read the file: %1").arg(scanner.error);
        return result;
    }
    Editor editor(text, scanner);
    if (!editor.apply(values)) {
        result.error = editor.error;
        return result;
    }
    const QString edited = editor.result();
    if (const QString parseError = tomlParseError(edited); !parseError.isEmpty()) {
        result.error = QStringLiteral("the edit would not be valid TOML: %1").arg(parseError);
        return result;
    }
    result.text = edited;
    result.changed = editor.changed;
    return result;
}

TomlEditResult removeFromToml(const QString &text, const QStringList &keys)
{
    TomlEditResult result;
    result.text = text;

    if (const QString parseError = tomlParseError(text); !parseError.isEmpty()) {
        result.error = QStringLiteral("the file is not valid TOML: %1").arg(parseError);
        return result;
    }
    Scanner scanner(text);
    if (!scanner.scan()) {
        result.error = QStringLiteral("could not read the file: %1").arg(scanner.error);
        return result;
    }
    Editor editor(text, scanner);
    if (!editor.remove(keys)) {
        result.error = editor.error;
        return result;
    }
    const QString edited = editor.result();
    if (const QString parseError = tomlParseError(edited); !parseError.isEmpty()) {
        result.error = QStringLiteral("the edit would not be valid TOML: %1").arg(parseError);
        return result;
    }
    result.text = edited;
    result.changed = editor.changed;
    return result;
}

QList<std::pair<QString, QString>> tomlKeysSet(const QString &text, const QStringList &keys)
{
    Scanner scanner(text);
    if (!scanner.scan())
        return {};
    return Editor(text, scanner).present(keys);
}

QString removeKeysFromFile(const QString &filePath, const QStringList &keys, QStringList *removed)
{
    if (removed)
        removed->clear();
    QFile in(filePath);
    if (!in.exists())
        return {};
    if (!in.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read '%1': %2").arg(filePath, in.errorString());
    const QString text = QString::fromUtf8(in.readAll());
    in.close();

    const TomlEditResult edit = removeFromToml(text, keys);
    if (!edit.error.isEmpty())
        return QStringLiteral("Cannot update '%1': %2").arg(filePath, edit.error);
    if (removed)
        *removed = edit.changed;
    if (edit.text == text)
        return {};

    // No QIODevice::Text: line endings are the file's own and must stay as they are.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return QStringLiteral("Cannot open '%1' for writing: %2").arg(filePath, file.errorString());
    file.write(edit.text.toUtf8());
    if (!file.commit())
        return QStringLiteral("Write failed for '%1': %2").arg(filePath, file.errorString());
    return {};
}

} // namespace dsqt::settings
