#include <QtTest>
#include <settings/dsSettings.h>
#include <settings/dsSettingsFile.h>
#include <memory>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

// ---------------------------------------------------------------------------
// Migrated from the removed dsqt::DsSettings / dsqt::DsSettingsRef API to the
// dsqt::Settings / dsqt::SettingsFile API introduced by the settings revamp.
//
// Behavioural differences that shaped this file:
//
//  * get<T>() / find<T>() return T (not std::optional<T>). "Absent" is now
//    expressed as "equal to the supplied default", so the old has_value()
//    assertions became value or default comparisons.
//  * Conversions are performed by QVariant, not by bespoke DsSettings code.
//    Only conversions QMetaType knows about succeed — notably date/time strings
//    must be ISO-8601, and a QVariantList never converts to a scalar.
//  * getWithMeta() is gone. Its nearest equivalent is provenance(), which
//    reports the source file (or "default" / "override") for a key.
//  * setDateFormat() / setCustomDateFormat() are gone; there are no
//    format hooks, so the custom / TextDate / RFC-2822 rows were dropped.
//  * getSettingsOrCreate() / forgetSettings() are gone; the registry is now
//    Settings::add() / Settings::forget() / Settings::settingsFile().
// ---------------------------------------------------------------------------

class DsSettingsTest : public QObject
{
    Q_OBJECT

  public:
    DsSettingsTest();
    ~DsSettingsTest();

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Registry (replaces getSettingsOrCreate / forgetSettings)
    void registry_addForgetAndFind();
    void registry_findShouldReturnTheDefaultForAnUnknownFile();

    void find_shouldReturnTheValueWhenTheKeyExists();
    void find_shouldReturnTheDefaultWhenTheKeyDoesNotExist();
    void getOr_shouldReturnTheSettingValueWhentheSettingExist();
    void getOr_shoudReturnTheOrValueWhenTheSettingDoesNotExist();

    //Base types (bool,int,int64,float,double,QString)
    void get_bool_shouldReturnExpectedBoolean_data();
    void get_bool_shouldReturnExpectedBoolean();
    void get_integral_shouldReturnExpectedIntegral_data();
    void get_integral_shouldReturnExpectedIntegral();
    void get_floatingPoint_shouldReturnExpectedFloatingPoint_data();
    void get_floatingPoint_shouldReturnExpectedFloatingPoint();
    void get_string_shouldReturnExpectedString_data();
    void get_string_shouldReturnExpectedString();
    void get_string_fromArrayOrTable_shouldNotConvert();

    //QColor
    void get_QColor_from_validString_shouldReturnAValidQColor_data();
    void get_QColor_from_validString_shouldReturnAValidQColor();
    void get_QColor_from_Arrays_shouldReturnAValidQColor_data();
    void get_QColor_from_Arrays_shouldReturnAValidQColor();
    void get_QColor_from_Tables_shouldReturnAValidQColor_data();
    void get_QColor_from_Tables_shouldReturnAValidQColor();

    //Date & time
    void get_QTime_from_iso8601String_shouldReturnAValidQTime();
    void get_QDate_from_iso8601String_shouldReturnAValidQDate();
    void get_QDateTime_from_iso8601String_shouldReturnAValidQDateTime();
    void get_dateTime_fromNonIsoStrings_shouldNotConvert();
    void get_QTime_from_tomlDateTime_shouldReturnAValidQTime_data();
    void get_QTime_from_tomlDateTime_shouldReturnAValidQTime();
    void get_QDate_from_tomlDateTime_shouldReturnAValidQDate_data();
    void get_QDate_from_tomlDateTime_shouldReturnAValidQDate();
    void get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTime_data();
    void get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTime();
    void get_dateTime_acrossTomlTypes_shouldOnlyConvertWhereQVariantCan();

    //GEOM
    void get_vectors_fromTomlArrays_shouldStayLists();
    void get_QPoint_from_tables_shouldReturnAValidQPointF();
    void get_QSize_from_tables_shouldReturnAValidQSizeF();
    void get_QRect_from_tables_shouldReturnAValidQRectF_data();
    void get_QRect_from_tables_shouldReturnAValidQRectF();
    void get_geom_fromUnknownKeySignatures_shouldStayMaps();

    //containers
    void get_QVariantList_shouldReturnAValidQVariantList_data();
    void get_QVariantList_shouldReturnAValidQVariantList();
    void get_QVariantList_intsOnly();
    void get_QVariantList_stringsOnly();
    void get_QVariantList_nested();
    void get_QVariantList_empty();
    void get_QVariantList_allTypes();
    void get_QVariantList_nonexistentKey();
    void get_QVariantList_legacyMetaRequiresATypeKey();

    void get_QVariantMap_shouldReturnAValidQVariantMap_data();
    void get_QVariantMap_shouldReturnAValidQVariantMap();
    void get_QVariantMap_stringsOnly();
    void get_QVariantMap_nested();
    void get_QVariantMap_empty();
    void get_QVariantMap_allTypes();
    void get_QVariantMap_nonexistentKey();
    void get_QVariantMap_arrayOfTablesStaysAList();

    void get_shouldReturnTheDefaultWhenTheSettingDoesNotExist();
    void provenance_shouldReportTheSourceOfAKey();
    void read_notable();
    void settingsFile_shouldStripLegacyMetadata();
    void settingsFile_shouldUnwrapLegacyArrays();
    void settingsFile_shouldInterpretLegacyMetadataColors();
    void settingsFile_shouldNotCollapseArrayOfTablesWithATypeKey();
    void settingsFile_shouldLoadExtraFilesBetweenBaseFilesAndOverrides();
    void settingsFile_shouldPruneOverridesThatMatchReloadedFiles();
    void settingsFile_shouldKeepOverridesSavedOutsideSearchPaths();

