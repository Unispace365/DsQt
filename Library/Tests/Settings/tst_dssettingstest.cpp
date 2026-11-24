#include <QtTest>
#include <settings/dsSettings.h>
#include <settings/dsSettingsFile.h>
#include <settings/dsQmlSettingsProxy.h>
#include <settings/dsSettingsTreeModel.h>
#include <settings/dsTomlEditor.h>
#include <core/dsEnvironment.h>
#include <QPointF>
#include <QQuaternion>
#include <QRectF>
#include <QSizeF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
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
    void find_variant_shouldPreserveTheStoredValue_data();
    void find_variant_shouldPreserveTheStoredValue();
    void find_variant_shouldPreserveDefaults();
    void qmlProxy_shouldReadTypedValues();
    void qmlProxy_shouldPreserveDefaults();
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

    // SettingsFile::bind
    void bind_shouldCallTheCallbackImmediately();
    void bind_shouldCallTheCallbackWhenTheValueChanges();
    void bind_shouldCallTheCallbackForANestedKey();
    void bind_shouldUseTheDefaultForAnUnknownKey();
    void bind_shouldCallAMemberFunction();
    void bind_shouldReplaceTheCallbackWhenReboundOnTheSameKeyAndContext();
    void bind_shouldStopWhenTheContextIsDestroyed();

    // Settings::bind — registry lookup and deferral
    void registryBind_shouldResolveImmediatelyWhenTheFileIsRegistered();
    void registryBind_shouldDeferUntilTheFileIsAdded();
    void registryBind_deferred_shouldKeepTrackingAfterItResolves();
    void registryBind_deferred_shouldReplayInCallOrder();
    void registryBind_deferred_shouldBeDroppedWhenTheContextDies();
    void registryBind_deferred_shouldNotResolveOnAnUnrelatedFile();

    // SettingsTreeModel (settings viewer editing)
    void treeModel_parseDisplayString_shouldRoundTrip_data();
    void treeModel_parseDisplayString_shouldRoundTrip();
    void treeModel_parseDisplayString_shouldAcceptLooseSeparators();
    void treeModel_parseDisplayString_shouldRejectMismatchedText_data();
    void treeModel_parseDisplayString_shouldRejectMismatchedText();
    void treeModel_setData_shouldKeepTheOriginalType_data();
    void treeModel_setData_shouldKeepTheOriginalType();
    void treeModel_setData_shouldRejectTextThatDoesNotFit();

    // Schema files (sliders and dropdowns in the settings viewer)
    void schema_shouldLoadEntriesFromTheSchemaFile();
    void schema_laterFilesShouldOverrideEarlierOnesKeyByKey();
    void schema_shouldBeReadAgainOnReload();
    void schemaEntry_slider_shouldSnapToTheStepGrid();
    void schemaEntry_slider_shouldKeepAnOffGridMaxReachable();
    void schemaEntry_options_shouldConvertToTheSettingType();
    void schemaEntry_shouldIgnoreWhatDoesNotFitTheType();
    void treeModel_schema_shouldDriveDisplayTooltipAndClamping();
    void treeModel_valueChanges_shouldUpdateRowsInPlace();

    // Paths (browse button in the settings viewer)
    void environment_contractPath_data();
    void environment_contractPath();
    void schemaEntry_editor_shouldSelectFileOrFolderForStrings();
    void schemaEntry_detectPath_data();
    void schemaEntry_detectPath();
    void treeModel_paths_shouldBeDetectedAndFlaggedWhenMissing();
    void schemaEntry_localPath_data();
    void schemaEntry_localPath();
    void treeModel_fileUrls_shouldBeCheckedAsPaths();
    void schemaEntry_switch_shouldBeTheDefaultForBooleans();
    void treeModel_switch_shouldFlipBooleans();

    // Restart marker
    void schemaEntry_restart_shouldAcceptBoolsAndLegacyStrings();
    void treeModel_restart_shouldExplainItselfInTheTooltip();

    // DsQt's built-in schema fragments
    void librarySchema_shouldBeCompiledIntoTheModules();
    void librarySchema_shouldLayerUnderTheApplicationSchema();
    void schema_restart_shouldBeInheritedFromATable();

    // Saving overrides into a hand-written file (settings::editToml)
    void tomlEdit_changedScalar_shouldOnlyChangeItsValue();
    void tomlEdit_string_shouldKeepItsQuotingStyle();
    void tomlEdit_equalValues_shouldLeaveTheTextAlone();
    void tomlEdit_array_ofTheSameLength_shouldChangeElementsInPlace();
    void tomlEdit_array_ofAnotherLength_shouldKeepItsLayout();
    void tomlEdit_array_legacyDoubleBrackets_shouldWriteTheInnerArray();
    void tomlEdit_inlineTable_shouldChangeOnlyTheChangedComponent();
    void tomlEdit_tableOfComponents_shouldChangeOnlyTheChangedComponent();
    void tomlEdit_newKey_shouldBeAppendedToItsTable();
    void tomlEdit_newKey_inDottedOrInlineTables();
    void tomlEdit_newRootKey_shouldGoBeforeTheFirstTable();
    void tomlEdit_newTable_shouldBeAppendedAtTheEnd();
    void tomlEdit_crlf_shouldBeKept();
    void tomlEdit_emptyText_shouldGetJustTheValues();
    void tomlEdit_arrayOfTables_shouldBeRefused();
    void tomlEdit_invalidToml_shouldBeRefused();
    void saveOverridesTo_shouldEditTheFileInPlace();
    void registry_shouldNoticeASearchFolderCreatedLater();
    void tomlRemove_shouldDropOnlyTheKeyLines();
    void tomlRemove_inlineTableMembers();
    void tomlRemove_tableValueKeepsItsHeader();
    void tomlRemove_arrayOfTablesShouldBeRefused();
    void tomlKeysSet_shouldListTheKeysWithTheirValues();
    void removeKeysFromFile_shouldEditInPlace();

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

void DsSettingsTest::find_variant_shouldPreserveTheStoredValue_data()
{
    QTest::addColumn<QVariant>("stored");
    QTest::newRow("string") << QVariant(QStringLiteral("redhat"));
    QTest::newRow("empty string") << QVariant(QStringLiteral(""));
    QTest::newRow("false") << QVariant(false);
    QTest::newRow("zero") << QVariant(0);
    QTest::newRow("integer64") << QVariant::fromValue(qint64(1) << 40);
    QTest::newRow("double") << QVariant(1.25);
    QTest::newRow("color") << QVariant::fromValue(QColor("#441f7e"));
    QTest::newRow("date") << QVariant(QDate(2026, 10, 3));
    QTest::newRow("list") << QVariant(QVariantList{1, QStringLiteral("two")});
    QTest::newRow("empty list") << QVariant(QVariantList{});
    QTest::newRow("map") << QVariant(QVariantMap{{"style", QStringLiteral("redhat")}});
    QTest::newRow("empty map") << QVariant(QVariantMap{});
}

void DsSettingsTest::find_variant_shouldPreserveTheStoredValue()
{
    QFETCH(QVariant, stored);
    auto& settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});
    dsqt::Settings::add("test_settings");
    auto* file = settings.settingsFile("test_settings");
    QVERIFY(file != nullptr);
    file->setOverride("variant.value", stored);

    const QVariant fallback = QStringLiteral("fallback");
    const QVariant actual = file->find<QVariant>("variant.value", fallback);
    QCOMPARE(actual.metaType(), stored.metaType());
    QCOMPARE(actual, stored);
    QCOMPARE(file->get<QVariant>("variant.value"), stored);
    QCOMPARE(file->getOr<QVariant>("variant.value", fallback), stored);
    QCOMPARE(dsqt::Settings::find<QVariant>("test_settings", "variant.value", fallback), stored);
    // Also exercise data loaded from TOML, rather than only runtime overrides.
    QCOMPARE(file->get<QVariant>("no_table"), QVariant(QStringLiteral("test value")));
}

void DsSettingsTest::find_variant_shouldPreserveDefaults()
{
    const QVariant fallback = QVariantMap{{"fallback", true}};
    QVERIFY(!test_settings->find<QVariant>("not_exist").isValid());
    QCOMPARE(test_settings->find<QVariant>("not_exist", fallback), fallback);
    QCOMPARE(test_settings->getOr<QVariant>("not_exist", fallback), fallback);
    QCOMPARE(dsqt::Settings::find<QVariant>("no_such_settings", "key", fallback), fallback);

    auto& settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});
    dsqt::Settings::add("test_settings");
    QCOMPARE(dsqt::Settings::find<QVariant>("test_settings", "not_exist", fallback), fallback);
}

void DsSettingsTest::qmlProxy_shouldReadTypedValues()
{
    auto& settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});
    dsqt::DsQmlSettingsProxy proxy;
    proxy.setTarget("test_settings");
    auto* file = settings.settingsFile("test_settings");
    QVERIFY(file != nullptr);
    file->setOverride("waffles.keyboard.style", QStringLiteral("redhat"));
    proxy.setPrefix("waffles.keyboard");
    QCOMPARE(proxy.getString("style", "waffles"), QVariant(QStringLiteral("redhat")));
    file->setOverride("waffles.keyboard.style", QStringLiteral(""));
    QCOMPARE(proxy.getString("style", "waffles"), QVariant(QStringLiteral("")));
    file->setOverride("waffles.keyboard.enabled", false);
    QCOMPARE(proxy.getBool("enabled", true), QVariant(false));
    file->setOverride("waffles.keyboard.size", 0);
    QCOMPARE(proxy.getInt("size", 42), QVariant(0));

    proxy.setPrefix("test.int");
    QCOMPARE(proxy.getInt("int_from_string", -1), QVariant(1024));
    proxy.setPrefix("test.color.strings");
    QCOMPARE(proxy.getColor("name", QColor(Qt::red)), QVariant::fromValue(QColor(Qt::blue)));
    proxy.setPrefix("");
    QCOMPARE(proxy.getString("no_table"), QVariant(QStringLiteral("test value")));
}

