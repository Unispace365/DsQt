#include "testHelpers.h"

#include <QDir>
#include <QTest>
#include <QUrl>

using namespace dsqt::bridge;
using namespace testing;

// Tests for DatabaseResource: the typed view on a media field.
class DatabaseResourceTest : public QObject {
    Q_OBJECT

  private slots:
    void emptyResource_hasSafeDefaults();

    void isValid_requiresAllFields_data();
    void isValid_requiresAllFields();

    void widthAndHeight_data();
    void widthAndHeight();

    void crop_data();
    void crop();

    void filepath_data();
    void filepath();
    void filepath_missingKeyReturnsDefault();
};

void DatabaseResourceTest::emptyResource_hasSafeDefaults() {
    DatabaseResource resource;
    QVERIFY(!resource.isValid());
    QCOMPARE(resource.filepath(), QString());
    QCOMPARE(resource.type(), QString());
    QCOMPARE(resource.width(), 0.0);
    QCOMPARE(resource.height(), 0.0);
    QCOMPARE(resource.crop(), QRectF(0, 0, 1, 1));
}

void DatabaseResourceTest::isValid_requiresAllFields_data() {
    QTest::addColumn<QString>("missingKey");
    QTest::addColumn<bool>("valid");

    QTest::newRow("complete") << QString() << true;
    for (const char* key : {"filepath", "crop", "type", "width", "height"})
        QTest::newRow(qPrintable(QString("missing %1").arg(key))) << QString(key) << false;
}

void DatabaseResourceTest::isValid_requiresAllFields() {
    QFETCH(QString, missingKey);
    QFETCH(bool, valid);

    QVariantHash hash = makeResourceHash("a.png");
    hash.remove(missingKey);
    QCOMPARE(DatabaseResource(hash).isValid(), valid);
}

void DatabaseResourceTest::widthAndHeight_data() {
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<qreal>("expected");

    QTest::newRow("float") << QVariant(1920.0f) << 1920.0;
    QTest::newRow("double") << QVariant(12.5) << 12.5;
    QTest::newRow("int") << QVariant(1080) << 1080.0;
    QTest::newRow("numeric string") << QVariant(QString("640")) << 640.0;
    QTest::newRow("fractional string") << QVariant(QString("12.5")) << 12.5;
    QTest::newRow("non-numeric string") << QVariant(QString("abc")) << 0.0;
    QTest::newRow("invalid variant") << QVariant() << 0.0;
}

void DatabaseResourceTest::widthAndHeight() {
    QFETCH(QVariant, value);
    QFETCH(qreal, expected);

    QVariantHash hash = makeResourceHash("a.png");
    hash.insert("width", value);
    hash.insert("height", value);

    DatabaseResource resource(hash);
    QCOMPARE(resource.width(), expected);
    QCOMPARE(resource.height(), expected);
}

void DatabaseResourceTest::crop_data() {
    QTest::addColumn<QVariant>("crop");
    QTest::addColumn<QRectF>("expected");

    const QRectF full(0, 0, 1, 1);
    // Values below are exactly representable as float, as the conversion goes through float.
    QTest::newRow("valid") << QVariant(QVariantList{0.25, 0.5, 0.75, 0.125}) << QRectF(0.25, 0.5, 0.75, 0.125);
    QTest::newRow("integers") << QVariant(QVariantList{0, 0, 1, 1}) << full;
    QTest::newRow("too short") << QVariant(QVariantList{0.25, 0.5, 0.75}) << full;
    QTest::newRow("too long") << QVariant(QVariantList{0.25, 0.5, 0.75, 0.5, 0.5}) << full;
    QTest::newRow("non-numeric entry") << QVariant(QVariantList{0.25, QString("x"), 0.75, 0.5}) << full;
    QTest::newRow("empty list") << QVariant(QVariantList{}) << full;
    QTest::newRow("missing") << QVariant() << full;
}

void DatabaseResourceTest::crop() {
    QFETCH(QVariant, crop);
    QFETCH(QRectF, expected);

    QVariantHash hash = makeResourceHash("a.png");
    if (crop.isValid())
        hash.insert("crop", crop);
    else
        hash.remove("crop");
    QCOMPARE(DatabaseResource(hash).crop(), expected);
}

// DsBridgeSqlQuery stores the local path of the file in "filepath" (DsResource::toQml()). That is a plain path,
// for example "C:/Users/me/AppData/Local/project/uploads/file.mp4", not a URL. These rows document what
// DatabaseResource::filepath() should return for both forms. Rows marked as known bugs fail today, because
// filepath() runs the value through QUrl::toLocalFile(), which only accepts "file:" URLs.
void DatabaseResourceTest::filepath_data() {
    QTest::addColumn<QString>("stored");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("knownBug");

    const QString localPath = QDir::temp().filePath("uploads/video.mp4");

    QTest::newRow("file url") << QUrl::fromLocalFile(localPath).toString() << localPath << false;
    QTest::newRow("encoded file url")
        << QString::fromLatin1(QUrl::fromLocalFile(QDir::temp().filePath("a b.mp4")).toEncoded())
        << QDir::temp().filePath("a b.mp4") << false;
    QTest::newRow("empty") << QString() << QString() << false;
    QTest::newRow("plain path") << localPath << localPath << true;
    QTest::newRow("plain windows path") << QString("C:/Users/me/AppData/Local/project/uploads/video.mp4")
                                        << QString("C:/Users/me/AppData/Local/project/uploads/video.mp4") << true;
}

void DatabaseResourceTest::filepath() {
    QFETCH(QString, stored);
    QFETCH(QString, expected);
    QFETCH(bool, knownBug);

    QVariantHash hash = makeResourceHash(stored);
    if (knownBug) QEXPECT_FAIL("", "filepath() only understands file: URLs, but Bridge stores plain paths", Continue);
    QCOMPARE(DatabaseResource(hash).filepath(), expected);
}

void DatabaseResourceTest::filepath_missingKeyReturnsDefault() {
    // The documentation says: "If the resource is invalid or the filepath field is missing, returns the specified
    // default value." The default is run through toLocalFile() as well, so a plain default comes back empty.
    DatabaseResource resource;
    QEXPECT_FAIL("", "the default value is also passed through QUrl::toLocalFile()", Continue);
    QCOMPARE(resource.filepath("/some/default.png"), QString("/some/default.png"));
}

QTEST_GUILESS_MAIN(DatabaseResourceTest)
#include "tst_databaseresource.moc"
