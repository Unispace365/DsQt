#include <QTest>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QFile>
#include <QLoggingCategory>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include "core/dsEnvironment.h"
#include "settings/dsSettings.h"

#include "core/dsGuiApplication.h"
#include "core/dsVersion.h"

class DsLoggingTest : public QObject
{
    Q_OBJECT

private slots:
    void engineRules_followMergedSettings();
    void outputCounts_data();
    void outputCounts();
    void unwritableLogPath_data();
    void unwritableLogPath();
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

static bool writeFixture(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

void DsLoggingTest::unwritableLogPath_data()
{
    QTest::addColumn<bool>("consoleEnabled");
    QTest::newRow("existing-console") << true;
    QTest::newRow("file-only-falls-back-to-console") << false;
}

void DsLoggingTest::unwritableLogPath()
{
    QFETCH(bool, consoleEnabled);
    QTemporaryDir fixtures;
    QVERIFY(fixtures.isValid());
    QVERIFY(QDir().mkpath(fixtures.path() + "/settings"));
    QVERIFY(writeFixture(fixtures.path() + "/blocked", "not a directory"));
    const QString probeName = QStringLiteral("ds_logging_probe")
#ifdef Q_OS_WIN
        + QStringLiteral(".exe")
#endif
        ;
    const QString originalDir = QCoreApplication::applicationDirPath();
    const QString probe = fixtures.path() + '/' + probeName;
    QVERIFY(QFile::copy(originalDir + '/' + probeName, probe));
    QVERIFY(writeFixture(fixtures.path() + "/settings/logging.ini",
        "[logger]\nasync=true\nplatform_std_log=" + QByteArray(consoleEnabled ? "true" : "false")
        + "\npath=" + fixtures.path().toUtf8() + "/blocked/probe.log\n"));
    QProcess process;
    auto env = QProcessEnvironment::systemEnvironment();
    env.remove("QT_LOGGING_RULES");
    env.remove("QT_LOGGING_CONF");
    env.insert("PATH", originalDir + QDir::listSeparator() + env.value("PATH"));
    process.setProcessEnvironment(env);
    process.start(probe, {});
    const bool finished = process.waitForFinished(15000);
    if (!finished) {
        process.kill();
        process.waitForFinished();
    }
    QVERIFY2(finished, qPrintable(process.errorString()));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    const QByteArray err = process.readAllStandardError();
    QCOMPARE(err.count("DsQt file logging disabled:"), 1);
    QCOMPARE(err.count("DSQT_PROBE_VISIBLE"), 1);
    QVERIFY(!err.contains("device not open"));
    QVERIFY(!err.contains("FileSink: Can't open"));
    QVERIFY(!QFileInfo::exists(fixtures.path() + "/blocked/probe.log"));
    QVERIFY(err.contains("DSQT_PROBE_LOG_PATH \n") || err.contains("DSQT_PROBE_LOG_PATH \r\n"));
}

void DsLoggingTest::engineRules_followMergedSettings()
{
    QVERIFY(dsqt::DsEnvironment::loadEngineSettings());
    auto *engine = dsqt::DsEnvironment::engineSettings();
    const QString oldFile = engine->fileName();
    const QStringList oldPaths = dsqt::Settings::instance().searchPaths();
    QTemporaryDir fixtures;
    QVERIFY(fixtures.isValid());
    QVERIFY(QDir().mkpath(fixtures.path() + "/higher"));
    const QString primary = fixtures.path() + "/engine.toml";
    QVERIFY(writeFixture(primary, "[engine.logging]\nrules = [[\"dsqt.test.logging=false\"]]\n"));
    QVERIFY(writeFixture(fixtures.path() + "/higher/engine.toml",
                         "[engine.logging]\nrules = [\"dsqt.test.logging=true\"]\n"));
    dsqt::Settings::instance().setSearchPaths({fixtures.path(), fixtures.path() + "/higher"});
    engine->setFileName("engine.toml");
    QLoggingCategory category("dsqt.test.logging");
    QVERIFY(category.isInfoEnabled()); // Later search path wins.
    QVERIFY(writeFixture(fixtures.path() + "/extra.toml",
                         "[engine.logging]\nrules = [[\"dsqt.test.logging=false\"]]\n"));
    engine->setExtraFiles({"extra.toml"});
    QVERIFY(!category.isInfoEnabled()); // Extras override primary files.
    engine->setOverride("engine.logging.rules", QStringList{"dsqt.test.logging=true"});
    QVERIFY(category.isInfoEnabled());
    engine->reload();
    QVERIFY(category.isInfoEnabled()); // Runtime override survives reload.
    engine->resetOverride("engine.logging.rules");
    QVERIFY(!category.isInfoEnabled());
    QVERIFY(writeFixture(fixtures.path() + "/extra.toml",
                         "[engine.logging]\nrules = [\"dsqt.test.logging=true\"]\n"));
    QTRY_VERIFY(category.isInfoEnabled()); // Watcher applies edits without explicit reload.
    QVERIFY(writeFixture(fixtures.path() + "/extra.toml", "[engine.logging]\nrules = []\n"));
    engine->reload();
    QVERIFY(category.isInfoEnabled()); // Empty rules clear the previous application rules.
    engine->setExtraFiles({});
    engine->setFileName(oldFile);
    dsqt::Settings::instance().setSearchPaths(oldPaths);
    QLoggingCategory::setFilterRules({});
}

void DsLoggingTest::outputCounts_data()
{
    QTest::addColumn<QByteArray>("ini");
    QTest::addColumn<QByteArray>("envRules");
    QTest::addColumn<bool>("confEnabled");
    QTest::addColumn<int>("stdoutCount");
    QTest::addColumn<int>("stderrCount");
    QTest::addColumn<int>("categoryCount");
    QTest::newRow("no-ini") << QByteArray() << QByteArray() << false << 0 << 1 << 0;
    QTest::newRow("standard-ini") << QByteArray("platform_std_log=true\n") << QByteArray() << false << 0 << 1 << 0;
    QTest::newRow("two-streams") << QByteArray("stdout=true\nplatform_std_log=true\n") << QByteArray() << false << 1 << 1 << 0;
    QTest::newRow("two-stderr-sinks") << QByteArray("stderr=true\nplatform_std_log=true\n") << QByteArray() << false << 0 << 2 << 0;
    QTest::newRow("qt-conf-overrides-engine") << QByteArray("platform_std_log=true\n") << QByteArray() << true << 0 << 1 << 1;
    QTest::newRow("qt-env-overrides-conf") << QByteArray("platform_std_log=true\n") << QByteArray("dsqt.logging.probe.info=false") << true << 0 << 1 << 0;
    QTest::newRow("qt-env-overrides-engine") << QByteArray("platform_std_log=true\n") << QByteArray("dsqt.logging.probe.info=true") << false << 0 << 1 << 1;
}

void DsLoggingTest::outputCounts()
{
    QFETCH(QByteArray, ini);
    QFETCH(QByteArray, envRules);
    QFETCH(bool, confEnabled);
    QFETCH(int, stdoutCount);
    QFETCH(int, stderrCount);
    QFETCH(int, categoryCount);
    QTemporaryDir fixtures;
    QVERIFY(fixtures.isValid());
    QVERIFY(QDir().mkpath(fixtures.path() + "/settings"));
    const QString probeName = QStringLiteral("ds_logging_probe")
#ifdef Q_OS_WIN
        + QStringLiteral(".exe")
#endif
        ;
    const QString originalDir = QCoreApplication::applicationDirPath();
    const QString probe = fixtures.path() + '/' + probeName;
    QVERIFY(QFile::copy(originalDir + '/' + probeName, probe));
    QVERIFY(writeFixture(fixtures.path() + "/settings/engine.toml",
                         "[engine.logging]\nrules = [[\"dsqt.logging.probe=false\", \"*.verbose=false\"]]\n"));
    if (!ini.isEmpty()) {
        QVERIFY(writeFixture(fixtures.path() + "/settings/logging.ini",
            "[logger]\nasync=true\npath=" + fixtures.path().toUtf8() + "/probe.log\n" + ini));
    }
    QProcess process;
    auto env = QProcessEnvironment::systemEnvironment();
    env.remove("QT_LOGGING_RULES");
    env.remove("QT_LOGGING_CONF");
    env.insert("PATH", originalDir + QDir::listSeparator() + env.value("PATH"));
    if (confEnabled) {
        const QString conf = fixtures.path() + "/qtlogging.ini";
        QVERIFY(writeFixture(conf, "[Rules]\ndsqt.logging.probe.info=true\n"));
        env.insert("QT_LOGGING_CONF", conf);
    }
    if (!envRules.isEmpty()) env.insert("QT_LOGGING_RULES", QString::fromUtf8(envRules));
    process.setProcessEnvironment(env);
    process.start(probe, {});
    const bool finished = process.waitForFinished(15000);
    if (!finished) {
        process.kill();
        process.waitForFinished();
    }
    QVERIFY2(finished, qPrintable(process.errorString()));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    const QByteArray out = process.readAllStandardOutput();
    const QByteArray err = process.readAllStandardError();
    QCOMPARE(out.count("DSQT_PROBE_VISIBLE"), stdoutCount);
    QCOMPARE(err.count("DSQT_PROBE_VISIBLE"), stderrCount);
    QCOMPARE(err.count("DSQT_PROBE_CATEGORY"), categoryCount * stderrCount);
    QCOMPARE((out + err).count("DSQT_PROBE_VERBOSE"), 0);
    const QByteArray marker("DSQT_PROBE_LOG_PATH ");
    const int start = err.indexOf(marker);
    QVERIFY(start >= 0);
    QString path = QString::fromUtf8(err.mid(start + marker.size()).split('\n').first()).trimmed();
    path.remove(QRegularExpression(QStringLiteral("\033\\[[0-9;]*m")));
    QFile log(path);
    QVERIFY2(log.open(QIODevice::ReadOnly), qPrintable(log.fileName()));
    const QByteArray contents = log.readAll();
    QCOMPARE(contents.count("DSQT_PROBE_VISIBLE"), 1);
    QCOMPARE(contents.count("DSQT_PROBE_CATEGORY"), categoryCount);
    QCOMPARE(contents.count("DSQT_PROBE_VERBOSE"), 0);
}

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