  private:
    // Directory the test data is copied into by the build (see CMakeLists.txt).
    static QString settingsDir() { return QDir::current().filePath("settings"); }

    std::unique_ptr<dsqt::SettingsFile> test_settings;

    // Search paths the Settings singleton had on entry to the current test,
    // restored by cleanup(). See the note there.
    QStringList saved_search_paths;
};

DsSettingsTest::DsSettingsTest()
{

}

DsSettingsTest::~DsSettingsTest()
{

}

void DsSettingsTest::initTestCase()
{
}

void DsSettingsTest::cleanupTestCase()
{

}

void DsSettingsTest::init()
{
    QLoggingCategory::setFilterRules("settings.parser*=false\n");

    // A standalone SettingsFile (no manager) keeps each test isolated from the
    // Settings singleton; the registry itself is covered by registry_* below.
    test_settings = std::make_unique<dsqt::SettingsFile>(nullptr, QStringList{settingsDir()});
    test_settings->setFileName("test_settings.toml");

    saved_search_paths = dsqt::Settings::instance().searchPaths();

    QVERIFY2(!test_settings->resolvedFilePaths().isEmpty(),
             qPrintable("test_settings.toml not found under " + settingsDir()));
}

void DsSettingsTest::cleanup()
{
    test_settings.reset();

    // The Settings singleton is process-wide, so registry mutations are undone
    // here rather than at the end of the test that made them: cleanup() still
    // runs when a test aborts early on a failed QVERIFY, whereas trailing
    // teardown inside the test body does not.
    //
    // Forgetting every registered name is correct while nothing is registered
    // in initTestCase(). If that changes, snapshot the names in init() and
    // forget only the ones added since.
    auto& settings = dsqt::Settings::instance();
    const auto names = settings.settingsNames();
    for (const QString& name : names)
        dsqt::Settings::forget(name);
    settings.setSearchPaths(saved_search_paths);
}

//*****************
//Registry
//*****************
void DsSettingsTest::registry_addForgetAndFind()
{
    auto& settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});

    QVERIFY(!settings.hasSettingsFile("test_settings"));

    dsqt::Settings::add("test_settings"); // loads test_settings.toml from the search paths

    QVERIFY(settings.hasSettingsFile("test_settings"));
    QVERIFY(settings.settingsNames().contains("test_settings"));

    auto* sf = settings.settingsFile("test_settings");
    QVERIFY(sf != nullptr);
    QCOMPARE(sf->find<QString>("no_table"), QStringLiteral("test value"));

    // The static find<T>() goes through the registry by name.
    QCOMPARE(dsqt::Settings::find<QString>("test_settings", "no_table"),
             QStringLiteral("test value"));

    dsqt::Settings::forget("test_settings");

    QVERIFY(!settings.hasSettingsFile("test_settings"));
    QVERIFY(!settings.settingsNames().contains("test_settings"));
}

void DsSettingsTest::registry_findShouldReturnTheDefaultForAnUnknownFile()
{
    QCOMPARE(dsqt::Settings::find<QString>("no_such_settings", "no_table",
                                           QStringLiteral("fallback")),
             QStringLiteral("fallback"));
}

//*****************
//Basic lookup
//*****************
void DsSettingsTest::find_shouldReturnTheValueWhenTheKeyExists()
{
    QCOMPARE(test_settings->get<QString>("no_table"), QStringLiteral("test value"));
}

void DsSettingsTest::find_shouldReturnTheDefaultWhenTheKeyDoesNotExist()
{
    // No optional any more — an absent key yields the (default-constructed) fallback.
    QVERIFY(!test_settings->contains("not_exist"));
    QVERIFY(test_settings->get<QString>("not_exist").isEmpty());
}

void DsSettingsTest::getOr_shouldReturnTheSettingValueWhentheSettingExist()
{
    const auto value = test_settings->getOr<QString>("no_table", QStringLiteral("fake"));
    QVERIFY2(value != QStringLiteral("fake"), "Or value was returned and it should not have been");
    QCOMPARE(value, QStringLiteral("test value"));
}

void DsSettingsTest::getOr_shoudReturnTheOrValueWhenTheSettingDoesNotExist()
{
    QCOMPARE(test_settings->getOr<QString>("not_exist", QStringLiteral("fake")),
             QStringLiteral("fake"));
}

//*****************
//Base Types
//*****************
//int
void DsSettingsTest::get_integral_shouldReturnExpectedIntegral_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<qint64>("result");

    QTest::newRow("int positive")      << "int_positive"     << static_cast<qint64>( 1024);
    QTest::newRow("int negative")      << "int_negative"     << static_cast<qint64>(-1024);
    QTest::newRow("int from string")   << "int_from_string"  << static_cast<qint64>( 1024);
    QTest::newRow("int from float")    << "int_from_float"   << static_cast<qint64>( 1024);
}

void DsSettingsTest::get_integral_shouldReturnExpectedIntegral(){
    QFETCH(QString, key);
    QFETCH(qint64, result);

    const auto int32_value = test_settings->get<int>("test.int." + key);
    const auto int64_value = test_settings->get<qint64>("test.int." + key);

    QCOMPARE(int32_value, static_cast<int>(result));
    QCOMPARE(int64_value, result);
}

//float
void DsSettingsTest::get_floatingPoint_shouldReturnExpectedFloatingPoint_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<double>("result");

    QTest::newRow("float positive")        << "float_positive"        << 1024.203;
    QTest::newRow("float notation")        << "float_notation"        << 3.43e+10;
    QTest::newRow("float big")             << "float_big"             << 3.43e+64;
    QTest::newRow("float from string")     << "float_from_string"     << 1024.578;
    // QVariant reports QString as convertible to double, but the conversion of a
    // non-numeric string fails and yields a value-initialised 0.0 (the old
    // DsSettings parser produced NaN here).
    QTest::newRow("float from bad string") << "float_from_bad_string" << 0.0;
    QTest::newRow("float from int")        << "float_from_int"        << 1024.0;
}