void DsSettingsTest::qmlProxy_shouldPreserveDefaults()
{
    const QVariant fallback = QStringLiteral("fallback");
    dsqt::DsQmlSettingsProxy proxy;
    QCOMPARE(proxy.getString("no_table", fallback), fallback);
    auto& settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});
    proxy.setTarget("test_settings");
    QCOMPARE(proxy.getString("not_exist", fallback), fallback);
    QVERIFY(!proxy.getString("not_exist").isValid());
    // Arrays and tables must not become scalar strings or numbers.
    QCOMPARE(proxy.getString("test.strings.string_from_array", fallback), fallback);
    QCOMPARE(proxy.getInt("test.strings.string_from_table", fallback), fallback);
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

//*****************
// Saving overrides: settings::editToml
//*****************

// Edits `text`; returns "<error>" (and logs why) if the edit was refused.
static QString editedToml(const QString &text, const QVariantMap &values, QStringList *changed = nullptr)
{
    const dsqt::settings::TomlEditResult result = dsqt::settings::editToml(text, values);
    if (!result.error.isEmpty()) {
        qWarning() << "editToml:" << result.error;
        return QStringLiteral("<error>");
    }
    if (changed)
        *changed = result.changed;
    return result.text;
}

void DsSettingsTest::tomlEdit_changedScalar_shouldOnlyChangeItsValue()
{
    const QString before = QStringLiteral(
        "# Window\n"
        "[window]\n"
        "width  = 1920   # pixels\n"
        "height = 1080\n"
        "scale = 1.5\n"
        "\n"
        "fullscreen = false # for now\n");
    QStringList changed;
    const QString after = editedToml(before,
                                     {{"window", QVariantMap{{"width", 1024},
                                                             {"scale", 2.0},
                                                             {"fullscreen", true},
                                                             {"height", 1080}}}},
                                     &changed);
    QCOMPARE(after, QStringLiteral(
        "# Window\n"
        "[window]\n"
        "width  = 1024   # pixels\n"
        "height = 1080\n"
        "scale = 2.0\n"
        "\n"
        "fullscreen = true # for now\n"));
    // height already held 1080, so it is not reported.
    QCOMPARE(changed, (QStringList{"window.fullscreen", "window.scale", "window.width"}));

    // A float written into an integer-looking value stays an integer when whole.
    QCOMPARE(editedToml("x = 3\n", {{"x", 4.0}}), QStringLiteral("x = 4\n"));
    QCOMPARE(editedToml("x = 3\n", {{"x", 4.25}}), QStringLiteral("x = 4.25\n"));
}

void DsSettingsTest::tomlEdit_string_shouldKeepItsQuotingStyle()
{
    const QString before = QStringLiteral(
        "basic = \"one\" # comment\n"
        "literal = 'C:\\path'\n"
        "color = '#ff0000'\n"
        "quoteInLiteral = 'plain'\n");
    const QString after = editedToml(before,
                                     {{"basic", QStringLiteral("two \"2\"")},
                                      {"literal", QStringLiteral("D:\\other")},
                                      {"color", QColor(0, 255, 0)},
                                      {"quoteInLiteral", QStringLiteral("it's")}});
    QCOMPARE(after, QStringLiteral(
        "basic = \"two \\\"2\\\"\" # comment\n"
        "literal = 'D:\\other'\n"
        "color = '#00ff00'\n"
        "quoteInLiteral = \"it's\"\n"));
}

void DsSettingsTest::tomlEdit_equalValues_shouldLeaveTheTextAlone()
{
    const QString before = QStringLiteral(
        "a = 0x10\n"
        "pos = {x=1,y=2}\n"
        "tint = \"red\"\n"
        "list = [ 1,2 , 3 ]\n");
    QStringList changed{"something"};
    const QString after = editedToml(before,
                                     {{"a", 16},
                                      {"pos", QPointF(1, 2)},
                                      {"tint", QColor(Qt::red)},
                                      {"list", QVariantList{1, 2, 3}}},
                                     &changed);
    QCOMPARE(after, before);
    QVERIFY(changed.isEmpty());
}

void DsSettingsTest::tomlEdit_array_ofTheSameLength_shouldChangeElementsInPlace()
{
    const QString before = QStringLiteral(
        "names = [\n"
        "    'alpha', # first\n"
        "    \"beta\",  # second\n"
        "]\n"
        "nums = [1, 2, 3]\n");
    const QString after = editedToml(before,
                                     {{"names", QVariantList{"alpha", "gamma"}},
                                      {"nums", QVariantList{1, 5, 3}}});
    QCOMPARE(after, QStringLiteral(
        "names = [\n"
        "    'alpha', # first\n"
        "    \"gamma\",  # second\n"
        "]\n"
        "nums = [1, 5, 3]\n"));
}

void DsSettingsTest::tomlEdit_array_ofAnotherLength_shouldKeepItsLayout()
{
    const QString before = QStringLiteral(
        "nums = [1, 2, 3] # three\n"
        "[menu]\n"
        "  items = [\n"
        "      'a',\n"
        "      'b',\n"
        "  ]\n");
    const QString after = editedToml(before,
                                     {{"nums", QVariantList{1, 2}},
                                      {"menu", QVariantMap{{"items", QVariantList{"a", "b", "c"}}}}});
    QCOMPARE(after, QStringLiteral(
        "nums = [1, 2] # three\n"
        "[menu]\n"
        "  items = [\n"
        "      'a',\n"
        "      'b',\n"
        "      'c',\n"
        "  ]\n"));
}

void DsSettingsTest::tomlEdit_array_legacyDoubleBrackets_shouldWriteTheInnerArray()
{
    QCOMPARE(editedToml("list = [[1, 2]]\n", {{"list", QVariantList{1, 3}}}),
             QStringLiteral("list = [[1, 3]]\n"));
    QCOMPARE(editedToml("width = [100, {type = \"int\"}]\n", {{"width", 200}}),
             QStringLiteral("width = [200, {type = \"int\"}]\n"));
}

void DsSettingsTest::tomlEdit_inlineTable_shouldChangeOnlyTheChangedComponent()
{
    const QString before = QStringLiteral(
        "pos  = { x = 10, y = 20 }   # top left\n"
        "size = {width=800, height=600}\n"
        "tint = { r = 255, g = 0, b = 0 }\n"
        "fade = { r = 1.0, g = 0.5, b = 0.0 }\n"
        "obj  = { name = 'a', count = 1 }\n");
    const QString after = editedToml(before,
                                     {{"pos", QPointF(10, 25)},
                                      {"size", QSizeF(1024, 600)},
                                      {"tint", QColor(255, 128, 0)},
                                      {"fade", QColor::fromRgbF(1.0, 0.5, 1.0)},
                                      {"obj", QVariantMap{{"count", 2}}}});
    QCOMPARE(after, QStringLiteral(
        "pos  = { x = 10, y = 25 }   # top left\n"
        "size = {width=1024, height=600}\n"
        "tint = { r = 255, g = 128, b = 0 }\n"
        "fade = { r = 1.0, g = 0.5, b = 1.0 }\n"
        "obj  = { name = 'a', count = 2 }\n"));
}

void DsSettingsTest::tomlEdit_tableOfComponents_shouldChangeOnlyTheChangedComponent()
{
    const QString before = QStringLiteral(
        "[window.rect]\n"
        "x = 0\n"
        "y = 0 # top\n"
        "w = 1920\n"
        "h = 1080\n");
    const QString after = editedToml(before,
                                     {{"window", QVariantMap{{"rect", QRectF(0, 0, 1920, 1200)}}}});
    QCOMPARE(after, QStringLiteral(
        "[window.rect]\n"
        "x = 0\n"
        "y = 0 # top\n"
        "w = 1920\n"
        "h = 1200\n"));
}

void DsSettingsTest::tomlEdit_newKey_shouldBeAppendedToItsTable()
{
    const QString before = QStringLiteral(
        "[a]\n"
        "    one = 1 # first\n"
        "\n"
        "# About b\n"
        "[b]\n"
        "two = 2\n");
    const QString after = editedToml(before,
                                     {{"a", QVariantMap{{"new", "x"}, {"pos", QPointF(1, 2)}}}});
    QCOMPARE(after, QStringLiteral(
        "[a]\n"
        "    one = 1 # first\n"
        "    new = \"x\"\n"
        "    pos = { x = 1, y = 2 }\n"
        "\n"
        "# About b\n"
        "[b]\n"
        "two = 2\n"));

    // A table with only a header gets the key straight below it; a quoted key stays quoted.
    QCOMPARE(editedToml("[empty]\n# nothing yet\n", {{"empty", QVariantMap{{"a b", 1}}}}),
             QStringLiteral("[empty]\n\"a b\" = 1\n# nothing yet\n"));
}

void DsSettingsTest::tomlEdit_newKey_inDottedOrInlineTables()
{
    const QString before = QStringLiteral(
        "[fruit]\n"
        "apple.color = 'red'\n"
        "pear = { color = 'green' }\n");
    const QString after = editedToml(before,
                                     {{"fruit", QVariantMap{{"apple", QVariantMap{{"size", 3}}},
                                                            {"pear", QVariantMap{{"size", 4}}}}}});
    QCOMPARE(after, QStringLiteral(
        "[fruit]\n"
        "apple.color = 'red'\n"
        "apple.size = 3\n"
        "pear = { color = 'green', size = 4 }\n"));
}

