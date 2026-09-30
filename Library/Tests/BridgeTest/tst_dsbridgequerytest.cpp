#include "bridge/dsBridgeQuery.h"
#include "bridge/dsQmlBridge.h"
#include "model/dsContentModel.h"

#include <QRegularExpression>
#include <QQmlComponent>
#include <QQmlContext>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTest>

#include <memory>

namespace dsqt::bridge {

class DSBridgeQueryTest : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void queryTables_loadsTagCatalog();
    void queryTables_loadsRecordTags();
    void queryTables_loadsTagFields();
    void queryTables_skipsDanglingTags();
    void queryTables_supportsOlderDatabases_data();
    void queryTables_supportsOlderDatabases();
    void queryTables_publishesAndRemovesTags();
    void qmlBridge_tagLookups();
    void qmlBridge_tagAssociationsUpdate();

  private:
    bool execute(const QString& sql);
    void publishContent();

    QSqlDatabase                      mDatabase;
    std::unique_ptr<DsBridgeSqlQuery> mQuery;
};

bool DSBridgeQueryTest::execute(const QString& sql) {
    QSqlQuery query(mDatabase);
    if (query.exec(sql)) return true;
    qWarning() << query.lastError().text() << sql;
    return false;
}

void DSBridgeQueryTest::publishContent() {
    QSignalSpy updated(&DsQmlBridge::instance(), &DsQmlBridge::bridgeUpdated);
    mQuery->mContent = mQuery->queryTables();
    mQuery->onProcessContent();
    QTRY_COMPARE(updated.count(), 1);
}

static QStringList modelUids(const QVariantList& models) {
    QStringList result;
    for (const auto& value : models) {
        auto* record = dynamic_cast<model::ContentModel*>(value.value<QObject*>());
        result.append(record ? record->value("uid").toString() : QStringLiteral("<invalid model>"));
    }
    return result;
}

