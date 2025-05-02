// gitignorechecker.cpp
#include "git_ignore_checker.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>

GitIgnoreChecker::GitIgnoreChecker(const QString& ignoreFilePath)
{
    QFile f(ignoreFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Unable to open .gitignore:" << ignoreFilePath;
        return;
    }
    m_isLoaded = true;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        // strip BOM, whitespace, skip comments/empty
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        bool neg = false;
        if (line.startsWith('!')) {
            neg = true;
            line = line.mid(1);
        }

        QString rx = convertPatternToRegex(line);
        QRegularExpression re(rx);
        if (!re.isValid()) {
            qWarning() << "Invalid gitignore pattern:" << line
                       << "→" << re.errorString();
            continue;
        }

        patterns.append({ neg, re });
    }
}

bool GitIgnoreChecker::isIgnored(const QString& filePath) const
{
    if(!m_isLoaded) return false;
    // normalize path: forward‐slashes, clean, strip leading "./"
    QString path = QDir::cleanPath(filePath).replace('\\', '/');
    if (path.startsWith("./"))
        path = path.mid(2);

    bool ignored = false;
    for (const auto& pat : patterns) {
        if (pat.regex.match(path).hasMatch()) {
            // last match wins: negation un-ignores
            ignored = !pat.negation;
        }
    }
    return ignored;
}

QString GitIgnoreChecker::convertPatternToRegex(const QString& rawPattern)
{
    QString pat = rawPattern;
    const bool anchored = pat.startsWith('/');
    if (anchored) pat = pat.mid(1);

    const bool dirOnly = pat.endsWith('/');
    if (dirOnly) pat.chop(1);

    // escape regex metachars except our glob tokens (*, ?, /)
    static const QRegularExpression esc(R"(([\.\\\+\^\$\(\)\=\!\{\}\|\]]))");
    pat.replace(esc, R"(\\1)");

    // glob → regex
    pat.replace("**", "___GLOBSTAR___");
    pat.replace("*", "[^/]*");
    pat.replace("___GLOBSTAR___", ".*");
    pat.replace("?", ".");

    // build final regex
    QString rx = "^";
    if (!anchored) {
        // match at any directory level
        rx += "(?:.*/)?";
    }
    rx += pat;
    if (dirOnly) {
        // only directories (or their descendants)
        rx += "(?:/.*)?";
    }
    rx += "$";
    return rx;
}