void DsSettingsTest::tomlEdit_newRootKey_shouldGoBeforeTheFirstTable()
{
    const QString before = QStringLiteral(
        "# File comment\n"
        "\n"
        "# About a\n"
        "[a]\n"
        "x = 1\n");
    QCOMPARE(editedToml(before, {{"top", true}}), QStringLiteral(
        "# File comment\n"
        "\n"
        "top = true\n"
        "\n"
        "# About a\n"
        "[a]\n"
        "x = 1\n"));

    // After the last root key when there is one.
    QCOMPARE(editedToml("name = 'n'\n\n[a]\nx = 1\n", {{"top", 2}}),
             QStringLiteral("name = 'n'\ntop = 2\n\n[a]\nx = 1\n"));
}

void DsSettingsTest::tomlEdit_newTable_shouldBeAppendedAtTheEnd()
{
    const QString before = QStringLiteral(
        "[a]\n"
        "x = 1 # no newline at the end");
    const QString after = editedToml(before,
                                     {{"a", QVariantMap{{"sub", QVariantMap{{"y", 2}}}}},
                                      {"b", QVariantMap{{"z", "three"}, {"w", 4}}}});
    QCOMPARE(after, QStringLiteral(
        "[a]\n"
        "x = 1 # no newline at the end\n"
        "\n"
        "[a.sub]\n"
        "y = 2\n"
        "\n"
        "[b]\n"
        "w = 4\n"
        "z = \"three\"\n"));
}

void DsSettingsTest::tomlEdit_crlf_shouldBeKept()
{
    const QString before = QStringLiteral(
        "# CRLF file\r\n"
        "[a]\r\n"
        "x = 1\r\n"
        "list = [\r\n"
        "  1,\r\n"
        "]\r\n");
    const QString after = editedToml(before,
                                     {{"a", QVariantMap{{"x", 2}, {"y", 3}, {"list", QVariantList{1, 2}}}}});
    QCOMPARE(after, QStringLiteral(
        "# CRLF file\r\n"
        "[a]\r\n"
        "x = 2\r\n"
        "list = [\r\n"
        "  1,\r\n"
        "  2,\r\n"
        "]\r\n"
        "y = 3\r\n"));
    QCOMPARE(editedToml(QStringLiteral("a = 1\r\n"), {{"b", QVariantMap{{"c", 1}}}}),
             QStringLiteral("a = 1\r\n\r\n[b]\r\nc = 1\r\n"));
}

void DsSettingsTest::tomlEdit_emptyText_shouldGetJustTheValues()
{
    QCOMPARE(editedToml({}, {{"top", 1}, {"window", QVariantMap{{"size", QSizeF(800, 600)}}}}),
             QStringLiteral("top = 1\n"
                            "\n"
                            "[window]\n"
                            "size = { w = 800, h = 600 }\n"));
}

void DsSettingsTest::tomlEdit_arrayOfTables_shouldBeRefused()
{
    const QString before = QStringLiteral("[[slide]]\nname = 'a'\n");
    const auto result = dsqt::settings::editToml(
        before, {{"slide", QVariantList{QVariantMap{{"name", "b"}}}}});
    QVERIFY(result.error.contains(QStringLiteral("array of tables")));
    QCOMPARE(result.text, before);
}

void DsSettingsTest::tomlEdit_invalidToml_shouldBeRefused()
{
    const QString before = QStringLiteral("a = = 1\n");
    const auto result = dsqt::settings::editToml(before, {{"a", 2}});
    QVERIFY(!result.error.isEmpty());
    QCOMPARE(result.text, before);
}

void DsSettingsTest::saveOverridesTo_shouldEditTheFileInPlace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath("settings.toml");
    const QByteArray before = "# Hand-written\r\n"
                              "[window]\r\n"
                              "width = 100   # px\r\n"
                              "title = 'Main'\r\n";
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly)); // binary: keep the CRLFs
        file.write(before);
    }

    dsqt::SettingsFile settings(nullptr, {dir.path()});
    settings.setFileName("settings.toml");
    settings.setOverride("window.width", 300);
    settings.setOverride("window.height", 200);

    QStringList written;
    QCOMPARE(settings.saveOverridesTo(path, &written), QString{});
    QCOMPARE(written, (QStringList{"window.height", "window.width"}));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("# Hand-written\r\n"
                                        "[window]\r\n"
                                        "width = 300   # px\r\n"
                                        "title = 'Main'\r\n"
                                        "height = 200\r\n"));
    file.close();

    // A file that does not exist yet is created, folder and all.
    const QString fresh = QDir(dir.path()).filePath("sub/new.toml");
    QCOMPARE(settings.saveOverridesTo(fresh), QString{});
    QFile created(fresh);
    QVERIFY(created.open(QIODevice::ReadOnly));
    QCOMPARE(created.readAll(), QByteArray("[window]\nheight = 200\nwidth = 300\n"));
}

void DsSettingsTest::registry_shouldNoticeASearchFolderCreatedLater()
{
    // Saving into a search folder that does not exist yet (say Local Platform)
    // creates it. The watcher must notice, so the reload prunes the override
    // that the new file now holds.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString base = QDir(dir.path()).filePath("base");
    const QString later = QDir(dir.path()).filePath("later/sub");
    QVERIFY(QDir().mkpath(base));
    writeTextFile(QDir(base).filePath("watch.toml"), "width = 100\n");

    auto &settings = dsqt::Settings::instance();
    settings.setSearchPaths({base, later});
    dsqt::Settings::add("watch_test", "watch.toml");
    auto *sf = settings.settingsFile("watch_test");
    QVERIFY(sf);
    QCOMPARE(sf->find<int>("width"), 100);

    sf->setOverride("width", 300);
    QCOMPARE(sf->provenance("width"), QStringLiteral("override"));

    QVERIFY(QDir().mkpath(later));
    const QString saved = QDir(later).filePath("watch.toml");
    QCOMPARE(sf->saveOverridesTo(saved), QString{});

    QTRY_COMPARE(sf->provenance("width"), saved);
    QVERIFY(sf->overrides().isEmpty());
    QCOMPARE(sf->find<int>("width"), 300);

    // Stop watching the temporary folders before they are deleted.
    dsqt::Settings::forget("watch_test");
    settings.setSearchPaths(saved_search_paths);
}

void DsSettingsTest::tomlRemove_shouldDropOnlyTheKeyLines()
{
    const QString before = QStringLiteral(
        "# About ripple\r\n"
        "[ripple]\r\n"
        "# How fast\r\n"
        "msPerTile = 100 # ms\r\n"
        "jitterMs = 0\r\n"
        "\r\n"
        "[menu]\r\n"
        "rightClick = 'ripple'\r\n");
    const auto result = dsqt::settings::removeFromToml(
        before, {"ripple.msPerTile", "menu.rightClick", "not.there"});
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.text, QStringLiteral(
        "# About ripple\r\n"
        "[ripple]\r\n"
        "# How fast\r\n"
        "jitterMs = 0\r\n"
        "\r\n"
        "[menu]\r\n"));
    QCOMPARE(result.changed, (QStringList{"ripple.msPerTile", "menu.rightClick"}));
}

void DsSettingsTest::tomlRemove_inlineTableMembers()
{
    const QString before = QStringLiteral(
        "a = { x = 1, y = 2, z = 3 }\n"
        "b = {x=1, y=2}\n"
        "c = { only = 1 }\n");
    const auto result = dsqt::settings::removeFromToml(before, {"a.y", "b.y", "c.only"});
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.text, QStringLiteral(
        "a = { x = 1, z = 3 }\n"
        "b = {x=1}\n"
        "c = {}\n"));

    // Removing the first member keeps the spacing after the brace.
    QCOMPARE(dsqt::settings::removeFromToml("p = { x = 1, y = 2 }\n", {"p.x"}).text,
             QStringLiteral("p = { y = 2 }\n"));
}

void DsSettingsTest::tomlRemove_tableValueKeepsItsHeader()
{
    // A point held as a table: its keys go, the header (and its comment) stays.
    const QString before = QStringLiteral(
        "# Where it starts\n"
        "[pos]\n"
        "x = 1\n"
        "y = 2\n"
        "[other]\n"
        "dotted.a = 1\n"
        "dotted.b = 2\n");
    const auto result = dsqt::settings::removeFromToml(before, {"pos", "other.dotted.a"});
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.text, QStringLiteral(
        "# Where it starts\n"
        "[pos]\n"
        "[other]\n"
        "dotted.b = 2\n"));
}

void DsSettingsTest::tomlRemove_arrayOfTablesShouldBeRefused()
{
    const QString before = QStringLiteral("[[slide]]\nname = 'a'\n");
    const auto result = dsqt::settings::removeFromToml(before, {"slide.name"});
    QVERIFY(result.error.contains(QStringLiteral("array of tables")));
    QCOMPARE(result.text, before);
}

void DsSettingsTest::tomlKeysSet_shouldListTheKeysWithTheirValues()
{
    const QString text = QStringLiteral(
        "[ripple]\n"
        "msPerTile = 100   # ms\n"
        "list = [\n"
        "  1,\n"
        "  2,\n"
        "]\n"
        "[pos]\n"
        "x = 1\n"
        "[[slide]]\n"
        "name = 'a'\n");
    const auto found = dsqt::settings::tomlKeysSet(
        text, {"ripple.msPerTile", "ripple.list", "pos", "ripple.missing", "slide.name"});
    QCOMPARE(found.size(), 3);
    QCOMPARE(found[0], (std::pair<QString, QString>{"ripple.msPerTile", "100"}));
    QCOMPARE(found[1], (std::pair<QString, QString>{"ripple.list", "[ 1, 2, ]"}));
    QCOMPARE(found[2].first, QStringLiteral("pos"));
}