void DsSettingsTest::get_floatingPoint_shouldReturnExpectedFloatingPoint(){
    QFETCH(QString, key);
    QFETCH(double, result);

    const auto float_value  = test_settings->get<float>("test.floating_point." + key);
    const auto double_value = test_settings->get<double>("test.floating_point." + key);

    QCOMPARE(float_value, static_cast<float>(result));
    QCOMPARE(double_value, result);
}

//string
void DsSettingsTest::get_string_shouldReturnExpectedString_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("result");

    // QVariant's number-to-string conversion uses the shortest round-trip form,
    // so 1024.203 renders as "1024.203" rather than the old "1024.203000".
    QTest::newRow("string from float")<<"string_from_float"<<"1024.203";
    QTest::newRow("string from int")<<"string_from_int"<<"1024";
    QTest::newRow("string from bool")<<"string_from_bool"<<"true";
    // QVariant renders QTime/QDateTime with Qt::ISODateWithMs, so the
    // millisecond field is always present.
    QTest::newRow("string from date")<<"string_from_date"<<"1979-05-27";
    QTest::newRow("string from time")<<"string_from_time"<<"07:32:00.000";
    QTest::newRow("string from datetime")<<"string_from_datetime"<<"1979-05-27T07:32:00.000Z";
}

void DsSettingsTest::get_string_shouldReturnExpectedString(){
    QFETCH(QString, key);
    QFETCH(QString, result);

    QCOMPARE(test_settings->get<QString>("test.strings." + key), result);
}

void DsSettingsTest::get_string_fromArrayOrTable_shouldNotConvert()
{
    // The old parser stringified arrays and tables. QVariant has no such
    // conversion, so these keys keep their container types.
    const QVariantList list = test_settings->get<QVariantList>("test.strings.string_from_array");
    QCOMPARE(list.size(), 4);
    QCOMPARE(list[0].toString(), QStringLiteral("this"));
    QCOMPARE(list[3].toString(), QStringLiteral("array"));
    QVERIFY(test_settings->get<QString>("test.strings.string_from_array").isEmpty());

    const QVariantMap map = test_settings->get<QVariantMap>("test.strings.string_from_table");
    QCOMPARE(map.size(), 3);
    QCOMPARE(map.value("one").toString(), QStringLiteral("1"));
    QVERIFY(test_settings->get<QString>("test.strings.string_from_table").isEmpty());
}

//bool
void DsSettingsTest::get_bool_shouldReturnExpectedBoolean_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<bool>("result");

    QTest::newRow("bool type")<<"bool_true"<<true;
    QTest::newRow("string true is true")<<"bool_str_true"<<true;
    QTest::newRow("string false is false")<<"bool_str_false"<<false;
    QTest::newRow("string empty is false")<<"bool_str_empty"<<false;
    QTest::newRow("string foobar is true")<<"bool_str_foobar"<<true;
}

void DsSettingsTest::get_bool_shouldReturnExpectedBoolean(){
    QFETCH(QString, key);
    QFETCH(bool, result);

    QCOMPARE(test_settings->get<bool>("test.bool." + key), result);
}


//*****************
//QColor
//*****************
void DsSettingsTest::get_QColor_from_validString_shouldReturnAValidQColor_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QColor>("result");

    QTest::newRow("hex with alpha")<<"hex"<<QColor(255,238,170,128);
    QTest::newRow("named color")<<"name"<<QColor::fromString("blue");
}

void DsSettingsTest::get_QColor_from_validString_shouldReturnAValidQColor(){
    QFETCH(QString, key);
    QFETCH(QColor, result);

    QCOMPARE(test_settings->get<QColor>("test.color.strings." + key), result);
}

void DsSettingsTest::get_QColor_from_Arrays_shouldReturnAValidQColor_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QColor>("result");

    QTest::newRow("float gray")<<"float_gray"<<QColor::fromRgbF(0.5,0.5,0.5,1.0);
    QTest::newRow("float rgb")<<"float_rgb"<<QColor::fromRgbF(0.5,0,0,1.0);
    QTest::newRow("float cmyk")<<"float_cmyk"<<QColor::fromCmykF(0,0.26,0.99,0.1,1.0);
    QTest::newRow("float hsv")<<"float_hsv"<<QColor::fromHsvF(0.12,0.99,0.99,1.0);
    QTest::newRow("float hsl")<<"float_hsl"<<QColor::fromHslF(0.12,0.98,0.5,1.0);

    QTest::newRow("float graya")<<"float_graya"<<QColor::fromRgbF(0.5,0.5,0.5,1.0);
    QTest::newRow("float rgba")<<"float_rgba"<<QColor::fromRgbF(0.5,0,0,1.0);
    QTest::newRow("float cmyka")<<"float_cmyka"<<QColor::fromCmykF(0,0.26,0.99,0.1,1.0);
    QTest::newRow("float hsva")<<"float_hsva"<<QColor::fromHsvF(0.12,0.99,0.99,1.0);
    QTest::newRow("float hsla")<<"float_hsla"<<QColor::fromHslF(0.12,0.98,0.5,1.0);

    QTest::newRow("int gray")<<"int_gray"<<QColor::fromRgb(128,128,128,255);
    QTest::newRow("int rgb")<<"int_rgb"<<QColor::fromRgb(128,0,0,255);
    QTest::newRow("int cmyk")<<"int_cmyk"<<QColor::fromCmyk(0,66,252,25,255);
    QTest::newRow("int hsv")<<"int_hsv"<<QColor::fromHsv(44,252,252,255);
    QTest::newRow("int hsl")<<"int_hsl"<<QColor::fromHsl(44,250,128,255);

    QTest::newRow("int graya")<<"int_graya"<<QColor::fromRgb(128,128,128,255);
    QTest::newRow("int rgba")<<"int_rgba"<<QColor::fromRgb(128,0,0,255);
    QTest::newRow("int cmyka")<<"int_cmyka"<<QColor::fromCmyk(0,66,252,25,255);
    QTest::newRow("int hsva")<<"int_hsva"<<QColor::fromHsv(44,252,252,255);
    QTest::newRow("int hsla")<<"int_hsla"<<QColor::fromHsl(44,250,128,255);
}

