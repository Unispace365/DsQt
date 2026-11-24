#pragma once

#include <utility>

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace dsqt::settings {

// The result of editToml().
struct TomlEditResult {
    QString     text;    // the edited text; the input itself when nothing changed or on error
    QStringList changed; // dotted keys whose value was written, in the order they were applied
    QString     error;   // non-empty if the text could not be edited; `text` is then the input
};

// Writes `values` into the TOML document `text` and changes nothing else.
//
// `values` is a nested map of leaf values, as SettingsFile::overrides() returns
// it: {"window": {"width": 1024}} writes window.width. toml++ cannot write a
// document back with its comments, so this works on the text itself:
//
//  - A key that already holds an equal value is left alone.
//  - A changed value is replaced where it stands, on its own line, keeping its
//    quoting style ('literal' or "basic") and, for an integer-looking number, its
//    integer form. Comments, blank lines, key order and line endings stay as
//    they are, byte for byte.
//  - An inline table ({ x = 1, y = 2 }) and a table of components ([pos] x = 1)
//    are changed component by component, so pos = QPointF(1, 3) only rewrites
//    the 2. An array of the same length is changed element by element; one of
//    another length is written anew, on one line or one element per line as
//    the original was.
//  - A missing key is appended to the end of its table (or of the inline table
//    or dotted keys that define its parent). A missing table is appended at the
//    end of the file as a [table] header.
//  - New lines use the line ending the document already uses (CRLF or LF).
//
// Keys inside an array of tables ([[slides]]) cannot be addressed and are
// reported as an error, as are values the document defines as a table but
// that do not split into its keys. The document must parse as TOML, and so
// must the result; otherwise the error is set and `text` is returned as given.
TomlEditResult editToml(const QString &text, const QVariantMap &values);

// Removes the dotted `keys` from the TOML document `text` and changes nothing
// else; a key the document does not set is skipped. A `key = value` line goes
// with its own trailing comment, while comments on lines of their own stay. A
// member of an inline table goes with its comma ({ a = 1, b = 2 } -> { a = 1 }).
// For a key the document holds as a table ([pos] x = 1 y = 2), the keys in it
// go and the [pos] header stays. `changed` lists the keys removed. Keys inside
// an array of tables are reported as an error.
TomlEditResult removeFromToml(const QString &text, const QStringList &keys);

// Which of the dotted `keys` the TOML document `text` sets, each with its value
// as written ("100", "'square'"; "{…}" for a table), in the order given.
// Returns an empty list if `text` cannot be read.
QList<std::pair<QString, QString>> tomlKeysSet(const QString &text, const QStringList &keys);

// removeFromToml() on the file at `filePath`, written back in place. A file
// that does not exist sets nothing and is left alone. `removed` receives the
// keys removed. Returns an empty string on success, or what went wrong; the
// file is then untouched.
QString removeKeysFromFile(const QString &filePath, const QStringList &keys,
                           QStringList *removed = nullptr);

// Formats `value` as a TOML value: QPointF(1, 2) -> "{ x = 1, y = 2 }",
// QColor -> "\"#rrggbb\"", a list -> "[1, 2]", and so on.
QString formatTomlValue(const QVariant &value);

// Parses one TOML value ("[1, 2]", "'text'", "{ x = 1, y = 2 }") the way
// SettingsFile reads a settings file, so colours and points come back typed.
// Returns an invalid QVariant if `text` is not a value. Defined in dsSettingsFile.cpp.
QVariant parseTomlValue(const QString &text);

} // namespace dsqt::settings