void DsSettingsTest::removeKeysFromFile_shouldEditInPlace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath("local.toml");
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Local\r\n[a]\r\nx = 1\r\ny = 2\r\n");
    }
    QStringList removed;
    QCOMPARE(dsqt::settings::removeKeysFromFile(path, {"a.x", "a.z"}, &removed), QString{});
    QCOMPARE(removed, QStringList{"a.x"});
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("# Local\r\n[a]\r\ny = 2\r\n"));

    // A file that does not exist is left alone.
    QCOMPARE(dsqt::settings::removeKeysFromFile(QDir(dir.path()).filePath("none.toml"), {"a.x"}),
             QString{});
    QVERIFY(!QFile::exists(QDir(dir.path()).filePath("none.toml")));
}

//*****************
//Bindings
//*****************

// Bind context for the member-function overload. No Q_OBJECT: bind() only needs a
// QObject identity to parent its SettingsBinding to, and a lifetime to track.
class BindTarget : public QObject
{
  public:
    using QObject::QObject;

    void onValue(const QString &value)
    {
        ++calls;
        last = value;
    }

    int     calls = 0;
    QString last;
};

void DsSettingsTest::bind_shouldCallTheCallbackImmediately()
{
    QObject context;
    int     calls = 0;
    QString seen;

    test_settings->bind<QString>("no_table", &context, [&](const QString &value) {
        ++calls;
        seen = value;
    });

    QCOMPARE(calls, 1);
    QCOMPARE(seen, QStringLiteral("test value"));
}

void DsSettingsTest::bind_shouldCallTheCallbackWhenTheValueChanges()
{
    QObject context;
    int     calls = 0;
    QString seen;

    test_settings->bind<QString>("no_table", &context, [&](const QString &value) {
        ++calls;
        seen = value;
    });
    QCOMPARE(calls, 1);

    test_settings->setOverride("no_table", QStringLiteral("changed"));

    QCOMPARE(calls, 2);
    QCOMPARE(seen, QStringLiteral("changed"));

    // Removing the override restores the file value, which is another change.
    test_settings->resetOverride("no_table");

    QCOMPARE(calls, 3);
    QCOMPARE(seen, QStringLiteral("test value"));
}

void DsSettingsTest::bind_shouldCallTheCallbackForANestedKey()
{
    QObject context;
    int     calls = 0;
    int     seen  = 0;

    // Nested keys bind to the child QQmlPropertyMap, not to the file itself.
    test_settings->bind<int>("test.int.int_positive", &context, [&](int value) {
        ++calls;
        seen = value;
    });

    QCOMPARE(calls, 1);
    QCOMPARE(seen, 1024);

    test_settings->setOverride("test.int.int_positive", 2048);

    QCOMPARE(calls, 2);
    QCOMPARE(seen, 2048);
}

void DsSettingsTest::bind_shouldUseTheDefaultForAnUnknownKey()
{
    QObject context;
    int     calls = 0;
    QString seen;

    test_settings->bind<QString>(
        "no_such_key", &context,
        [&](const QString &value) {
            ++calls;
            seen = value;
        },
        QStringLiteral("fallback"));

    QCOMPARE(calls, 1);
    QCOMPARE(seen, QStringLiteral("fallback"));

    // bind() creates the key path, so the binding is live before the key has a value.
    test_settings->setOverride("no_such_key", QStringLiteral("now set"));

    QCOMPARE(calls, 2);
    QCOMPARE(seen, QStringLiteral("now set"));
}

void DsSettingsTest::bind_shouldCallAMemberFunction()
{
    BindTarget target;

    test_settings->bind<QString>("no_table", &target, &BindTarget::onValue);

    QCOMPARE(target.calls, 1);
    QCOMPARE(target.last, QStringLiteral("test value"));

    test_settings->setOverride("no_table", QStringLiteral("changed"));

    QCOMPARE(target.calls, 2);
    QCOMPARE(target.last, QStringLiteral("changed"));
}

void DsSettingsTest::bind_shouldReplaceTheCallbackWhenReboundOnTheSameKeyAndContext()
{
    QObject context;
    int     first  = 0;
    int     second = 0;

    test_settings->bind<QString>("no_table", &context, [&](const QString &) { ++first; });
    test_settings->bind<QString>("no_table", &context, [&](const QString &) { ++second; });

    // Each bind() call invokes its own callback once, immediately.
    QCOMPARE(first, 1);
    QCOMPARE(second, 1);

    test_settings->setOverride("no_table", QStringLiteral("changed"));

    // Only the most recent callback is still installed.
    QCOMPARE(first, 1);
    QCOMPARE(second, 2);
}

void DsSettingsTest::bind_shouldStopWhenTheContextIsDestroyed()
{
    int   calls   = 0;
    auto *context = new QObject;

    test_settings->bind<QString>("no_table", context, [&](const QString &) { ++calls; });
    QCOMPARE(calls, 1);

    delete context;

    test_settings->setOverride("no_table", QStringLiteral("changed"));

    QCOMPARE(calls, 1);
}

void DsSettingsTest::registryBind_shouldResolveImmediatelyWhenTheFileIsRegistered()
{
    auto &settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});
    dsqt::Settings::add("test_settings");

    QObject context;
    int     calls = 0;
    QString seen;

    dsqt::Settings::bind<QString>(
        "test_settings", "no_table", &context,
        [&](const QString &value) {
            ++calls;
            seen = value;
        },
        QStringLiteral("fallback"));

    QCOMPARE(calls, 1);
    QCOMPARE(seen, QStringLiteral("test value"));
}

void DsSettingsTest::registryBind_shouldDeferUntilTheFileIsAdded()
{
    auto &settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});
    QVERIFY(!settings.hasSettingsFile("test_settings"));

    QObject context;
    int     calls = 0;
    QString seen;

    QTest::ignoreMessage(QtWarningMsg,
                         "Settings::bind: no settings file registered for 'test_settings' - "
                         "binding 'no_table' is deferred until one is added.");
    dsqt::Settings::bind<QString>(
        "test_settings", "no_table", &context,
        [&](const QString &value) {
            ++calls;
            seen = value;
        },
        QStringLiteral("fallback"));

    // The immediate call still happens, with the supplied default.
    QCOMPARE(calls, 1);
    QCOMPARE(seen, QStringLiteral("fallback"));

    dsqt::Settings::add("test_settings");

    // Replayed against the file as soon as it is registered.
    QCOMPARE(calls, 2);
    QCOMPARE(seen, QStringLiteral("test value"));
}

void DsSettingsTest::registryBind_deferred_shouldKeepTrackingAfterItResolves()
{
    auto &settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});

    QObject context;
    int     calls = 0;
    QString seen;

    QTest::ignoreMessage(QtWarningMsg,
                         "Settings::bind: no settings file registered for 'test_settings' - "
                         "binding 'no_table' is deferred until one is added.");
    dsqt::Settings::bind<QString>(
        "test_settings", "no_table", &context,
        [&](const QString &value) {
            ++calls;
            seen = value;
        },
        QStringLiteral("fallback"));

    dsqt::Settings::add("test_settings");
    QCOMPARE(calls, 2);

    // A replayed bind is an ordinary bind: later changes still reach it.
    settings.settingsFile("test_settings")->setOverride("no_table", QStringLiteral("changed"));

    QCOMPARE(calls, 3);
    QCOMPARE(seen, QStringLiteral("changed"));
}

void DsSettingsTest::registryBind_deferred_shouldReplayInCallOrder()
{
    auto &settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});

    QObject context;
    int     first  = 0;
    int     second = 0;

    QTest::ignoreMessage(QtWarningMsg,
                         "Settings::bind: no settings file registered for 'test_settings' - "
                         "binding 'no_table' is deferred until one is added.");
    dsqt::Settings::bind<QString>("test_settings", "no_table", &context,
                                  [&](const QString &) { ++first; }, QStringLiteral("fallback"));

    QTest::ignoreMessage(QtWarningMsg,
                         "Settings::bind: no settings file registered for 'test_settings' - "
                         "binding 'no_table' is deferred until one is added.");
    dsqt::Settings::bind<QString>("test_settings", "no_table", &context,
                                  [&](const QString &) { ++second; }, QStringLiteral("fallback"));

    QCOMPARE(first, 1);
    QCOMPARE(second, 1);

    dsqt::Settings::add("test_settings");

    // Both are replayed, each with its own immediate call.
    QCOMPARE(first, 2);
    QCOMPARE(second, 2);

    settings.settingsFile("test_settings")->setOverride("no_table", QStringLiteral("changed"));

    // Replaying in call order means the later bind is the one left installed,
    // matching what would have happened had the file been registered all along.
    QCOMPARE(first, 2);
    QCOMPARE(second, 3);
}

void DsSettingsTest::registryBind_deferred_shouldBeDroppedWhenTheContextDies()
{
    auto &settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});

    int   calls   = 0;
    auto *context = new QObject;

    QTest::ignoreMessage(QtWarningMsg,
                         "Settings::bind: no settings file registered for 'test_settings' - "
                         "binding 'no_table' is deferred until one is added.");
    dsqt::Settings::bind<QString>("test_settings", "no_table", context,
                                  [&](const QString &) { ++calls; }, QStringLiteral("fallback"));
    QCOMPARE(calls, 1);

    delete context;

    // The parked bind must be skipped rather than dereferenced.
    dsqt::Settings::add("test_settings");

    QCOMPARE(calls, 1);
}

void DsSettingsTest::registryBind_deferred_shouldNotResolveOnAnUnrelatedFile()
{
    auto &settings = dsqt::Settings::instance();
    settings.setSearchPaths({settingsDir()});

    QObject context;
    int     calls = 0;

    QTest::ignoreMessage(QtWarningMsg,
                         "Settings::bind: no settings file registered for 'test_settings' - "
                         "binding 'no_table' is deferred until one is added.");
    dsqt::Settings::bind<QString>("test_settings", "no_table", &context,
                                  [&](const QString &) { ++calls; }, QStringLiteral("fallback"));
    QCOMPARE(calls, 1);

    // Registering a different file leaves the bind parked.
    dsqt::Settings::add("engine");
    QCOMPARE(calls, 1);

    dsqt::Settings::add("test_settings");
    QCOMPARE(calls, 2);
}