void DsSettingsTest::get_QColor_from_Arrays_shouldReturnAValidQColor(){
    QFETCH(QString, key);
    QFETCH(QColor, result);

    QCOMPARE(test_settings->get<QColor>("test.color.arrays." + key), result);
}

void DsSettingsTest::get_QColor_from_Tables_shouldReturnAValidQColor_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QColor>("result");

    QTest::newRow("float rgb")<<"float_rgb"<<QColor::fromRgbF(0.5,0,0,1.0);
    QTest::newRow("float cmyk")<<"float_cmyk"<<QColor::fromCmykF(0,0.26,0.99,0.1,1.0);
    QTest::newRow("float hsv")<<"float_hsv"<<QColor::fromHsvF(0.12,0.99,0.99,1.0);
    QTest::newRow("float hsl")<<"float_hsl"<<QColor::fromHslF(0.12,0.98,0.5,1.0);

    QTest::newRow("float rgba")<<"float_rgba"<<QColor::fromRgbF(0.5,0,0,1.0);
    QTest::newRow("float cmyka")<<"float_cmyka"<<QColor::fromCmykF(0,0.26,0.99,0.1,1.0);
    QTest::newRow("float hsva")<<"float_hsva"<<QColor::fromHsvF(0.12,0.99,0.99,1.0);
    QTest::newRow("float hsla")<<"float_hsla"<<QColor::fromHslF(0.12,0.98,0.5,1.0);

    QTest::newRow("int rgb")<<"int_rgb"<<QColor::fromRgb(128,0,0,255);
    QTest::newRow("int cmyk")<<"int_cmyk"<<QColor::fromCmyk(0,66,252,25,255);
    QTest::newRow("int hsv")<<"int_hsv"<<QColor::fromHsv(44,252,252,255);
    QTest::newRow("int hsl")<<"int_hsl"<<QColor::fromHsl(44,250,128,255);

    QTest::newRow("int rgba")<<"int_rgba"<<QColor::fromRgb(128,0,0,255);
    QTest::newRow("int cmyka")<<"int_cmyka"<<QColor::fromCmyk(0,66,252,25,255);
    QTest::newRow("int hsva")<<"int_hsva"<<QColor::fromHsv(44,252,252,255);
    QTest::newRow("int hsla")<<"int_hsla"<<QColor::fromHsl(44,250,128,255);
}

void DsSettingsTest::get_QColor_from_Tables_shouldReturnAValidQColor(){
    QFETCH(QString, key);
    QFETCH(QColor, result);

    QCOMPARE(test_settings->get<QColor>("test.color.tables." + key), result);
}

//*****************
//QDate, QTime, QDateTime
//*****************
void DsSettingsTest::get_QTime_from_iso8601String_shouldReturnAValidQTime()
{
    QCOMPARE(test_settings->get<QTime>("test.time.strings.ISO8601_default"), QTime(17,30,30));
}

void DsSettingsTest::get_QDate_from_iso8601String_shouldReturnAValidQDate()
{
    QCOMPARE(test_settings->get<QDate>("test.date.strings.ISO8601_default"), QDate(2023,1,30));
}

void DsSettingsTest::get_QDateTime_from_iso8601String_shouldReturnAValidQDateTime()
{
    QCOMPARE(test_settings->get<QDateTime>("test.datetime.strings.ISO8601_default"),
             QDateTime(QDate(2023,1,30), QTime(17,30,30)));
}

void DsSettingsTest::get_dateTime_fromNonIsoStrings_shouldNotConvert()
{
    // setDateFormat() / setCustomDateFormat() are gone. QVariant parses date and
    // time strings as ISO-8601 only, so these fixture values no longer convert.
    QVERIFY(!test_settings->get<QTime>("test.time.strings.custom_time").isValid());
    QVERIFY(!test_settings->get<QDate>("test.date.strings.text_date").isValid());
    QVERIFY(!test_settings->get<QDate>("test.date.strings.rfc2822").isValid());
    QVERIFY(!test_settings->get<QDate>("test.date.strings.custom_date").isValid());
    QVERIFY(!test_settings->get<QDateTime>("test.datetime.strings.custom_date_time").isValid());
}

void DsSettingsTest::get_QTime_from_tomlDateTime_shouldReturnAValidQTime_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QTime>("result");

    QTest::newRow("offset1")<<"odt1"<<QTime(7,32);
    QTest::newRow("offset2")<<"odt2"<<QTime(0,32);
    QTest::newRow("offset3")<<"odt3"<<QTime(0,32,00,999);
    QTest::newRow("local datetime")<<"ldt1"<<QTime(7,32);
    QTest::newRow("local datetime fract seconds")<<"ldt2"<<QTime(0,32,00,999);
    QTest::newRow("local time")<<"lt1"<<QTime(7,32);
    QTest::newRow("local time fract seconds")<<"lt2"<<QTime(0,32,00,999);
    // "local date" (ld1) dropped: QVariant has no QDate -> QTime conversion.
}

void DsSettingsTest::get_QTime_from_tomlDateTime_shouldReturnAValidQTime(){
    QFETCH(QString, key);
    QFETCH(QTime, result);

    QCOMPARE(test_settings->get<QTime>("test.datetimes.toml." + key), result);
}

