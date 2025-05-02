// git_ignore_checker.h
#ifndef GIT_IGNORE_CHECKER_H
#define GIT_IGNORE_CHECKER_H

#pragma once

#include <QString>
#include <QVector>
#include <QRegularExpression>

struct GitIgnorePattern {
    bool negation;               // true if line started with '!'
    QRegularExpression regex;    // the converted pattern
};

class GitIgnoreChecker
{
public:
    /// Load and parse the .gitignore at `ignoreFilePath`
    explicit GitIgnoreChecker(const QString& ignoreFilePath);

    /// Returns true if `filePath` would be ignored by these rules
    bool isIgnored(const QString& filePath) const;

private:
    QVector<GitIgnorePattern> patterns;
    bool m_isLoaded = false;

    /// Convert one gitignore glob into a QRegularExpression
    static QString convertPatternToRegex(const QString& rawPattern);
};

#endif // GIT_IGNORE_CHECKER_H