// ── SettingsTreeModel ────────────────────────────────────────────────────────

namespace {

// Depth-first search for the value-column index of the item at `path`.
QModelIndex findValueIndex(const QAbstractItemModel &model, const QString &path,
                           const QModelIndex &parent = {})
{
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex idx = model.index(row, 0, parent);
        if (idx.data(dsqt::SettingsTreeModel::FullPathRole).toString() == path)
            return idx.siblingAtColumn(1);
        if (const QModelIndex found = findValueIndex(model, path, idx); found.isValid())
            return found;
    }
    return {};
}

// A SettingsFile backed by a TOML file written into a temporary directory.
struct TempSettingsFile
{
    QTemporaryDir dir;
    std::unique_ptr<dsqt::SettingsFile> file;

    explicit TempSettingsFile(const QByteArray &toml, const QByteArray &schema = {})
    {
        write("viewer.toml", toml);
        if (!schema.isEmpty())
            write("viewer.schema.toml", schema);
        file = std::make_unique<dsqt::SettingsFile>(nullptr, QStringList{dir.path()});
        file->setFileName("viewer.toml");
    }

    void write(const QString &name, const QByteArray &content)
    {
        QFile out(dir.filePath(name));
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            out.write(content);
    }
};

const QByteArray kViewerToml = R"(
points = [1, 2, 3]

[window]
width = 1920
scale = 0.5
title = "hello"

[render]
mode = "fast"
quality = 2

[range]
min = 1
)";

const QByteArray kViewerSchema = R"(
[window.width]
min = 640
max = 3840
step = 10
description = "Width of the window"

[window.scale]
min = 0.0
max = 1.0

[render.mode]
options = ["red", "fast"]
labels = ["Red", "Fast"]

[render.quality]
options = [1, 2, 4]

[range.min]          # a setting that happens to be called "min"
min = 0
max = 5

[points]
min = 0
max = 10
)";

} // namespace

void DsSettingsTest::treeModel_parseDisplayString_shouldRoundTrip_data()
{
    QTest::addColumn<QVariant>("value");

    QTest::newRow("point")      << QVariant(QPointF(10, -20.5));
    QTest::newRow("point-prec") << QVariant(QPointF(1234567.125, 0.1));
    QTest::newRow("size")       << QVariant(QSizeF(1920, 1080));
    QTest::newRow("rect")       << QVariant(QRectF(1.5, 2, 300, 400));
    QTest::newRow("vector2d")   << QVariant(QVector2D(0.1f, -2.f));
    QTest::newRow("vector3d")   << QVariant(QVector3D(0.1f, 0.2f, 0.3f));
    QTest::newRow("vector4d")   << QVariant(QVector4D(1.f, 2.f, 3.f, 0.25f));
    QTest::newRow("quaternion") << QVariant(QQuaternion(0.5f, 1.f, 2.f, 3.f));
    QTest::newRow("color")      << QVariant(QColor(0x80, 0x10, 0x20, 0x40));
    QTest::newRow("bool")       << QVariant(true);
    QTest::newRow("int64")      << QVariant(qlonglong(-9000000000));
    QTest::newRow("double")     << QVariant(0.1);
    QTest::newRow("string")     << QVariant(QStringLiteral(" padded (1, 2) "));
    QTest::newRow("date")       << QVariant(QDate(2026, 11, 17));
    QTest::newRow("time")       << QVariant(QTime(13, 45, 10));
}

void DsSettingsTest::treeModel_parseDisplayString_shouldRoundTrip()
{
    QFETCH(QVariant, value);
    const QString  text   = dsqt::SettingsTreeModel::displayString(value);
    const QVariant parsed = dsqt::SettingsTreeModel::parseDisplayString(text, value.metaType());
    QVERIFY2(parsed.isValid(), qPrintable("could not parse " + text));
    QCOMPARE(parsed.metaType(), value.metaType());
    QCOMPARE(parsed, value);
}

void DsSettingsTest::treeModel_parseDisplayString_shouldAcceptLooseSeparators()
{
    using dsqt::SettingsTreeModel;
    const QMetaType point = QMetaType::fromType<QPointF>();
    const QMetaType size  = QMetaType::fromType<QSizeF>();
    QCOMPARE(SettingsTreeModel::parseDisplayString("(3, 4)", point), QVariant(QPointF(3, 4)));
    QCOMPARE(SettingsTreeModel::parseDisplayString("3,4", point), QVariant(QPointF(3, 4)));
    QCOMPARE(SettingsTreeModel::parseDisplayString("  3 4  ", point), QVariant(QPointF(3, 4)));
    QCOMPARE(SettingsTreeModel::parseDisplayString("[-1e2; .5]", point), QVariant(QPointF(-100, 0.5)));
    QCOMPARE(SettingsTreeModel::parseDisplayString("800x600", size), QVariant(QSizeF(800, 600)));
    QCOMPARE(SettingsTreeModel::parseDisplayString(QStringLiteral(u"800 × 600"), size),
             QVariant(QSizeF(800, 600)));
}

void DsSettingsTest::treeModel_parseDisplayString_shouldRejectMismatchedText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QMetaType>("type");

    const QMetaType point = QMetaType::fromType<QPointF>();
    QTest::newRow("point-empty")    << QString()                    << point;
    QTest::newRow("point-word")     << QStringLiteral("abc")        << point;
    QTest::newRow("point-one")      << QStringLiteral("1")          << point;
    QTest::newRow("point-three")    << QStringLiteral("1, 2, 3")    << point;
    QTest::newRow("point-trailing") << QStringLiteral("1, 2 px")    << point;
    QTest::newRow("rect-two")       << QStringLiteral("(1, 2)")     << QMetaType::fromType<QRectF>();
    QTest::newRow("bool-word")      << QStringLiteral("yes")        << QMetaType::fromType<bool>();
    QTest::newRow("int-fraction")   << QStringLiteral("1.5")        << QMetaType::fromType<qlonglong>();
    QTest::newRow("color-word")     << QStringLiteral("notacolor")  << QMetaType::fromType<QColor>();
    QTest::newRow("date-garbage")   << QStringLiteral("17/11/2026") << QMetaType::fromType<QDate>();
}

void DsSettingsTest::treeModel_parseDisplayString_shouldRejectMismatchedText()
{
    QFETCH(QString, text);
    QFETCH(QMetaType, type);
    QVERIFY(!dsqt::SettingsTreeModel::parseDisplayString(text, type).isValid());
}

void DsSettingsTest::treeModel_setData_shouldKeepTheOriginalType_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QVariant>("expected");

    QTest::newRow("point")  << "window.pos"      << "(3, 4.5)"       << QVariant(QPointF(3, 4.5));
    QTest::newRow("size")   << "window.size"     << "1280 x 720"     << QVariant(QSizeF(1280, 720));
    QTest::newRow("rect")   << "window.rect"     << "(0, 0, 10, 20)" << QVariant(QRectF(0, 0, 10, 20));
    QTest::newRow("vec3")   << "window.axis"     << "(0, 1, 0)"      << QVariant(QVector3D(0, 1, 0));
    QTest::newRow("int")    << "window.count"    << "7"              << QVariant(qlonglong(7));
    QTest::newRow("string") << "window.title"    << "(1, 2)"         << QVariant(QStringLiteral("(1, 2)"));
    QTest::newRow("listed") << "window.points.1" << "(5, 6)"         << QVariant(QPointF(5, 6));
}

void DsSettingsTest::treeModel_setData_shouldKeepTheOriginalType()
{
    QFETCH(QString, path);
    QFETCH(QString, text);
    QFETCH(QVariant, expected);

    TempSettingsFile settings(R"(
[window]
pos    = { x = 1, y = 2 }
size   = { w = 640, h = 480 }
rect   = { x = 1, y = 2, w = 3, h = 4 }
axis   = { x = 1, y = 0, z = 0 }
count  = 3
title  = "hello"
points = [ { x = 1, y = 1 }, { x = 2, y = 2 } ]
)");

    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const QModelIndex idx = findValueIndex(model, path);
    QVERIFY2(idx.isValid(), qPrintable("no tree item for " + path));
    QVERIFY(model.flags(idx) & Qt::ItemIsEditable);

    // The delegate's line edit always hands the model a QString.
    QVERIFY(model.setData(idx, text, Qt::EditRole));

    // SettingsFile::value() does not index into lists, so read list elements
    // ("window.points.1") through their parent list.
    QVariant stored;
    const qsizetype dot = path.lastIndexOf(u'.');
    bool isElement = false;
    const int element = path.mid(dot + 1).toInt(&isElement);
    if (isElement)
        stored = settings.file->value(path.left(dot)).toList().value(element);
    else
        stored = settings.file->value(path);
    QCOMPARE(stored.metaType(), expected.metaType());
    QCOMPARE(stored, expected);
    QCOMPARE(findValueIndex(model, path).data(Qt::EditRole), expected);
}

void DsSettingsTest::treeModel_setData_shouldRejectTextThatDoesNotFit()
{
    TempSettingsFile settings("pos = { x = 1, y = 2 }\n");

    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const QModelIndex idx = findValueIndex(model, "pos");
    QVERIFY(idx.isValid());
    QVERIFY(!model.setData(idx, QStringLiteral("not a point"), Qt::EditRole));

    QCOMPARE(settings.file->value("pos"), QVariant(QPointF(1, 2)));
    QCOMPARE(settings.file->provenance("pos"), settings.file->resolvedFilePaths().value(0));
}