void DsSettingsTest::get_QDate_from_tomlDateTime_shouldReturnAValidQDate_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QDate>("result");

    QTest::newRow("offset1")<<"odt1"<<QDate(1979,5,27);
    QTest::newRow("offset2")<<"odt2"<<QDate(1979,5,27);
    QTest::newRow("offset3")<<"odt3"<<QDate(1979,5,27);
    QTest::newRow("local datetime")<<"ldt1"<<QDate(1979,5,27);
    QTest::newRow("local datetime fract seconds")<<"ldt2"<<QDate(1979,5,27);
    QTest::newRow("local date")<<"ld1"<<QDate(1979,5,27);
    QTest::newRow("local time")<<"lt1"<<QDate(); //invalid date
    QTest::newRow("local time fract seconds")<<"lt2"<<QDate(); //invalid date
}

void DsSettingsTest::get_QDate_from_tomlDateTime_shouldReturnAValidQDate(){
    QFETCH(QString, key);
    QFETCH(QDate, result);

    QCOMPARE(test_settings->get<QDate>("test.datetimes.toml." + key), result);
}

void DsSettingsTest::get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTime_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QDateTime>("result");

    QTest::newRow("offset1")<<"odt1"<<QDateTime(QDate(1979,5,27),QTime(7,32),QTimeZone::utc());
    QTest::newRow("offset2")<<"odt2"
                            <<QDateTime(QDate(1979,5,27),QTime(0,32),
                                        QTimeZone::fromSecondsAheadOfUtc(-7*3600));
    QTest::newRow("offset3")<<"odt3"
                            <<QDateTime(QDate(1979,5,27),QTime(0,32,00,999),
                                        QTimeZone::fromSecondsAheadOfUtc(-7*3600));
    QTest::newRow("local datetime")<<"ldt1"<<QDateTime(QDate(1979,5,27),QTime(7,32));
    QTest::newRow("local datetime fract seconds")<<"ldt2"<<QDateTime(QDate(1979,5,27),QTime(0,32,00,999));
    QTest::newRow("local date")<<"ld1"<<QDateTime(QDate(1979,5,27),QTime(0,0));
    // "local time" (lt1/lt2) dropped: QVariant has no QTime -> QDateTime conversion.
}

void DsSettingsTest::get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTime(){
    QFETCH(QString, key);
    QFETCH(QDateTime, result);

    QCOMPARE(test_settings->get<QDateTime>("test.datetimes.toml." + key), result);
}

void DsSettingsTest::get_dateTime_acrossTomlTypes_shouldOnlyConvertWhereQVariantCan()
{
    // QMetaType registers QDateTime->QDate, QDateTime->QTime and QDate->QDateTime,
    // but nothing from a bare QTime and nothing from QDate to QTime.
    QVERIFY(!test_settings->get<QTime>("test.datetimes.toml.ld1").isValid());
    QVERIFY(!test_settings->get<QDateTime>("test.datetimes.toml.lt1").isValid());
}

//*****************
//Geometry
//*****************
void DsSettingsTest::get_vectors_fromTomlArrays_shouldStayLists()
{
    // Numeric TOML arrays are QVariantLists now — they are not coerced into
    // QVector2D/3D/4D. Table forms ({x, y}, {x, y, z}, {w, x, y, z}) are the
    // supported way to express vectors.
    const QVariantList vec2 = test_settings->get<QVariantList>("test.geom.vectors.vec2");
    const QVariantList vec3 = test_settings->get<QVariantList>("test.geom.vectors.vec3");
    const QVariantList vec4 = test_settings->get<QVariantList>("test.geom.vectors.vec4");

    QCOMPARE(vec2.size(), 2);
    QCOMPARE(vec3.size(), 3);
    QCOMPARE(vec4.size(), 4);
    QCOMPARE(vec2[0].toInt(), 20);
    QCOMPARE(vec3[2].toInt(), 40);
    QCOMPARE(vec4[3].toInt(), 50);

    QVERIFY(test_settings->get<QVector2D>("test.geom.vectors.vec2").isNull());
    QVERIFY(test_settings->get<QVector3D>("test.geom.vectors.vec3").isNull());
}

void DsSettingsTest::get_QPoint_from_tables_shouldReturnAValidQPointF()
{
    QCOMPARE(test_settings->get<QPointF>("test.geom.elements.x_and_y"), QPointF(10,20));
}

void DsSettingsTest::get_QSize_from_tables_shouldReturnAValidQSizeF()
{
    QCOMPARE(test_settings->get<QSizeF>("test.geom.elements.w_and_h"), QSizeF(40,50));
}

void DsSettingsTest::get_QRect_from_tables_shouldReturnAValidQRectF_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QRectF>("result");

    QTest::newRow("table (x,y,w,h)")<<"test.geom.elements.rect_xywh"<<QRectF(10,20,40,50);
    QTest::newRow("table (x1,y1,x2,y2)")<<"test.geom.elements.rect_x1y1x2y2"<<QRectF(10,20,40,50);
    QTest::newRow("example rect 1")<<"example.example_rect_1"<<QRectF(10,5,100,100);
    QTest::newRow("example rect 3")<<"example.example_rect_3"<<QRectF(10,5,100,100);
    // Legacy table value + {type="rect"} metadata.
    QTest::newRow("example rect 4")<<"example.example_rect_4"<<QRectF(10,5,100,100);
}

void DsSettingsTest::get_QRect_from_tables_shouldReturnAValidQRectF()
{
    QFETCH(QString, key);
    QFETCH(QRectF, result);

    QCOMPARE(test_settings->get<QRectF>(key), result);
}

