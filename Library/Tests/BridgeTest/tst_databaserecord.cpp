#include "testHelpers.h"

#include <QDir>
#include <QTest>
#include <QUrl>

using namespace dsqt::bridge;
using namespace testing;

// Tests for DatabaseRecord: typed accessors on top of a QVariantHash.
class DatabaseRecordTest : public QObject {
    Q_OBJECT

  private slots:
    void emptyRecord_hasSafeDefaults();
    void builtInAccessors();
    void rank();
    void parentAndChildUids();

    void variants_data();
    void variants();

    void days_data();
    void days();

    void startAndEnd();

    void list_data();
    void list();
    void list_customSeparator();

    void resource_data();
    void resource();

    void filepath_data();
    void filepath();
};

void DatabaseRecordTest::emptyRecord_hasSafeDefaults() {
    DatabaseRecord record;
    QVERIFY(record.isEmpty());
    QCOMPARE(record.uid(), QString());
    QCOMPARE(record.recordName(), QString());
    QCOMPARE(record.typeName(), QString());
    QCOMPARE(record.typeKey(), QString());
    QCOMPARE(record.rank(), -1);
    QVERIFY(record.parentUids().isEmpty());
    QVERIFY(record.childUids().isEmpty());
    QVERIFY(record.list("anything").isEmpty());
    QVERIFY(!record.isRoot());
    QVERIFY(!record.isEvent());
    QVERIFY(!record.isPlatform());
    QVERIFY(!record.isResource("anything"));
    QVERIFY(!record.start().isValid());
    QVERIFY(!record.end().isValid());
    QCOMPARE(record.days(), 0x7F);
}

void DatabaseRecordTest::builtInAccessors() {
    QVariantHash fields;
    fields.insert("record_name", "My record");
    fields.insert("type_name", "Playlist");
    fields.insert("type_key", "playlist");
    const DatabaseRecord record = makeRecord("abc123", fields);

    QCOMPARE(record.uid(), QString("abc123"));
    QCOMPARE(record.recordName(), QString("My record"));
    QCOMPARE(record.typeName(), QString("Playlist"));
    QCOMPARE(record.typeKey(), QString("playlist"));
}

void DatabaseRecordTest::rank() {
    QCOMPARE(makeRecord("a").rank(), -1);
    QCOMPARE(makeRecord("a", {{"rank", 7}}).rank(), 7);
}

void DatabaseRecordTest::parentAndChildUids() {
    const DatabaseRecord record =
        makeRecord("a", {{"parent_uid", QStringList{"p1", "p2"}}, {"child_uid", QStringList{"c1", "c2", "c3"}}});
    QCOMPARE(record.parentUids(), (QStringList{"p1", "p2"}));
    QCOMPARE(record.childUids(), (QStringList{"c1", "c2", "c3"}));
}

void DatabaseRecordTest::variants_data() {
    QTest::addColumn<QString>("variant");
    QTest::addColumn<bool>("isRoot");
    QTest::addColumn<bool>("isEvent");
    QTest::addColumn<bool>("isPlatform");

    QTest::newRow("root content") << "ROOT_CONTENT" << true << false << false;
    QTest::newRow("event") << "SCHEDULE" << false << true << false;
    QTest::newRow("platform") << "ROOT_PLATFORM" << false << false << true;
    QTest::newRow("regular record") << "" << false << false << false;
    QTest::newRow("unknown") << "SOMETHING_ELSE" << false << false << false;
    QTest::newRow("case sensitive") << "schedule" << false << false << false;
}

void DatabaseRecordTest::variants() {
    QFETCH(QString, variant);
    QFETCH(bool, isRoot);
    QFETCH(bool, isEvent);
    QFETCH(bool, isPlatform);

    const DatabaseRecord record = makeRecord("a", {{"variant", variant}});
    QCOMPARE(record.isRoot(), isRoot);
    QCOMPARE(record.isEvent(), isEvent);
    QCOMPARE(record.isPlatform(), isPlatform);
}

void DatabaseRecordTest::days_data() {
    QTest::addColumn<QVariant>("stored");
    QTest::addColumn<int>("expected");

    QTest::newRow("missing means every day") << QVariant() << 0x7F;
    QTest::newRow("every day") << QVariant(0x7F) << 0x7F;
    QTest::newRow("sunday only") << QVariant(0x01) << 0x01;
    QTest::newRow("weekdays") << QVariant(0x3E) << 0x3E;
    QTest::newRow("no days") << QVariant(0) << 0;
    QTest::newRow("bits above saturday are ignored") << QVariant(0xFF) << 0x7F;
    QTest::newRow("only high bits") << QVariant(0x80) << 0;
}

void DatabaseRecordTest::days() {
    QFETCH(QVariant, stored);
    QFETCH(int, expected);

    QVariantHash fields;
    if (stored.isValid()) fields.insert("effective_days", stored);
    QCOMPARE(makeRecord("a", fields).days(), expected);
}