// ── Schema files ─────────────────────────────────────────────────────────────

void DsSettingsTest::schema_shouldLoadEntriesFromTheSchemaFile()
{
    TempSettingsFile settings(kViewerToml, kViewerSchema);
    dsqt::SettingsFile &file = *settings.file;

    QCOMPARE(file.resolvedSchemaPaths(), QStringList{settings.dir.filePath("viewer.schema.toml")});

    const QVariantMap width = file.schema("window.width");
    QCOMPARE(width.value("min").toInt(), 640);
    QCOMPARE(width.value("max").toInt(), 3840);
    QCOMPARE(width.value("step").toInt(), 10);
    QCOMPARE(width.value("description").toString(), QStringLiteral("Width of the window"));

    // Read literally: "red" stays a string, it does not become a QColor.
    const QVariantList options = file.schema("render.mode").value("options").toList();
    QCOMPARE(options.size(), 2);
    QCOMPARE(options.at(0).metaType(), QMetaType::fromType<QString>());
    QCOMPARE(options.at(0).toString(), QStringLiteral("red"));

    // A table holding a *setting* named "min" is not an entry for its parent.
    QVERIFY(file.schema("range").isEmpty());
    QCOMPARE(file.schema("range.min").value("max").toInt(), 5);

    // List elements fall back to their list's entry.
    QCOMPARE(file.schema("points.1"), file.schema("points"));
    QVERIFY(!file.schema("points").isEmpty());

    QVERIFY(file.schema("window").isEmpty());
    QVERIFY(file.schema("does.not.exist").isEmpty());
}

void DsSettingsTest::schema_laterFilesShouldOverrideEarlierOnesKeyByKey()
{
    QTemporaryDir base, app;
    const auto write = [](const QTemporaryDir &dir, const QString &name, const QByteArray &text) {
        QFile out(dir.filePath(name));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(text);
    };
    write(base, "viewer.toml", "value = 5\n");
    write(base, "viewer.schema.toml", "[value]\nmin = 0\nmax = 10\nstep = 1\n");
    write(app, "viewer.toml", "value = 6\n");
    write(app, "viewer.schema.toml", "[value]\nmax = 20\n");

    dsqt::SettingsFile file(nullptr, QStringList{base.path(), app.path()});
    file.setFileName("viewer.toml");

    const QVariantMap entry = file.schema("value");
    QCOMPARE(entry.value("min").toInt(), 0);
    QCOMPARE(entry.value("max").toInt(), 20);
    QCOMPARE(entry.value("step").toInt(), 1);
}

void DsSettingsTest::schema_shouldBeReadAgainOnReload()
{
    TempSettingsFile settings(kViewerToml, "[window.width]\nmin = 1\nmax = 2\n");
    QCOMPARE(settings.file->schema("window.width").value("max").toInt(), 2);

    settings.write("viewer.schema.toml", "[window.width]\nmin = 1\nmax = 3\n");
    settings.file->reload();
    QCOMPARE(settings.file->schema("window.width").value("max").toInt(), 3);
}

void DsSettingsTest::schemaEntry_slider_shouldSnapToTheStepGrid()
{
    using Entry = dsqt::SettingsSchemaEntry;

    // Integer setting: a fractional step is rounded to a whole one.
    const Entry ints = Entry::fromMap({{"min", 0}, {"max", 100}, {"step", 2.6}},
                                      QMetaType::fromType<qlonglong>());
    QCOMPARE(ints.kind, Entry::Kind::Slider);
    QVERIFY(ints.integral);
    QCOMPARE(ints.step, 3.0);
    QCOMPARE(ints.snapped(7.0), QVariant(qlonglong(6)));
    QCOMPARE(ints.snapped(-50.0), QVariant(qlonglong(0)));
    QCOMPARE(ints.format(42.0), QStringLiteral("42"));

    // Real setting without a step: a hundredth of the range, two decimals.
    const Entry reals = Entry::fromMap({{"min", 0.0}, {"max", 1.0}}, QMetaType::fromType<double>());
    QCOMPARE(reals.kind, Entry::Kind::Slider);
    QCOMPARE(reals.decimals, 2);
    QCOMPARE(reals.snapped(0.333), QVariant(0.33));
    QCOMPARE(reals.snapped(0.1 * 3), QVariant(0.3)); // no 0.30000000000000004
    QCOMPARE(reals.format(0.5), QStringLiteral("0.50"));
    QCOMPARE(reals.ratio(0.25), 0.25);
    QCOMPARE(reals.ratio(7.0), 1.0);

    // Hand-typed values are clamped, not snapped.
    QCOMPARE(reals.clamped(0.337), QVariant(0.337));
    QCOMPARE(reals.clamped(3.0), QVariant(1.0));
}

void DsSettingsTest::schemaEntry_slider_shouldKeepAnOffGridMaxReachable()
{
    using Entry = dsqt::SettingsSchemaEntry;
    const Entry entry = Entry::fromMap({{"min", 0}, {"max", 10}, {"step", 3}},
                                       QMetaType::fromType<qlonglong>());
    QCOMPARE(entry.snapped(10.0), QVariant(qlonglong(10)));
    QCOMPARE(entry.snapped(9.4), QVariant(qlonglong(9)));
    QCOMPARE(entry.snapped(9.6), QVariant(qlonglong(10)));
}

void DsSettingsTest::schemaEntry_options_shouldConvertToTheSettingType()
{
    using Entry = dsqt::SettingsSchemaEntry;

    // Strings and numbers both work for a number setting; what does not fit is dropped.
    const Entry ints = Entry::fromMap({{"options", QVariantList{"1", qlonglong(2), 2.5, "x"}}},
                                      QMetaType::fromType<qlonglong>());
    QCOMPARE(ints.kind, Entry::Kind::Options);
    QCOMPARE(ints.options, (QVariantList{qlonglong(1), qlonglong(2)}));
    QCOMPARE(ints.labels, (QStringList{"1", "2"}));
    QCOMPARE(ints.optionIndex(QVariant(qlonglong(2))), 1);
    QCOMPARE(ints.optionIndex(QVariant(qlonglong(3))), -1);

    // Labels pair up with options by position.
    const Entry strings = Entry::fromMap({{"options", QVariantList{"red", "fast"}},
                                          {"labels", QVariantList{"Red", "Fast"}}},
                                         QMetaType::fromType<QString>());
    QCOMPARE(strings.options, (QVariantList{QStringLiteral("red"), QStringLiteral("fast")}));
    QCOMPARE(strings.labels, (QStringList{"Red", "Fast"}));

    // Options of a colour setting are parsed as colours.
    const Entry colours = Entry::fromMap({{"options", QVariantList{"#ff0000", "nonsense"}}},
                                         QMetaType::fromType<QColor>());
    QCOMPARE(colours.options, (QVariantList{QVariant::fromValue(QColor(Qt::red))}));
}

void DsSettingsTest::schemaEntry_shouldIgnoreWhatDoesNotFitTheType()
{
    using Entry = dsqt::SettingsSchemaEntry;
    const QVariantMap range{{"min", 0}, {"max", 10}};

    QCOMPARE(Entry::fromMap(range, QMetaType::fromType<QString>()).kind, Entry::Kind::None);
    QCOMPARE(Entry::fromMap(range, QMetaType::fromType<QPointF>()).kind, Entry::Kind::None);
    QCOMPARE(Entry::fromMap({{"min", 5}, {"max", 5}}, QMetaType::fromType<double>()).kind,
             Entry::Kind::None);
    QCOMPARE(Entry::fromMap({{"min", 0}}, QMetaType::fromType<double>()).kind, Entry::Kind::None);

    // Options take precedence; if none of them fit, there is no fallback to a slider.
    QCOMPARE(Entry::fromMap({{"min", 0}, {"max", 10}, {"options", QVariantList{"x"}}},
                            QMetaType::fromType<qlonglong>())
                 .kind,
             Entry::Kind::None);

    // A description alone still reaches the tooltip.
    const Entry described = Entry::fromMap({{"description", "Hi"}}, QMetaType::fromType<QString>());
    QCOMPARE(described.kind, Entry::Kind::None);
    QCOMPARE(described.description, QStringLiteral("Hi"));
}

void DsSettingsTest::treeModel_schema_shouldDriveDisplayTooltipAndClamping()
{
    using Entry = dsqt::SettingsSchemaEntry;
    TempSettingsFile settings(kViewerToml, kViewerSchema);
    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const QModelIndex width = findValueIndex(model, "window.width");
    const auto widthEntry = width.data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>();
    QCOMPARE(widthEntry.kind, Entry::Kind::Slider);
    QCOMPARE(widthEntry.max, 3840.0);
    QVERIFY(width.data(Qt::ToolTipRole).toString().contains("<qt>Width of the window<br>"));
    QCOMPARE(width.data(dsqt::SettingsTreeModel::SchemaRole).toMap().value("step").toInt(), 10);

    // An option with a label is displayed by its label; the value is untouched.
    const QModelIndex mode = findValueIndex(model, "render.mode");
    QCOMPARE(mode.data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>().kind,
             Entry::Kind::Options);
    QCOMPARE(mode.data(Qt::DisplayRole).toString(), QStringLiteral("Fast"));
    QCOMPARE(mode.data(Qt::EditRole), QVariant(QStringLiteral("fast")));

    // List elements use their list's entry.
    const QModelIndex point = findValueIndex(model, "points.1");
    QCOMPARE(point.data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>().kind,
             Entry::Kind::Slider);

    // Typed values outside a slider's range are clamped to it.
    QVERIFY(model.setData(width, QStringLiteral("99999"), Qt::EditRole));
    QCOMPARE(settings.file->value("window.width"), QVariant(qlonglong(3840)));
}