void DsSettingsTest::get_geom_fromUnknownKeySignatures_shouldStayMaps()
{
    // Only the documented key signatures are interpreted; {x1, y1} and {x2, y2}
    // are not among them, so they stay plain maps.
    QCOMPARE(test_settings->get<QVariantMap>("test.geom.elements.x1_and_y1").size(), 2);
    QCOMPARE(test_settings->get<QVariantMap>("test.geom.elements.x2_and_y2").size(), 2);
    QVERIFY(test_settings->get<QPointF>("test.geom.elements.x1_and_y1").isNull());
    QVERIFY(test_settings->get<QRectF>("test.geom.elements.x2_and_y2").isNull());
}

//*****************
//Containers
//*****************
void DsSettingsTest::get_QVariantList_shouldReturnAValidQVariantList_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QVariantList>("result");

    // {r=1, g=0, b=0, a=1} is now recognised as a colour key signature and,
    // because every channel is within 0..1, read on the unit scale.
    QVariantList simple = {
        QVariant(10),
        QVariant("string"),
        QVariant(10.5),
        QVariant::fromValue(QColor::fromRgbF(1.0, 0.0, 0.0, 1.0))
    };
    QVariantList list_of_maps = {
        QVariantMap({
            { "a", QVariant("first first")  },
            { "b", QVariant("first second") },
        }),
        QVariantMap({
            { "a", QVariant("second first") },
            { "b", QVariant("second second") }
        }),
    };

    QTest::newRow("without_meta")    << "list_wo_meta"          << simple;
    QTest::newRow("of_maps_w_meta")  << "list_of_maps_w_meta"   << list_of_maps;
}

void DsSettingsTest::get_QVariantList_shouldReturnAValidQVariantList()
{
    QFETCH(QString, key);
    QFETCH(QVariantList, result);

    QCOMPARE(test_settings->get<QVariantList>("test.list." + key), result);
}

void DsSettingsTest::get_QVariantList_intsOnly()
{
    const auto result = test_settings->get<QVariantList>("test.list.list_ints_only");
    QVariantList expected = {
        QVariant(qint64(1)), QVariant(qint64(2)), QVariant(qint64(3)),
        QVariant(qint64(4)), QVariant(qint64(5))
    };
    QCOMPARE(result, expected);
}

void DsSettingsTest::get_QVariantList_stringsOnly()
{
    const auto result = test_settings->get<QVariantList>("test.list.list_strings_only");
    QVariantList expected = {
        QVariant("alpha"), QVariant("beta"), QVariant("gamma")
    };
    QCOMPARE(result, expected);
}

void DsSettingsTest::get_QVariantList_nested()
{
    const auto result = test_settings->get<QVariantList>("test.list.list_nested");
    QVariantList inner1 = { QVariant(qint64(1)), QVariant(qint64(2)) };
    QVariantList inner2 = { QVariant(qint64(3)), QVariant(qint64(4)) };
    QVariantList expected = { QVariant(inner1), QVariant(inner2) };
    QCOMPARE(result, expected);
}

void DsSettingsTest::get_QVariantList_empty()
{
    // [[]] in TOML = array containing one empty array. A single-element array
    // whose only element is an array is unwrapped, so this reads as an empty list.
    QVERIFY(test_settings->contains("test.list.list_empty"));
    QVERIFY(test_settings->get<QVariantList>("test.list.list_empty").isEmpty());
}

void DsSettingsTest::get_QVariantList_allTypes()
{
    const auto list = test_settings->get<QVariantList>("test.list.list_all_types");
    QCOMPARE(list.size(), 5);
    QCOMPARE(list[0].toBool(), true);
    QCOMPARE(list[1].toLongLong(), 42LL);
    QCOMPARE(list[2].toDouble(), 3.14);
    QCOMPARE(list[3].toString(), QString("hello"));
    // Element 4 is a datetime — verify it converts
    QVERIFY(list[4].canConvert<QDateTime>());
    QCOMPARE(list[4].toDateTime().date(), QDate(1979, 5, 27));
}

void DsSettingsTest::get_QVariantList_nonexistentKey()
{
    QVERIFY(!test_settings->contains("test.list.does_not_exist"));
    QVERIFY(test_settings->get<QVariantList>("test.list.does_not_exist").isEmpty());
}

void DsSettingsTest::get_QVariantList_legacyMetaRequiresATypeKey()
{
    // The legacy "[value, {meta}]" form is only unwrapped when the metadata
    // table carries a "type" key. list_w_meta uses "types" (plural), so it is
    // left as a literal two-element array.
    const auto raw = test_settings->get<QVariantList>("test.list.list_w_meta");
    QCOMPARE(raw.size(), 2);

    const QVariantList payload = raw[0].toList();
    QCOMPARE(payload.size(), 4);
    QCOMPARE(payload[0].toLongLong(), 10LL);
    QCOMPARE(payload[1].toString(), QStringLiteral("string"));
    QCOMPARE(payload[2].toDouble(), 10.5);
    QCOMPARE(payload[3].value<QColor>(), QColor::fromRgbF(1.0, 0.0, 0.0, 1.0));

    QVERIFY(raw[1].toMap().contains("types"));
}

void DsSettingsTest::get_QVariantMap_shouldReturnAValidQVariantMap_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QVariantMap>("result");

    QVariantMap map = {
        {"int",QVariant(10)},
        {"string",QVariant("string")},
        {"float",QVariant(10.5)},
        {"obj",QVariantMap({{"a",QVariant("one")}})},
        {"array",QVariantList({QVariant(qint64(1))})},
        {"date",QVariant(QDateTime(QDate(1979,5,27),QTime(7,32),QTimeZone::utc()))}
    };

    QTest::newRow("inline table")<<"map_a"<<map;
    QTest::newRow("table header")<<"header"<<map;
}

void DsSettingsTest::get_QVariantMap_shouldReturnAValidQVariantMap()
{
    QFETCH(QString, key);
    QFETCH(QVariantMap, result);

    QCOMPARE(test_settings->get<QVariantMap>("test.maps." + key), result);
}