void DatabaseRecordTest::startAndEnd() {
    const DatabaseRecord record = makeEvent("e", QTime(9, 0), QTime(17, 30));
    QCOMPARE(record.start(), at(9, 0));
    QCOMPARE(record.end(), at(17, 30));
}

// What a LINKS field looks like is decided by queryTables(): it appends every target to a QStringList. Text-like
// fields that hold a list are plain strings, separated by commas. list() has to cope with both.
void DatabaseRecordTest::list_data() {
    QTest::addColumn<QVariant>("stored");
    QTest::addColumn<QStringList>("expected");
    QTest::addColumn<bool>("knownBug");

    QTest::newRow("missing") << QVariant() << QStringList() << false;
    QTest::newRow("empty string") << QVariant(QString()) << QStringList() << false;
    QTest::newRow("single string") << QVariant(QString("a")) << QStringList{"a"} << false;
    QTest::newRow("comma separated") << QVariant(QString("a,b,c")) << QStringList{"a", "b", "c"} << false;
    QTest::newRow("empty parts are skipped") << QVariant(QString("a,,b,")) << QStringList{"a", "b"} << false;
    QTest::newRow("string list, empty") << QVariant(QStringList()) << QStringList() << false;
    QTest::newRow("string list, one target") << QVariant(QStringList{"a"}) << QStringList{"a"} << false;
    QTest::newRow("string list, two targets") << QVariant(QStringList{"a", "b"}) << QStringList{"a", "b"} << true;
    QTest::newRow("string list, three targets")
        << QVariant(QStringList{"a", "b", "c"}) << QStringList{"a", "b", "c"} << true;
}

void DatabaseRecordTest::list() {
    QFETCH(QVariant, stored);
    QFETCH(QStringList, expected);
    QFETCH(bool, knownBug);

    QVariantHash fields;
    if (stored.isValid()) fields.insert("key", stored);
    const DatabaseRecord record = makeRecord("a", fields);

    if (knownBug)
        QEXPECT_FAIL("", "QVariant::toString() on a QStringList with more than one entry returns an empty string",
                     Continue);
    QCOMPARE(record.list("key"), expected);
}

void DatabaseRecordTest::list_customSeparator() {
    const DatabaseRecord record = makeRecord("a", {{"key", "a|b|c"}});
    QCOMPARE(record.list("key", '|'), (QStringList{"a", "b", "c"}));
    // With the default separator the same string is one item.
    QCOMPARE(record.list("key"), (QStringList{"a|b|c"}));
}

void DatabaseRecordTest::resource_data() {
    QTest::addColumn<QVariant>("stored");
    QTest::addColumn<bool>("isResource");

    QTest::newRow("missing") << QVariant() << false;
    QTest::newRow("complete resource") << QVariant(makeResourceHash("a.png")) << true;
    QTest::newRow("incomplete resource") << QVariant(QVariantHash{{"filepath", "a.png"}}) << false;
    QTest::newRow("plain string") << QVariant(QString("a.png")) << false;
    QTest::newRow("number") << QVariant(12) << false;
    QTest::newRow("string list") << QVariant(QStringList{"a", "b"}) << false;
}

void DatabaseRecordTest::resource() {
    QFETCH(QVariant, stored);
    QFETCH(bool, isResource);

    QVariantHash fields;
    if (stored.isValid()) fields.insert("media", stored);
    const DatabaseRecord record = makeRecord("a", fields);

    QCOMPARE(record.isResource("media"), isResource);
    QCOMPARE(record.resource("media").isValid(), isResource);
}

// DatabaseRecord::filepath() is the shorthand for resource(key).filepath(). See DatabaseResourceTest for the
// explanation of the known bugs.
void DatabaseRecordTest::filepath_data() {
    QTest::addColumn<QString>("stored");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("knownBug");

    const QString localPath = QDir::temp().filePath("uploads/video.mp4");

    QTest::newRow("file url") << QUrl::fromLocalFile(localPath).toString() << localPath << false;
    QTest::newRow("empty") << QString() << QString() << false;
    QTest::newRow("plain path") << localPath << localPath << true;
    QTest::newRow("plain windows path") << QString("C:/Users/me/AppData/Local/project/uploads/video.mp4")
                                        << QString("C:/Users/me/AppData/Local/project/uploads/video.mp4") << true;
}

void DatabaseRecordTest::filepath() {
    QFETCH(QString, stored);
    QFETCH(QString, expected);
    QFETCH(bool, knownBug);

    const DatabaseRecord record = makeRecord("a", {{"media", makeResourceHash(stored)}});

    if (knownBug) QEXPECT_FAIL("", "filepath() only understands file: URLs, but Bridge stores plain paths", Continue);
    QCOMPARE(record.filepath("media"), expected);
}

QTEST_GUILESS_MAIN(DatabaseRecordTest)
#include "tst_databaserecord.moc"