void DsSettingsTest::treeModel_valueChanges_shouldUpdateRowsInPlace()
{
    TempSettingsFile settings(kViewerToml, kViewerSchema);
    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);

    // Same keys, new value: rows are updated, the model is not reset, and a
    // persistent index (what a view's selection or a slider drag holds) survives.
    const QPersistentModelIndex width = findValueIndex(model, "window.width");
    settings.file->setOverride("window.width", qlonglong(1000));
    QCOMPARE(resets.count(), 0);
    QVERIFY(width.isValid());
    QCOMPARE(width.data(Qt::EditRole), QVariant(qlonglong(1000)));
    QVERIFY(std::any_of(changes.cbegin(), changes.cend(), [&](const QList<QVariant> &args) {
        const auto topLeft = args.at(0).value<QModelIndex>();
        const auto bottomRight = args.at(1).value<QModelIndex>();
        return topLeft.parent() == width.parent() && topLeft.row() <= width.row()
               && bottomRight.row() >= width.row() && bottomRight.column() >= width.column();
    }));

    // A new key changes the tree's shape: that still resets.
    settings.file->setOverride("window.height", qlonglong(1080));
    QCOMPARE(resets.count(), 1);
}

// ── Paths ────────────────────────────────────────────────────────────────────

void DsSettingsTest::environment_contractPath_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("expected");

    QTest::newRow("local wins over documents")
        << "C:/Users/me/Documents/downstream/cache/a.png" << "%LOCAL%/cache/a.png";
    QTest::newRow("documents")        << "C:/Users/me/Documents/notes.txt" << "%DOCUMENTS%/notes.txt";
    QTest::newRow("root itself")      << "C:/Users/me/Documents/downstream" << "%LOCAL%";
    QTest::newRow("trailing slash")   << "C:/Users/me/Documents/downstream/cache/" << "%LOCAL%/cache/";
    QTest::newRow("app")              << "D:/Apps/RippleWall/data/movies" << "%APP%/data/movies";
    QTest::newRow("folder boundary")  << "C:/Users/me/Documents2/x" << "C:/Users/me/Documents2/x";
    QTest::newRow("outside")          << "E:/media/clip.mp4" << "E:/media/clip.mp4";
    QTest::newRow("backslashes")      << R"(D:\Apps\RippleWall\settings\app.toml)" << "%APP%/settings/app.toml";
    QTest::newRow("only at the start") << "E:/backup/D:/Apps/RippleWall" << "E:/backup/D:/Apps/RippleWall";
    QTest::newRow("cleaned")          << "D:/Apps/RippleWall/./data/../data/x" << "%APP%/data/x";
#ifdef Q_OS_WIN
    QTest::newRow("case on windows")  << "c:/users/ME/documents/downstream/x" << "%LOCAL%/x";
#endif
}

void DsSettingsTest::environment_contractPath()
{
    QFETCH(QString, path);
    QFETCH(QString, expected);

    const QList<dsqt::DsEnvironment::PathVariable> variables{
        {"%DOCUMENTS%", "C:/Users/me/Documents"},
        {"%LOCAL%", "C:/Users/me/Documents/downstream"},
        {"%APP%", R"(D:\Apps\RippleWall)"},
        {"%SHARED%", ""},       // unknown folders are ignored
        {"%RES%", "%RES%"},     // as are unresolved ones
    };
    QCOMPARE(dsqt::DsEnvironment::contractPath(path, variables), expected);
}

void DsSettingsTest::schemaEntry_editor_shouldSelectFileOrFolderForStrings()
{
    using Entry = dsqt::SettingsSchemaEntry;
    const QMetaType string = QMetaType::fromType<QString>();

    const Entry file = Entry::fromMap({{"editor", "file"}, {"filter", "Images (*.png)"}}, string);
    QCOMPARE(file.kind, Entry::Kind::File);
    QCOMPARE(file.fileFilter, QStringLiteral("Images (*.png)"));
    QCOMPARE(Entry::fromMap({{"editor", "Folder"}}, string).kind, Entry::Kind::Folder);

    // "text" switches path detection off; file/folder on a non-string is ignored.
    // A legacy-metadata style "type" key is not read at all.
    QCOMPARE(Entry::fromMap({{"type", "file"}}, string).kind, Entry::Kind::None);
    const Entry text = Entry::fromMap({{"editor", "text"}}, string);
    QCOMPARE(text.kind, Entry::Kind::None);
    QVERIFY(!text.detectPaths);
    QCOMPARE(Entry::fromMap({{"editor", "file"}}, QMetaType::fromType<qlonglong>()).kind,
             Entry::Kind::None);
}

void DsSettingsTest::schemaEntry_detectPath_data()
{
    using Kind = dsqt::SettingsSchemaEntry::Kind;
    QTest::addColumn<QString>("value");
    QTest::addColumn<Kind>("expected");

    QTest::newRow("variable folder")    << "%LOCAL%/no-such-cache/" << Kind::Folder;
    QTest::newRow("variable no ext")    << "%APP%/no-such-data" << Kind::Folder;
    QTest::newRow("variable file")      << "%APP%/no-such-image.png" << Kind::File;
    QTest::newRow("variable any case")  << "%local%/x.txt" << Kind::File;
    QTest::newRow("drive")              << "Q:/does/not/exist.mp4" << Kind::File;
    QTest::newRow("drive backslash")    << R"(Q:\does\not\exist)" << Kind::Folder;
    QTest::newRow("unc")                << R"(\\server\share\clip.mp4)" << Kind::File;
    QTest::newRow("missing slash root") << "/api/v1" << Kind::None;
    QTest::newRow("relative")           << "data/movies" << Kind::None;
    QTest::newRow("word")               << "ripple" << Kind::None;
    QTest::newRow("file url variable")  << "file:///%APP%/no-such-sound.wav" << Kind::File;
    QTest::newRow("file url drive")     << "file:///Q:/does/not/exist/" << Kind::Folder;
    QTest::newRow("file url any case")  << "FILE:///Q:/clip.mp4" << Kind::File;
    QTest::newRow("url")                << "https://example.com/a.png" << Kind::None;
    QTest::newRow("qrc")                << "qrc:/qt/qml/App/Main.qml" << Kind::None;
    QTest::newRow("empty")              << "" << Kind::None;
}

void DsSettingsTest::schemaEntry_detectPath()
{
    using Kind = dsqt::SettingsSchemaEntry::Kind;
    QFETCH(QString, value);
    QFETCH(Kind, expected);
    QCOMPARE(dsqt::SettingsSchemaEntry::detectPath(value), expected);
}

void DsSettingsTest::treeModel_paths_shouldBeDetectedAndFlaggedWhenMissing()
{
    using Entry = dsqt::SettingsSchemaEntry;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    {
        QFile clip(media.filePath("clip"));  // a file without an extension
        QVERIFY(clip.open(QIODevice::WriteOnly));
    }

    const QByteArray toml = QStringLiteral(
        "folder = \"%1\"\n"
        "clip = \"%2\"\n"
        "missing = \"%1/gone.png\"\n"
        "plain = \"%1\"\n"
        "chosen = \"anything\"\n"
        "title = \"hello\"\n")
        .arg(media.path(), media.filePath("clip")).toUtf8();
    const QByteArray schema = "[plain]\neditor = \"text\"\n\n[chosen]\neditor = \"file\"\n";
    TempSettingsFile settings(toml, schema);
    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const auto entry = [&model](const QString &path) {
        return findValueIndex(model, path).data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>();
    };

    // Existing paths are classified by what is on disk, not by their extension.
    QCOMPARE(entry("folder").kind, Entry::Kind::Folder);
    QVERIFY(!entry("folder").pathMissing);
    QCOMPARE(entry("clip").kind, Entry::Kind::File);

    // Missing paths are still recognised, and flagged.
    QCOMPARE(entry("missing").kind, Entry::Kind::File);
    QVERIFY(entry("missing").pathMissing);
    QVERIFY(findValueIndex(model, "missing").data(Qt::ToolTipRole).toString().contains("Not found"));

    // The schema decides when it says so.
    QCOMPARE(entry("plain").kind, Entry::Kind::None);
    QCOMPARE(entry("chosen").kind, Entry::Kind::File);
    QVERIFY(entry("chosen").pathMissing);
    QCOMPARE(entry("title").kind, Entry::Kind::None);
}

void DsSettingsTest::schemaEntry_localPath_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("expected");

    const QString app = dsqt::DsEnvironment::expandq("%APP%/data/fire.wav");
    QTest::newRow("plain variable")    << "%APP%/data/fire.wav" << app;
    QTest::newRow("url variable")      << "file:///%APP%/data/fire.wav" << app;
    QTest::newRow("url two slashes")   << "file://%APP%/data/fire.wav" << app;
    QTest::newRow("url drive")         << "file:///C:/temp/fire.wav" << "C:/temp/fire.wav";
    QTest::newRow("url escapes")       << "file:///C:/temp/my%20clip.mp4" << "C:/temp/my clip.mp4";
    QTest::newRow("url spaces")        << "file:///C:/temp/my clip.mp4" << "C:/temp/my clip.mp4";
    QTest::newRow("url rooted")        << "file:///home/user/x.png" << "/home/user/x.png";
    QTest::newRow("url single slash")  << "file:/C:/temp/x.png" << "C:/temp/x.png";
    QTest::newRow("url host")          << "file://server/share/x.png" << "//server/share/x.png";
    QTest::newRow("cfg folder")        << "file:///%CFG_FOLDER%/x.png"
                                       << dsqt::DsEnvironment::expandq("%CFG_FOLDER%/x.png");
}

void DsSettingsTest::schemaEntry_localPath()
{
    QFETCH(QString, value);
    QFETCH(QString, expected);
    QCOMPARE(dsqt::SettingsSchemaEntry::localPath(value), expected);
}