void DsSettingsTest::get_QVariantMap_stringsOnly()
{
    const auto result = test_settings->get<QVariantMap>("test.maps.map_strings_only");
    QVariantMap expected = {
        {"name", QVariant("Alice")},
        {"city", QVariant("NYC")},
        {"role", QVariant("dev")}
    };
    QCOMPARE(result, expected);
}

void DsSettingsTest::get_QVariantMap_nested()
{
    const auto map = test_settings->get<QVariantMap>("test.maps.map_nested");
    QCOMPARE(map["outer_key"].toString(), QString("outer_val"));

    // Verify nested map
    QVERIFY(map["inner"].canConvert<QVariantMap>());
    const auto inner = map["inner"].toMap();
    QCOMPARE(inner["inner_key"].toString(), QString("inner_val"));

    // Verify doubly nested map
    QVERIFY(inner["deep"].canConvert<QVariantMap>());
    const auto deep = inner["deep"].toMap();
    QCOMPARE(deep["deepest"].toString(), QString("found"));
}

void DsSettingsTest::get_QVariantMap_empty()
{
    QVERIFY(test_settings->contains("test.maps.map_empty"));
    QVERIFY(test_settings->get<QVariantMap>("test.maps.map_empty").isEmpty());
}

void DsSettingsTest::get_QVariantMap_allTypes()
{
    const auto map = test_settings->get<QVariantMap>("test.maps.map_all_types");

    QCOMPARE(map["b"].toBool(), true);
    QCOMPARE(map["i"].toLongLong(), 42LL);
    QCOMPARE(map["f"].toDouble(), 3.14);
    QCOMPARE(map["s"].toString(), QString("hello"));

    QVERIFY(map["dt"].canConvert<QDateTime>());
    QCOMPARE(map["dt"].toDateTime().date(), QDate(1979, 5, 27));

    QVERIFY(map["arr"].canConvert<QVariantList>());
    QCOMPARE(map["arr"].toList().size(), 2);

    QVERIFY(map["sub"].canConvert<QVariantMap>());
    QCOMPARE(map["sub"].toMap()["x"].toLongLong(), 1LL);
}

void DsSettingsTest::get_QVariantMap_nonexistentKey()
{
    QVERIFY(!test_settings->contains("test.maps.does_not_exist"));
    QVERIFY(test_settings->get<QVariantMap>("test.maps.does_not_exist").isEmpty());
}

void DsSettingsTest::get_QVariantMap_arrayOfTablesStaysAList()
{
    // [[test.maps.header_meta]] is a TOML array of tables. The legacy
    // "[value, {meta}]" unwrap deliberately does not apply when the first
    // element is itself a table, so both tables are preserved.
    const auto list = test_settings->get<QVariantList>("test.maps.header_meta");
    QCOMPARE(list.size(), 2);

    const auto data = list[0].toMap();
    QCOMPARE(data["fruit_1"].toString(), QStringLiteral("apple"));
    QCOMPARE(data["fruit_2"].toString(), QStringLiteral("banana"));
    QCOMPARE(data["array"].toList().size(), 3);
    QCOMPARE(data["map"].toMap()["b"].toLongLong(), 2LL);

    QCOMPARE(list[1].toMap()["types"].toList().size(), 4);

    // ...and it is therefore not readable as a map.
    QVERIFY(test_settings->get<QVariantMap>("test.maps.header_meta").isEmpty());
}

void DsSettingsTest::get_shouldReturnTheDefaultWhenTheSettingDoesNotExist()
{
    QVERIFY(!test_settings->contains("does_not_exist"));
    QCOMPARE(test_settings->getOr<QString>("does_not_exist", QStringLiteral("fallback")),
             QStringLiteral("fallback"));
}

void DsSettingsTest::provenance_shouldReportTheSourceOfAKey()
{
    // Replaces getWithMeta(): the metadata table is gone, but the source of a
    // value is still recoverable.
    QCOMPARE(test_settings->get<QString>("test.meta.meta_example"),
             QStringLiteral("string-value"));
    QVERIFY(test_settings->provenance("test.meta.meta_example").endsWith("test_settings.toml"));
    QVERIFY(test_settings->provenance("test.list.list_w_meta").endsWith("test_settings.toml"));

    QVERIFY(test_settings->provenance("does.not.exist").isEmpty());

    test_settings->setDefault<int>("synthetic.default_key", 5);
    QCOMPARE(test_settings->provenance("synthetic.default_key"), QStringLiteral("default"));

    test_settings->set<int>("test.int.int_positive", 7);
    QCOMPARE(test_settings->provenance("test.int.int_positive"), QStringLiteral("override"));
    QCOMPARE(test_settings->get<int>("test.int.int_positive"), 7);
}

void DsSettingsTest::read_notable()
{
    QCOMPARE(test_settings->get<QString>("no_table"), QStringLiteral("test value"));
}

void DsSettingsTest::settingsFile_shouldStripLegacyMetadata()
{
    dsqt::SettingsFile settings(nullptr, {settingsDir()});
    settings.setFileName("test_settings.toml");

    QCOMPARE(settings.find<QString>("no_table_arrayed"), QString("test value"));
    QCOMPARE(settings.find<QString>("test.meta.meta_example"), QString("string-value"));
}

void DsSettingsTest::settingsFile_shouldUnwrapLegacyArrays()
{
    dsqt::SettingsFile settings(nullptr, {settingsDir()});
    settings.setFileName("test_settings.toml");

    const QVariantList list = settings.find<QVariantList>("test.list.list_strings_only");
    QCOMPARE(list.size(), 3);
    QCOMPARE(list[0].toString(), QString("alpha"));
    QCOMPARE(list[1].toString(), QString("beta"));
    QCOMPARE(list[2].toString(), QString("gamma"));
}