void DSBridgeQueryTest::init() {
    mDatabase = QSqlDatabase::addDatabase("QSQLITE", "bridge-query-test");
    mDatabase.setDatabaseName(":memory:");
    QVERIFY2(mDatabase.open(), qPrintable(mDatabase.lastError().text()));

    // Include the columns consumed by queryTables(), without requiring BridgeSync.
    QVERIFY(execute("CREATE TABLE lookup (uid TEXT, type TEXT, parent_uid TEXT, name TEXT, app_key TEXT, "
                    "reverse_ordering INTEGER)"));
    QVERIFY(execute("CREATE TABLE record (uid TEXT, type_uid TEXT, parent_uid TEXT, parent_slot TEXT, variant TEXT, "
                    "name TEXT, span_type TEXT, span_start_date TEXT, span_end_date TEXT, start_time TEXT, "
                    "end_time TEXT, effective_days INTEGER, complete INTEGER, visible INTEGER, rank INTEGER)"));
    QVERIFY(execute("CREATE TABLE trait_map (type_uid TEXT, trait_uid TEXT)"));
    QVERIFY(execute("CREATE TABLE defaults (field_uid TEXT, field_type TEXT, checked INTEGER, color TEXT, "
                    "text_value TEXT, rich_text TEXT, number REAL, number_min REAL, number_max REAL, "
                    "option_type TEXT, option_value TEXT)"));
    QVERIFY(execute("CREATE TABLE resource (hash TEXT, type TEXT, uri TEXT, width INTEGER, height INTEGER, "
                    "duration REAL, pages INTEGER, file_size INTEGER, filename TEXT)"));
    QVERIFY(execute("CREATE TABLE value (uid TEXT, field_uid TEXT, record_uid TEXT, field_type TEXT, "
                    "is_default INTEGER, is_empty INTEGER, checked INTEGER, color TEXT, text_value TEXT, "
                    "rich_text TEXT, number REAL, number_min REAL, number_max REAL, datetime TEXT, "
                    "resource_hash TEXT, crop_x REAL, crop_y REAL, crop_w REAL, crop_h REAL, "
                    "composite_frame TEXT, composite_x REAL, composite_y REAL, composite_w REAL, composite_h REAL, "
                    "option_type TEXT, option_value TEXT, link_url TEXT, link_target_uid TEXT, "
                    "preview_resource_hash TEXT, hotspot_x REAL, hotspot_y REAL, hotspot_w REAL, hotspot_h REAL, "
                    "tags TEXT DEFAULT '_')"));
    QVERIFY(execute("CREATE TABLE tags (uid TEXT PRIMARY KEY, tag_class_uid TEXT, label TEXT)"));
    QVERIFY(execute("CREATE TABLE record_tags (record_uid TEXT, tag_uid TEXT)"));

    QVERIFY(execute("INSERT INTO lookup (uid, type, name, app_key) VALUES "
                    "('type1', 'type', 'Article', 'article'), "
                    "('class1', 'tag_class', 'Topics', 'topic-class'), "
                    "('class2', 'tag_class', 'Audience', 'audience'), "
                    "('field1', 'field', 'Keywords', 'key-words'), "
                    "('field2', 'field', 'Empty Tags', 'empty_tags'), "
                    "('field3', 'field', 'Title', 'title')"));
    QVERIFY(execute("INSERT INTO record (uid, type_uid, variant, name, complete, visible, rank) VALUES "
                    "('record1', 'type1', 'ROOT_CONTENT', 'Article One', 1, 1, 1), "
                    "('hidden1', 'type1', 'ROOT_CONTENT', 'Hidden Article', 1, 0, 2)"));
    QVERIFY(execute("INSERT INTO tags (uid, tag_class_uid, label) VALUES "
                    "('tag1', 'class1', 'Art, Design'), "
                    "('tag2', 'class1', 'Science'), "
                    "('tag3', 'class2', 'Adults')"));
    QVERIFY(execute("INSERT INTO record_tags (record_uid, tag_uid) VALUES "
                    "('record1', 'tag2'), ('record1', 'tag1'), ('record1', 'tag3')"));
    QVERIFY(execute("INSERT INTO value (uid, field_uid, record_uid, field_type, tags) VALUES "
                    "('value1', 'field1', 'record1', 'TAGS', 'tag1'), "
                    "('value1', 'field1', 'record1', 'TAGS', 'tag2'), "
                    "('value2', 'field2', 'record1', 'TAGS', '_')"));
    QVERIFY(execute("INSERT INTO value (uid, field_uid, record_uid, field_type, text_value) VALUES "
                    "('value3', 'field3', 'record1', 'TEXT', 'An article')"));

    // Drive the query directly, without initializing an application engine.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("QObject::connect.*invalid nullptr parameter"));
    mQuery            = std::make_unique<DsBridgeSqlQuery>();
    mQuery->mDatabase = mDatabase;
}

