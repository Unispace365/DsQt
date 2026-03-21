#include <QTest>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "core/dsGuiApplication.h"
#include "core/dsVersion.h"

class DsLoggingTest : public QObject
{
    Q_OBJECT

private slots:
    // -- resolveLogPath tests --

    void resolvePath_emptyReturnsDefault();
    void resolvePath_fullFilePathUsedVerbatim();
    void resolvePath_directoryAppendsDefaultName();
    void resolvePath_dottedDirectoryOnDiskAppendsDefaultName();
    void resolvePath_dottedPathNotOnDiskTreatedAsFile();

    // -- dsVersion.h tests --

    void version_macroIsDefined();
    void version_majorMinorPatchAreDefined();
};

// ---------------------------------------------------------------------------
// resolveLogPath
// ---------------------------------------------------------------------------

void DsLoggingTest::resolvePath_emptyReturnsDefault()
{
    const QString result = DsGuiApplication::resolveLogPath(
        QString(), QStringLiteral("App.log.txt"),
        QStringLiteral("C:/default/path/App.log.txt"));

    QCOMPARE(result, QStringLiteral("C:/default/path/App.log.txt"));
}

void DsLoggingTest::resolvePath_fullFilePathUsedVerbatim()
{
    const QString result = DsGuiApplication::resolveLogPath(
        QStringLiteral("C:/custom/logs/output.log.txt"),
        QStringLiteral("App.log.txt"),
        QStringLiteral("C:/default/path/App.log.txt"));

    QCOMPARE(result, QStringLiteral("C:/custom/logs/output.log.txt"));
}

void DsLoggingTest::resolvePath_directoryAppendsDefaultName()
{
    const QString result = DsGuiApplication::resolveLogPath(
        QStringLiteral("C:/custom/logs"),
        QStringLiteral("App.log.txt"),
        QStringLiteral("C:/default/path/App.log.txt"));

    QCOMPARE(result, QStringLiteral("C:/custom/logs/App.log.txt"));
}

void DsLoggingTest::resolvePath_dottedDirectoryOnDiskAppendsDefaultName()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Create a directory with a dot in its name
    const QString dottedDir = tmpDir.path() + QStringLiteral("/my.logs");
    QVERIFY(QDir().mkpath(dottedDir));

    const QString result = DsGuiApplication::resolveLogPath(
        dottedDir, QStringLiteral("App.log.txt"),
        QStringLiteral("C:/default/path/App.log.txt"));

    QCOMPARE(result, dottedDir + QStringLiteral("/App.log.txt"));
}

void DsLoggingTest::resolvePath_dottedPathNotOnDiskTreatedAsFile()
{
    // A path with an extension that does NOT exist on disk as a directory
    // should be treated as a file path
    const QString result = DsGuiApplication::resolveLogPath(
        QStringLiteral("C:/nonexistent/my.logs"),
        QStringLiteral("App.log.txt"),
        QStringLiteral("C:/default/path/App.log.txt"));

    QCOMPARE(result, QStringLiteral("C:/nonexistent/my.logs"));
}

// ---------------------------------------------------------------------------
// dsVersion.h
// ---------------------------------------------------------------------------

void DsLoggingTest::version_macroIsDefined()
{
    const QString version = QStringLiteral(DSQT_VERSION);
    QVERIFY2(!version.isEmpty(), "DSQT_VERSION should not be empty");
    // Version string should look like "X.Y.Z"
    QVERIFY2(version.count('.') >= 2,
             qPrintable(QStringLiteral("Expected semver format, got: ") + version));
}

void DsLoggingTest::version_majorMinorPatchAreDefined()
{
    // These should be integer literals, verify they are non-negative
    QVERIFY2(DSQT_VERSION_MAJOR >= 0, "DSQT_VERSION_MAJOR should be non-negative");
    QVERIFY2(DSQT_VERSION_MINOR >= 0, "DSQT_VERSION_MINOR should be non-negative");
    QVERIFY2(DSQT_VERSION_PATCH >= 0, "DSQT_VERSION_PATCH should be non-negative");

    // Verify consistency between string and numeric macros
    const QString expected = QStringLiteral("%1.%2.%3")
        .arg(DSQT_VERSION_MAJOR).arg(DSQT_VERSION_MINOR).arg(DSQT_VERSION_PATCH);
    QCOMPARE(QStringLiteral(DSQT_VERSION), expected);
}

QTEST_MAIN(DsLoggingTest)
#include "tst_dsloggingtest.moc"