void DsSettingsTest::settingsFile_shouldInterpretLegacyMetadataColors()
{
    dsqt::SettingsFile settings(nullptr, {settingsDir()});
    settings.setFileName("test_settings.toml");

    QCOMPARE(settings.find<QColor>("test.color.arrays.float_rgb"),
             QColor::fromRgbF(0.5, 0.0, 0.0, 1.0));
    QCOMPARE(settings.find<QColor>("test.color.arrays.int_hsv"),
             QColor::fromHsv(44, 252, 252, 255));
    QCOMPARE(settings.find<QColor>("test.color.tables.float_rgb"),
             QColor::fromRgbF(0.5, 0.0, 0.0, 1.0));
    QCOMPARE(settings.find<QColor>("test.color.tables.int_cmyk"),
             QColor::fromCmyk(0, 66, 252, 25, 255));
    QCOMPARE(settings.find<QColor>("example.example_color_1"),
             QColor::fromRgbF(0.5, 0.5, 0.5, 1.0));

    // Table and array values carrying a bare {type="color"} marker.
    QCOMPARE(settings.find<QColor>("example.example_color_2"),
             QColor::fromRgbF(0.5, 0.5, 0.5, 1.0));
    QCOMPARE(settings.find<QColor>("example.example_color_3"),
             QColor::fromRgbF(0.5, 0.5, 0.5, 1.0));
    QCOMPARE(settings.find<QColor>("example.example_color_4"),
             QColor::fromRgbF(0.5, 0.5, 0.5, 1.0));
}

static void writeTextFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QCOMPARE(file.write(contents), static_cast<qint64>(contents.size()));
}

void DsSettingsTest::settingsFile_shouldNotCollapseArrayOfTablesWithATypeKey()
{
    // Counterpart to the legacy-metadata tests: a two-entry array of tables must
    // survive intact even though the second table has a "type" key. Only
    // type="color" and type="rect" permit a table in the value slot.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    writeTextFile(QDir(dir.path()).filePath("settings.toml"),
                  "[[slide]]\n"
                  "type = \"video\"\n"
                  "src = \"a.mp4\"\n"
                  "\n"
                  "[[slide]]\n"
                  "type = \"image\"\n"
                  "src = \"b.png\"\n");

    dsqt::SettingsFile settings(nullptr, {dir.path()});
    settings.setFileName("settings.toml");

    const QVariantList slides = settings.find<QVariantList>("slide");
    QCOMPARE(slides.size(), 2);
    QCOMPARE(slides[0].toMap()["src"].toString(), QStringLiteral("a.mp4"));
    QCOMPARE(slides[1].toMap()["src"].toString(), QStringLiteral("b.png"));
}

void DsSettingsTest::settingsFile_shouldLoadExtraFilesBetweenBaseFilesAndOverrides()
{
    QTemporaryDir lowDir;
    QTemporaryDir highDir;
    QVERIFY(lowDir.isValid());
    QVERIFY(highDir.isValid());

    writeTextFile(QDir(lowDir.path()).filePath("settings.toml"),
                  "value = 10\n"
                  "base_only = 11\n");
    writeTextFile(QDir(highDir.path()).filePath("settings.toml"),
                  "value = 20\n");
    writeTextFile(QDir(lowDir.path()).filePath("extra.toml"),
                  "value = 30\n"
                  "extra_only = 31\n");
    writeTextFile(QDir(highDir.path()).filePath("extra.toml"),
                  "value = 40\n");

    dsqt::SettingsFile settings(nullptr, {lowDir.path(), highDir.path()});
    settings.setDefault("value", 5);
    settings.setFileName("settings.toml");
    settings.setExtraFiles({"extra.toml"});

    QCOMPARE(settings.find<int>("value"), 40);
    QCOMPARE(settings.find<int>("base_only"), 11);
    QCOMPARE(settings.find<int>("extra_only"), 31);
    QVERIFY(settings.provenance("value").endsWith("extra.toml"));

    settings.setOverride("value", 50);
    QCOMPARE(settings.find<int>("value"), 50);
    QCOMPARE(settings.provenance("value"), QStringLiteral("override"));
}

void DsSettingsTest::settingsFile_shouldPruneOverridesThatMatchReloadedFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = QDir(dir.path()).filePath("settings.toml");
    writeTextFile(filePath, "width = 100\n");

    dsqt::SettingsFile settings(nullptr, {dir.path()});
    settings.setFileName("settings.toml");
    settings.setOverride("width", 300);
    settings.setOverride("height", 400);

    QCOMPARE(settings.saveOverridesTo(filePath), QString{});
    QCOMPARE(settings.overrides().size(), 2);

    settings.reload();

    QVERIFY(settings.overrides().isEmpty());
    QCOMPARE(settings.find<int>("width"), 300);
    QCOMPARE(settings.find<int>("height"), 400);
}

void DsSettingsTest::settingsFile_shouldKeepOverridesSavedOutsideSearchPaths()
{
    QTemporaryDir searchDir;
    QTemporaryDir otherDir;
    QVERIFY(searchDir.isValid());
    QVERIFY(otherDir.isValid());

    const QString watchedFilePath = QDir(searchDir.path()).filePath("settings.toml");
    const QString otherFilePath = QDir(otherDir.path()).filePath("settings.toml");
    writeTextFile(watchedFilePath, "width = 100\n");

    dsqt::SettingsFile settings(nullptr, {searchDir.path()});
    settings.setFileName("settings.toml");
    settings.setOverride("width", 300);

    QCOMPARE(settings.saveOverridesTo(otherFilePath), QString{});
    settings.reload();

    QCOMPARE(settings.overrides().value("width").toInt(), 300);
    QCOMPARE(settings.find<int>("width"), 300);
}

QTEST_MAIN(DsSettingsTest)
#include "tst_dssettingstest.moc"