void DSBridgeQueryTest::cleanup() {
    mQuery.reset();
    mDatabase.close();
    mDatabase = QSqlDatabase();
    QSqlDatabase::removeDatabase("bridge-query-test");

    const auto uids = model::ContentLookup::get().keys();
    for (const auto& uid : uids) {
        if (uid.length() <= 12) model::ContentModel::deleteNow(uid);
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void DSBridgeQueryTest::queryTables_loadsTagCatalog() {
    const auto content = mQuery->queryTables();
    QCOMPARE(content.tagUids(), QStringList({"tag1", "tag2", "tag3"}));
    QCOMPARE(content.tags().size(), 3);
    QCOMPARE(content.contentUids(), QStringList({"record1"}));

    const auto tag = content.find("tag1");
    QCOMPARE(tag.uid(), "tag1");
    QCOMPARE(tag.recordName(), "Art, Design");
    QCOMPARE(tag.value("tag_uid").toString(), "tag1");
    QCOMPARE(tag.value("tag_class_uid").toString(), "class1");
    QCOMPARE(tag.value("tag_class_app_key").toString(), "topic-class");
    QCOMPARE(tag.value("label").toString(), "Art, Design");
    QVERIFY(!tag.isRoot());
    QVERIFY(!tag.isEvent());
    QVERIFY(!tag.isPlatform());
}

void DSBridgeQueryTest::queryTables_loadsRecordTags() {
    // Repeated assignments should not duplicate labels or uids.
    QVERIFY(execute("INSERT INTO record_tags VALUES ('record1', 'tag1')"));
    const auto content = mQuery->queryTables();
    const auto record  = content.find("record1");
    QCOMPARE(record.value("topic_class_tag_uids").toStringList(), QStringList({"tag1", "tag2"}));
    QCOMPARE(record.value("topic_class_tags").toStringList(), QStringList({"Art, Design", "Science"}));
    QCOMPARE(record.value("audience_tag_uids").toStringList(), QStringList({"tag3"}));
    QCOMPARE(record.value("audience_tags").toStringList(), QStringList({"Adults"}));
    QCOMPARE(record.value("title").toString(), "An article");
}

void DSBridgeQueryTest::queryTables_loadsTagFields() {
    QVERIFY(execute("INSERT INTO value (uid, field_uid, record_uid, field_type, tags) VALUES "
                    "('value4', 'field1', 'record1', 'TAGS', 'tag1')"));
    const auto content = mQuery->queryTables();
    const auto record  = content.find("record1");
    QCOMPARE(record.value("key_words").toStringList(), QStringList({"tag1", "tag2"}));
    QCOMPARE(record.value("key_words_labels").toStringList(), QStringList({"Art, Design", "Science"}));
    QCOMPARE(record.value("key_words_field_uid").toString(), "field1");
    QVERIFY(record.contains("empty_tags"));
    QVERIFY(record.value("empty_tags").toStringList().isEmpty());
    QVERIFY(record.value("empty_tags_labels").toStringList().isEmpty());
    QCOMPARE(record.value("empty_tags_field_uid").toString(), "field2");
}

void DSBridgeQueryTest::queryTables_skipsDanglingTags() {
    QVERIFY(execute("INSERT INTO tags VALUES ('orphan1', 'missing', 'Orphan')"));
    QVERIFY(execute("INSERT INTO record_tags VALUES "
                    "('record1', 'missing'), ('record1', 'orphan1'), ('missing', 'tag1'), ('hidden1', 'tag1')"));
    QVERIFY(execute("INSERT INTO value (uid, field_uid, record_uid, field_type, tags) VALUES "
                    "('value4', 'field1', 'record1', 'TAGS', 'missing')"));
    const auto content = mQuery->queryTables();
    const auto record  = content.find("record1");
    QVERIFY(content.find("missing").isEmpty());
    QVERIFY(content.find("hidden1").isEmpty());
    QVERIFY(!record.contains("_tags"));
    QCOMPARE(record.value("topic_class_tag_uids").toStringList(), QStringList({"tag1", "tag2"}));
    QCOMPARE(record.value("key_words").toStringList(), QStringList({"tag1", "tag2"}));
    QCOMPARE(record.value("key_words_labels").toStringList(), QStringList({"Art, Design", "Science"}));
    // A missing class does not discard the tag's own metadata.
    QCOMPARE(content.find("orphan1").value("label").toString(), "Orphan");
}

void DSBridgeQueryTest::queryTables_supportsOlderDatabases_data() {
    QTest::addColumn<bool>("tagTable");
    QTest::addColumn<bool>("recordTagsTable");
    QTest::addColumn<bool>("tagColumn");

    QTest::newRow("no-tag-schema") << false << false << false;
    QTest::newRow("no-tag-catalog") << false << true << true;
    QTest::newRow("no-record-tags") << true << false << true;
    QTest::newRow("no-value-tags") << true << true << false;
}

void DSBridgeQueryTest::queryTables_supportsOlderDatabases() {
    QFETCH(bool, tagTable);
    QFETCH(bool, recordTagsTable);
    QFETCH(bool, tagColumn);

    if (!tagTable) QVERIFY(execute("DROP TABLE tags"));
    if (!recordTagsTable) QVERIFY(execute("DROP TABLE record_tags"));
    if (!tagColumn) QVERIFY(execute("ALTER TABLE value DROP COLUMN tags"));

    const auto content = mQuery->queryTables();
    const auto record  = content.find("record1");
    QCOMPARE(record.value("title").toString(), "An article");
    QCOMPARE(content.tagUids().size(), tagTable ? 3 : 0);
    QCOMPARE(record.value("topic_class_tag_uids").toStringList().size(), tagTable && recordTagsTable ? 2 : 0);
    QCOMPARE(record.value("key_words").toStringList().size(), tagTable && tagColumn ? 2 : 0);
}

void DSBridgeQueryTest::queryTables_publishesAndRemovesTags() {
    auto&      bridge = DsQmlBridge::instance();
    QSignalSpy updated(&bridge, &DsQmlBridge::bridgeUpdated);

    mQuery->mContent = mQuery->queryTables();
    auto record      = model::ContentModel::createOrUpdate(mQuery->mContent.find("record1"));
    mQuery->onProcessContent();
    QTRY_COMPARE(updated.count(), 1);

    auto tags = bridge.content()->getChildByName("Tags");
    QVERIFY(tags);
    QCOMPARE(tags->getChildren().size(), 3);
    QCOMPARE(bridge.content()->value("tag_uid").toStringList(), QStringList({"tag1", "tag2", "tag3"}));
    QVERIFY(bridge.getRecordById("tag1"));
    QCOMPARE(bridge.getRecordById("tag1")->parent(), tags);
    QCOMPARE(record->value("key_words").toStringList(), QStringList({"tag1", "tag2"}));

    QVERIFY(execute("UPDATE tags SET label = 'Updated' WHERE uid = 'tag2'"));
    QVERIFY(execute("DELETE FROM tags WHERE uid = 'tag1'"));
    QVERIFY(execute("DELETE FROM record_tags WHERE tag_uid = 'tag1'"));
    QVERIFY(execute("DELETE FROM value WHERE tags = 'tag1'"));
    mQuery->mContent = mQuery->queryTables();
    model::ContentModel::createOrUpdate(mQuery->mContent.find("record1"));
    mQuery->onProcessContent();
    QTRY_COMPARE(updated.count(), 2);

    QVERIFY(!bridge.getRecordById("tag1"));
    QCOMPARE(tags->getChildren().size(), 2);
    QCOMPARE(bridge.database().tagUids(), QStringList({"tag2", "tag3"}));
    QCOMPARE(record->value("key_words").toStringList(), QStringList({"tag2"}));
    QCOMPARE(record->value("key_words_labels").toStringList(), QStringList({"Updated"}));
    QCOMPARE(record->value("topic_class_tags").toStringList(), QStringList({"Updated"}));

    QVERIFY(execute("DELETE FROM tags"));
    QVERIFY(execute("DELETE FROM record_tags"));
    QVERIFY(execute("DELETE FROM value WHERE field_type = 'TAGS'"));
    mQuery->mContent = mQuery->queryTables();
    model::ContentModel::createOrUpdate(mQuery->mContent.find("record1"));
    mQuery->onProcessContent();
    QTRY_COMPARE(updated.count(), 3);

    QVERIFY(tags->getChildren().isEmpty());
    QVERIFY(bridge.content()->value("tag_uid").toStringList().isEmpty());
    QVERIFY(bridge.database().tags().isEmpty());
    QVERIFY(!record->value("key_words").isValid());
    QVERIFY(!record->value("topic_class_tags").isValid());
}

void DSBridgeQueryTest::qmlBridge_tagLookups() {
    // Deliberately put record2 before record1 to verify record order rather than hash/UID order.
    QVERIFY(execute("INSERT INTO record (uid, type_uid, variant, name, complete, visible, rank) VALUES "
                    "('record2', 'type1', 'ROOT_CONTENT', 'Article Two', 1, 1, 0), "
                    "('fieldonly', 'type1', 'ROOT_CONTENT', 'Field Only', 1, 1, 3), "
                    "('plain', 'type1', 'ROOT_CONTENT', 'Untagged', 1, 1, 4)"));
    QVERIFY(execute("INSERT INTO lookup (uid, type, name, app_key) VALUES "
                    "('class3', 'tag_class', 'No App Key', '')"));
    QVERIFY(execute("INSERT INTO tags VALUES ('tag4', 'class3', 'Guid Only')"));
    QVERIFY(execute("INSERT INTO record_tags VALUES "
                    "('record2', 'tag1'), ('record2', 'tag2'), ('record2', 'tag4'), "
                    "('record1', 'tag1'), ('hidden1', 'tag1'), ('record2', 'missing')"));
    QVERIFY(execute("INSERT INTO value (uid, field_uid, record_uid, field_type, tags) VALUES "
                    "('value4', 'field1', 'fieldonly', 'TAGS', 'tag2'), "
                    "('value5', 'field1', 'fieldonly', 'TAGS', 'tag2'), "
                    "('value6', 'field1', 'fieldonly', 'TAGS', 'missing'), "
                    "('value7', 'field2', 'fieldonly', 'TAGS', '_')"));
    // An ordinary text field containing a tag UID must not create an association.
    QVERIFY(execute("INSERT INTO value (uid, field_uid, record_uid, field_type, text_value) VALUES "
                    "('value8', 'field3', 'plain', 'TEXT', 'tag1')"));
    publishContent();

    auto& bridge = DsQmlBridge::instance();
    QCOMPARE(modelUids(bridge.getTagsForClass("class1")), QStringList({"tag1", "tag2"}));
    QCOMPARE(modelUids(bridge.getTagsForClass("topic-class")), QStringList({"tag1", "tag2"}));
    QCOMPARE(modelUids(bridge.getTagsForClass("class3")), QStringList({"tag4"}));
    QCOMPARE(modelUids(bridge.getRecordsWithTag("tag1")), QStringList({"record2", "record1"}));
    QCOMPARE(modelUids(bridge.getRecordsWithTag("tag2")), QStringList({"record2", "record1", "fieldonly"}));
    QCOMPARE(modelUids(bridge.getRecordsWithTagClass("class1")),
             QStringList({"record2", "record1", "fieldonly"}));
    QCOMPARE(modelUids(bridge.getRecordsWithTagClass("topic-class")),
             QStringList({"record2", "record1", "fieldonly"}));
    QCOMPARE(modelUids(bridge.getRecordsWithTagClass("class3")), QStringList({"record2"}));
    QCOMPARE(modelUids(bridge.getTagsForRecord("record1")), QStringList({"tag1", "tag2", "tag3"}));
    QCOMPARE(modelUids(bridge.getTagsForRecord("record2")), QStringList({"tag1", "tag2", "tag4"}));
    QCOMPARE(modelUids(bridge.getTagsForRecord("fieldonly")), QStringList({"tag2"}));
    QVERIFY(bridge.getTagsForRecord("plain").isEmpty());
    QVERIFY(bridge.getTagsForRecord("hidden1").isEmpty());
    QVERIFY(bridge.getTagsForClass("topic_class").isEmpty()); // Use the original CMS app_key.
    for (const auto& unknown : QStringList({"", "missing"})) {
        QVERIFY(bridge.getTagsForClass(unknown).isEmpty());
        QVERIFY(bridge.getRecordsWithTag(unknown).isEmpty());
        QVERIFY(bridge.getRecordsWithTagClass(unknown).isEmpty());
        QVERIFY(bridge.getTagsForRecord(unknown).isEmpty());
    }

    // Exercise real QML invocation and property-map access, not just the C++ return values.
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("testBridge", &bridge);
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQml
        QtObject {
            property var tags: testBridge.getTagsForClass("topic-class")
            property string tagLabel: tags[0].label
            property string tagIds: tags.map(function(tag) { return tag.uid }).join(",")
            property string recordIds: testBridge.getRecordsWithTag("tag2")
                .map(function(record) { return record.uid }).join(",")
            property string classRecordIds: testBridge.getRecordsWithTagClass("class1")
                .map(function(record) { return record.uid }).join(",")
            property string recordTagIds: testBridge.getTagsForRecord("record2")
                .map(function(tag) { return tag.uid }).join(",")
        }
    )", QUrl());
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    QCOMPARE(object->property("tagLabel").toString(), "Art, Design");
    QCOMPARE(object->property("tagIds").toString(), "tag1,tag2");
    QCOMPARE(object->property("recordIds").toString(), "record2,record1,fieldonly");
    QCOMPARE(object->property("classRecordIds").toString(), "record2,record1,fieldonly");
    QCOMPARE(object->property("recordTagIds").toString(), "tag1,tag2,tag4");
}

void DSBridgeQueryTest::qmlBridge_tagAssociationsUpdate() {
    auto& bridge = DsQmlBridge::instance();
    publishContent();

    // Removing the direct assignment preserves an association still selected in a TAGS field.
    QVERIFY(execute("DELETE FROM record_tags WHERE tag_uid IN ('tag1', 'tag3')"));
    publishContent();
    QCOMPARE(modelUids(bridge.getRecordsWithTag("tag1")), QStringList({"record1"}));
    QVERIFY(bridge.getRecordsWithTagClass("audience").isEmpty());
    QCOMPARE(modelUids(bridge.getTagsForRecord("record1")), QStringList({"tag1", "tag2"}));

    QVERIFY(execute("DELETE FROM value WHERE tags = 'tag1'"));
    QVERIFY(execute("UPDATE lookup SET app_key = 'subjects' WHERE uid = 'class1'"));
    // A class UID wins over a colliding app key on a different class.
    QVERIFY(execute("UPDATE lookup SET app_key = 'class1' WHERE uid = 'class2'"));
    QVERIFY(execute("UPDATE tags SET label = 'Updated' WHERE uid = 'tag2'"));
    publishContent();
    QVERIFY(bridge.getRecordsWithTag("tag1").isEmpty());
    QVERIFY(bridge.getTagsForClass("topic-class").isEmpty());
    QCOMPARE(modelUids(bridge.getTagsForClass("subjects")), QStringList({"tag1", "tag2"}));
    QCOMPARE(modelUids(bridge.getTagsForClass("class1")), QStringList({"tag1", "tag2"}));
    QCOMPARE(modelUids(bridge.getTagsForClass("class2")), QStringList({"tag3"}));
    QCOMPARE(modelUids(bridge.getRecordsWithTagClass("subjects")), QStringList({"record1"}));
    QCOMPARE(modelUids(bridge.getTagsForRecord("record1")), QStringList({"tag2"}));
    QCOMPARE(bridge.getTagsForRecord("record1").first().value<QObject*>()->property("label").toString(),
             "Updated");

    // Removing a tag clears its associations even if the source rows still reference it.
    QVERIFY(execute("DELETE FROM tags WHERE uid = 'tag2'"));
    publishContent();
    QVERIFY(bridge.getRecordsWithTag("tag2").isEmpty());
    QVERIFY(bridge.getRecordsWithTagClass("subjects").isEmpty());
    QVERIFY(bridge.getTagsForRecord("record1").isEmpty());
}

} // namespace dsqt::bridge

QTEST_GUILESS_MAIN(dsqt::bridge::DSBridgeQueryTest)

#include "tst_dsbridgequerytest.moc"