void DsSettingsTest::treeModel_fileUrls_shouldBeCheckedAsPaths()
{
    using Entry = dsqt::SettingsSchemaEntry;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    {
        QFile clip(media.filePath("fire.wav"));
        QVERIFY(clip.open(QIODevice::WriteOnly));
    }

    const QByteArray toml = QStringLiteral(
        "found = \"file:///%1\"\n"
        "missing = \"file:///%2\"\n")
        .arg(media.filePath("fire.wav"), media.filePath("gone.wav")).toUtf8();
    TempSettingsFile settings(toml, "[found]\neditor = \"file\"\n");
    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const auto entry = [&model](const QString &path) {
        return findValueIndex(model, path).data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>();
    };
    QCOMPARE(entry("found").kind, Entry::Kind::File);
    QVERIFY(!entry("found").pathMissing);
    QCOMPARE(entry("missing").kind, Entry::Kind::File); // detected without a schema
    QVERIFY(entry("missing").pathMissing);
    const QString tip = findValueIndex(model, "missing").data(Qt::ToolTipRole).toString();
    QVERIFY2(tip.contains(QDir::toNativeSeparators(media.filePath("gone.wav")).toHtmlEscaped()),
             qPrintable(tip));
}

// ── Boolean switches ───────────────────────────────────────────────────────────

void DsSettingsTest::schemaEntry_switch_shouldBeTheDefaultForBooleans()
{
    using Entry = dsqt::SettingsSchemaEntry;
    const QMetaType boolean = QMetaType::fromType<bool>();

    QCOMPARE(Entry::fromMap({}, boolean).kind, Entry::Kind::Switch);
    QCOMPARE(Entry::fromMap({{"description", "On or off"}}, boolean).kind, Entry::Kind::Switch);
    QCOMPARE(Entry::fromMap({{"editor", "text"}}, boolean).kind, Entry::Kind::None);

    // Options still win, for Booleans with labels of their own.
    const Entry labelled = Entry::fromMap(
        {{"options", QVariantList{true, false}}, {"labels", QVariantList{"Shown", "Hidden"}}}, boolean);
    QCOMPARE(labelled.kind, Entry::Kind::Options);

    // Only Booleans.
    QCOMPARE(Entry::fromMap({}, QMetaType::fromType<QString>()).kind, Entry::Kind::None);
    QCOMPARE(Entry::fromMap({}, QMetaType::fromType<int>()).kind, Entry::Kind::None);
}

void DsSettingsTest::treeModel_switch_shouldFlipBooleans()
{
    using Entry = dsqt::SettingsSchemaEntry;
    TempSettingsFile settings("enabled = true\nflags = [false, true]\nplain = false\n",
                              "[plain]\neditor = \"text\"\n");
    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const auto kind = [&model](const QString &path) {
        return findValueIndex(model, path).data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>().kind;
    };
    QCOMPARE(kind("enabled"), Entry::Kind::Switch);
    QCOMPARE(kind("flags.0"), Entry::Kind::Switch); // list elements too
    QCOMPARE(kind("plain"), Entry::Kind::None);

    // The switch stores a bool, which stays a Boolean.
    QVERIFY(model.setData(findValueIndex(model, "enabled"), false, Qt::EditRole));
    QCOMPARE(settings.file->value("enabled").metaType(), QMetaType::fromType<bool>());
    QCOMPARE(settings.file->value("enabled").toBool(), false);
    QCOMPARE(findValueIndex(model, "enabled").data(Qt::DisplayRole).toString(), QStringLiteral("false"));

    QVERIFY(model.setData(findValueIndex(model, "flags.0"), true, Qt::EditRole));
    const QVariantList flags = settings.file->value("flags").toList();
    QCOMPARE(flags, (QVariantList{true, true}));
}

// ── Restart marker ───────────────────────────────────────────────────────────

void DsSettingsTest::schemaEntry_restart_shouldAcceptBoolsAndLegacyStrings()
{
    using Entry = dsqt::SettingsSchemaEntry;
    const QMetaType string = QMetaType::fromType<QString>();

    QVERIFY(Entry::fromMap({{"restart", true}}, string).restart);
    QVERIFY(Entry::fromMap({{"restart", "full"}}, string).restart); // as in old metadata
    QVERIFY(!Entry::fromMap({{"restart", false}}, string).restart);
    QVERIFY(!Entry::fromMap({{"restart", "none"}}, string).restart);
    QVERIFY(!Entry::fromMap({{"restart", ""}}, string).restart);
    QVERIFY(!Entry::fromMap({}, string).restart);

    // It combines with any editor.
    const Entry slider = Entry::fromMap({{"min", 0}, {"max", 10}, {"restart", true}},
                                        QMetaType::fromType<qlonglong>());
    QCOMPARE(slider.kind, Entry::Kind::Slider);
    QVERIFY(slider.restart);
}

void DsSettingsTest::treeModel_restart_shouldExplainItselfInTheTooltip()
{
    using Entry = dsqt::SettingsSchemaEntry;
    TempSettingsFile settings("project = \"demo\"\nother = 1\n", "[project]\nrestart = true\n");

    // restart on its own is enough to make a schema entry.
    QCOMPARE(settings.file->schema("project").value("restart").toBool(), true);

    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const QModelIndex project = findValueIndex(model, "project");
    QVERIFY(project.data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>().restart);
    QVERIFY(project.data(Qt::ToolTipRole).toString().contains("Takes effect after the app restarts."));
    QVERIFY(!findValueIndex(model, "other").data(Qt::ToolTipRole).toString().contains("restart"));

    // Once changed at runtime, the tooltip says the change is waiting.
    QVERIFY(model.setData(project, QStringLiteral("changed"), Qt::EditRole));
    QVERIFY(findValueIndex(model, "project").data(Qt::ToolTipRole).toString()
                .contains("Changed: takes effect after the app restarts."));
}

// ── DsQt's built-in schema fragments ─────────────────────────────────────────

void DsSettingsTest::librarySchema_shouldBeCompiledIntoTheModules()
{
    // This test links Dsqt::Core only: Core's fragments are there, Bridge's are not.
    const QStringList engine = dsqt::SettingsFile::librarySchemaPaths("engine.toml");
    QVERIFY2(engine.contains(":/qt/qml/Dsqt/Core/schema/engine.schema.toml"),
             qPrintable(engine.join(", ")));
    QVERIFY(!engine.contains(":/qt/qml/Dsqt/Bridge/schema/engine.schema.toml"));
    QCOMPARE(dsqt::SettingsFile::librarySchemaPaths("configuration.toml"),
             QStringList{":/qt/qml/Dsqt/Core/schema/configuration.schema.toml"});
    QVERIFY(dsqt::SettingsFile::librarySchemaPaths("viewer.toml").isEmpty());
}

void DsSettingsTest::librarySchema_shouldLayerUnderTheApplicationSchema()
{
    QTemporaryDir dir;
    const auto write = [&dir](const QString &name, const QByteArray &text) {
        QFile out(dir.filePath(name));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(text);
    };
    write("engine.toml", "[engine.window]\nmode = \"window\"\n\n[engine.font.default]\nstyle = \"StyleItalic\"\n");
    write("engine.schema.toml", "[engine.window.mode]\ndescription = \"Ours\"\n");

    dsqt::SettingsFile file(nullptr, QStringList{dir.path()});
    file.setFileName("engine.toml");

    // Library fragments come first, the application's schema files after them.
    const QStringList paths = file.resolvedSchemaPaths();
    QCOMPARE(paths.first(), QStringLiteral(":/qt/qml/Dsqt/Core/schema/engine.schema.toml"));
    QCOMPARE(paths.last(), dir.filePath("engine.schema.toml"));

    // Key by key: Core's options stay, the application's description wins.
    const QVariantMap mode = file.schema("engine.window.mode");
    QCOMPARE(mode.value("options").toList().size(), 3);
    QCOMPARE(mode.value("description").toString(), QStringLiteral("Ours"));

    // Through the model: Core's dropdown, and restart inherited from [engine.font].
    using Entry = dsqt::SettingsSchemaEntry;
    dsqt::SettingsTreeModel model;
    model.setSettingsFile(&file);
    const auto entry = [&model](const QString &path) {
        return findValueIndex(model, path).data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>();
    };
    QCOMPARE(entry("engine.window.mode").kind, Entry::Kind::Options);
    QVERIFY(!entry("engine.window.mode").restart);
    QCOMPARE(entry("engine.font.default.style").kind, Entry::Kind::Options);
    QVERIFY(entry("engine.font.default.style").restart);
    QCOMPARE(findValueIndex(model, "engine.font.default.style").data(Qt::DisplayRole).toString(),
             QStringLiteral("Italic"));
}

void DsSettingsTest::schema_restart_shouldBeInheritedFromATable()
{
    using Entry = dsqt::SettingsSchemaEntry;
    TempSettingsFile settings("[group.inner]\na = 1\n\n[other]\nb = 2\n",
                              "[group]\nrestart = true\n\n[group.inner.a]\nmin = 0\nmax = 5\n");
    dsqt::SettingsTreeModel model;
    model.setSettingsFile(settings.file.get());

    const auto a = findValueIndex(model, "group.inner.a").data(dsqt::SettingsTreeModel::SchemaEntryRole).value<Entry>();
    QVERIFY(a.restart);                     // from [group], two levels up
    QCOMPARE(a.kind, Entry::Kind::Slider);  // its own entry still applies
    QVERIFY(!findValueIndex(model, "other.b").data(dsqt::SettingsTreeModel::SchemaEntryRole)
                 .value<Entry>().restart);
}

QTEST_MAIN(DsSettingsTest)
#include "tst_dssettingstest.moc"
