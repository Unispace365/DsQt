#include <QtTest>
#include <dssettings.h>
#include <dsenvironment.h>
#include <optional>

// add necessary includes here

class DSSettingsTest : public QObject
{
    Q_OBJECT

public:
    DSSettingsTest();
    ~DSSettingsTest();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void getOrCreateSettings_shouldReturnExisitngSettingsAndTrueWhenSettingsExist();
    void getOrCreateSettings_shouldReturnNewSettingsAndFalseWhenSettingsDoNotExist();
    void getSettings_shouldReturnExistingSettingsWhenSettingsExist();
    void getSettings_shouldReturnEmptingRefWhenSettingsDoNotExist();
    void getOr_shouldReturnTheSettingValueWhentheSettingExist();
    void getOr_shoudReturnTheOrValueWhenTheSettingDoesNotExist();

    //Base types (bool,int32,uint32,int64,uint64,float,double,std::string,QString)
    void get_bool_shouldReturnExpectedBoolean_data();
    void get_bool_shouldReturnExpectedBoolean();
    void get_integral_shouldReturnExpectedIntegral_data();
    void get_integral_shouldReturnExpectedIntegral();
    void get_floatingPoint_shouldReturnExpectedFloatingPoint_data();
    void get_floatingPoint_shouldReturnExpectedFloatingPoint();
    void get_string_shouldReturnExpectedString_data();
    void get_string_shouldReturnExpectedString();


    //QColor
    void get_QColor_from_validString_shouldReturnAValidQColorOptional_data();
    void get_QColor_from_validString_shouldReturnAValidQColorOptional();
    void get_QColor_from_Arrays_shouldReturnAValidQColorOptional_data();
    void get_QColor_from_Arrays_shouldReturnAValidQColorOptional();
    void get_QColor_from_Tables_shouldReturnAValidQColorOptional_data();
    void get_QColor_from_Tables_shouldReturnAValidQColorOptional();

    //Date & time
    void get_QTime_from_validString_shouldReturnAValidQTimeOptional_data();
    void get_QTime_from_validString_shouldReturnAValidQTimeOptional();
    void get_QDate_from_validString_shouldReturnAValidQDateOptional_data();
    void get_QDate_from_validString_shouldReturnAValidQDateOptional();
    void get_QDateTime_from_validString_shouldReturnAValidQDateTimeOptional_data();
    void get_QDateTime_from_validString_shouldReturnAValidQDateTimeOptional();
	void get_QTime_from_tomlDateTime_shouldReturnAValidQTimeOptional_data();
    void get_QTime_from_tomlDateTime_shouldReturnAValidQTimeOptional();
	void get_QDate_from_tomlDateTime_shouldReturnAValidQDateOptional_data();
    void get_QDate_from_tomlDateTime_shouldReturnAValidQDateOptional();
	void get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTimeOptional_data();
    void get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTimeOptional();

    void get_shouldReturnAnEmptyOptionalWhenTheSettingDoesNotExist();
    void getWithMeta_shouldReturnAValidTupleWhenTheSettingExists();
    void read_notable();

private:
    dsqt::DSSettingsRef test_settings;
};

DSSettingsTest::DSSettingsTest()
{

}

DSSettingsTest::~DSSettingsTest()
{

}

void DSSettingsTest::initTestCase()
{
    DSEnv::loadEngineSettings();



}

void DSSettingsTest::cleanupTestCase()
{

}

void DSSettingsTest::init()
{
	QLoggingCategory::setFilterRules("settings.parser*=false\n"
									 );
    auto [existed,test] = dsqt::DSSettings::getSettingsOrCreate("test_settings");
    test_settings = test;
    if(!existed){
		//qWarning()<<"test_settings did not already exist";
    }
    DSEnv::loadSettings("test_settings","test_settings.toml");
}

void DSSettingsTest::cleanup()
{
    dsqt::DSSettings::forgetSettings("test_settings");
}

//settings creation
void DSSettingsTest::getOrCreateSettings_shouldReturnExisitngSettingsAndTrueWhenSettingsExist()
{
    auto [existed,test] = dsqt::DSSettings::getSettingsOrCreate("test_settings");
    QVERIFY(existed);
    QVERIFY(test);
}

void DSSettingsTest::getOrCreateSettings_shouldReturnNewSettingsAndFalseWhenSettingsDoNotExist()
{
    auto [existed,test] = dsqt::DSSettings::getSettingsOrCreate("new_settings");
    QVERIFY(!existed);
    QVERIFY(test);
}

void DSSettingsTest::getSettings_shouldReturnExistingSettingsWhenSettingsExist(){
    auto std_string = test_settings->get<std::string>("no_table");
    QVERIFY2(std_string == "test value","std::string fail");

}

void DSSettingsTest::getSettings_shouldReturnEmptingRefWhenSettingsDoNotExist(){
    auto std_string = test_settings->get<std::string>("not_exist");
    QVERIFY(std_string.has_value() == false);
}
void DSSettingsTest::getOr_shouldReturnTheSettingValueWhentheSettingExist(){
    auto std_string = test_settings->getOr<std::string>("no_table","fake");
    QVERIFY2(std_string != "fake","Or value was returned and it should not have been");
    QVERIFY2(std_string == "test value","Incorrect value was returned");
}
void DSSettingsTest::getOr_shoudReturnTheOrValueWhenTheSettingDoesNotExist(){
    auto std_string = test_settings->getOr<std::string>("not_exist","fake");
    QVERIFY2(std_string == "fake","Incorrect value was returned");
}
//*****************
//Base Types
//*****************
void DSSettingsTest::get_integral_shouldReturnExpectedIntegral_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<int64_t>("result");

    QTest::newRow("int positive")<<"int_positive"<<1024ll;
    QTest::newRow("int negative")<<"int_negative"<<-1024ll;
    QTest::newRow("int from string")<<"int_from_string"<<1024ll;
    QTest::newRow("int from float")<<"int_from_float"<<1024ll;

};
void DSSettingsTest::get_integral_shouldReturnExpectedIntegral(){
    QFETCH(QString, key);
    QFETCH(int64_t, result);


    auto int32_value = test_settings->get<int>("test.int."+key.toStdString());
    auto int64_value = test_settings->get<int64_t>("test.int."+key.toStdString());

    QCOMPARE(int32_value.has_value(),true);
    QCOMPARE(int64_value.has_value(),true);

    QCOMPARE(int32_value.value(),(int32_t)result);
    QCOMPARE(int64_value.value(),(int64_t)result);
};
void DSSettingsTest::get_floatingPoint_shouldReturnExpectedFloatingPoint_data(){

};
void DSSettingsTest::get_floatingPoint_shouldReturnExpectedFloatingPoint(){
    qWarning() << "Not Implemented";
};
void DSSettingsTest::get_string_shouldReturnExpectedString_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<bool>("result");

    QTest::newRow("bool type")<<"bool_true"<<true;
    QTest::newRow("string true is true")<<"bool_str_true"<<true;
    QTest::newRow("string false is false")<<"bool_str_false"<<false;
    QTest::newRow("string empty is false")<<"bool_str_empty"<<false;
    QTest::newRow("string foobar is true")<<"bool_str_foobar"<<true;
};
void DSSettingsTest::get_string_shouldReturnExpectedString(){
    qWarning() << "Not Implemented";
};
void DSSettingsTest::get_bool_shouldReturnExpectedBoolean_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<bool>("result");

    QTest::newRow("bool type")<<"bool_true"<<true;
    QTest::newRow("string true is true")<<"bool_str_true"<<true;
    QTest::newRow("string false is false")<<"bool_str_false"<<false;
    QTest::newRow("string empty is false")<<"bool_str_empty"<<false;
    QTest::newRow("string foobar is true")<<"bool_str_foobar"<<true;
};
void DSSettingsTest::get_bool_shouldReturnExpectedBoolean(){
    QFETCH(QString, key);
    QFETCH(bool, result);

    auto test_value = test_settings->get<bool>("test.bool."+key.toStdString());
    QCOMPARE(test_value.has_value(),true);
    QCOMPARE(test_value.value(),result);
};


//*****************
//QColor
//*****************
void DSSettingsTest::get_QColor_from_validString_shouldReturnAValidQColorOptional_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QColor>("result");

    QTest::newRow("hex with alpha")<<"hex"<<QColor(255,238,170,128);
    QTest::newRow("named color")<<"name"<<QColor::fromString("blue");
};
void DSSettingsTest::get_QColor_from_validString_shouldReturnAValidQColorOptional(){
    QFETCH(QString, key);
    QFETCH(QColor, result);

     auto test_value = test_settings->get<QColor>("test.color.strings."+key.toStdString());
     QCOMPARE(test_value.has_value(),true);
     QCOMPARE(test_value.value(),result);
};
void DSSettingsTest::get_QColor_from_Arrays_shouldReturnAValidQColorOptional_data(){
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
};
void DSSettingsTest::get_QColor_from_Arrays_shouldReturnAValidQColorOptional(){
    QFETCH(QString, key);
    QFETCH(QColor, result);

     auto test_value = test_settings->get<QColor>("test.color.arrays."+key.toStdString());
     QCOMPARE(test_value.has_value(),true);
     QCOMPARE(test_value.value(),result);
};
void DSSettingsTest::get_QColor_from_Tables_shouldReturnAValidQColorOptional_data(){
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
};
void DSSettingsTest::get_QColor_from_Tables_shouldReturnAValidQColorOptional(){
    QFETCH(QString, key);
    QFETCH(QColor, result);

     auto test_value = test_settings->get<QColor>("test.color.tables."+key.toStdString());
     QCOMPARE(test_value.has_value(),true);
     QCOMPARE(test_value.value(),result);
};

//*****************
//QDate, QTime, QDateTime
//*****************
void DSSettingsTest::get_QTime_from_validString_shouldReturnAValidQTimeOptional_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QTime>("result");
    QTest::addColumn<Qt::DateFormat>("format");
    QTest::addColumn<QString>("format_string");


    QTest::newRow("Iso8601")<<"ISO8601_default"<<QTime(17,30,30)<<Qt::DateFormat::ISODateWithMs<<"";
    QTest::newRow("TextDate")<<"ISO8601_default"<<QTime(17,30,30)<<Qt::DateFormat::TextDate<<"";
    QTest::newRow("Custom")<<"custom_time"<<QTime(17,30)<<Qt::DateFormat::ISODateWithMs<<"h:map";
}
void DSSettingsTest::get_QTime_from_validString_shouldReturnAValidQTimeOptional(){
    QFETCH(QString, key);
    QFETCH(QTime, result);
    QFETCH(Qt::DateFormat,format);
    QFETCH(QString, format_string);

    test_settings->setDateFormat(format);
    test_settings->setCustomDateFormat(format_string);
    auto test_value = test_settings->get<QTime>("test.time.strings."+key.toStdString());
    QCOMPARE(test_value.has_value(),true);
    QCOMPARE(test_value.value(),result);
}

void DSSettingsTest::get_QDate_from_validString_shouldReturnAValidQDateOptional_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QDate>("result");
    QTest::addColumn<Qt::DateFormat>("format");
    QTest::addColumn<QString>("format_string");


    QTest::newRow("Iso8601")<<"ISO8601_default"<<QDate(2023,1,30)<<Qt::DateFormat::ISODate<<"";
    QTest::newRow("QT::TextDate")<<"text_date"<<QDate(2023,1,30)<<Qt::DateFormat::TextDate<<"";
    QTest::newRow("RFC2822")<<"rfc2822"<<QDate(2023,1,30)<<Qt::DateFormat::RFC2822Date<<"";
    QTest::newRow("Custom")<<"custom_date"<<QDate(2023,8,27)<<Qt::DateFormat::ISODateWithMs<<"M/d/yyyy";
}
void DSSettingsTest::get_QDate_from_validString_shouldReturnAValidQDateOptional(){
    QFETCH(QString, key);
    QFETCH(QDate, result);
    QFETCH(Qt::DateFormat,format);
    QFETCH(QString, format_string);

    test_settings->setDateFormat(format);
    test_settings->setCustomDateFormat(format_string);
    auto test_value = test_settings->get<QDate>("test.date.strings."+key.toStdString());
    QCOMPARE(test_value.has_value(),true);
    QCOMPARE(test_value.value(),result);
}

void DSSettingsTest::get_QDateTime_from_validString_shouldReturnAValidQDateTimeOptional_data(){
    QTest::addColumn<QString>("key");
    QTest::addColumn<QDateTime>("result");
    QTest::addColumn<Qt::DateFormat>("format");
    QTest::addColumn<QString>("format_string");


    QTest::newRow("Iso8601")<<"ISO8601_default"<<QDateTime(QDate(2023,1,30),QTime(17,30,30))<<Qt::DateFormat::ISODateWithMs<<"";
    QTest::newRow("Custom")<<"custom_date_time"<<QDateTime(QDate(2023,8,27),QTime(7,30))<<Qt::DateFormat::ISODateWithMs<<"h:map on M/d/yyyy";
}
void DSSettingsTest::get_QDateTime_from_validString_shouldReturnAValidQDateTimeOptional(){
    QFETCH(QString, key);
    QFETCH(QDateTime, result);
    QFETCH(Qt::DateFormat,format);
    QFETCH(QString, format_string);

    test_settings->setDateFormat(format);
    test_settings->setCustomDateFormat(format_string);
    auto test_value = test_settings->get<QDateTime>("test.datetime.strings."+key.toStdString());
    QCOMPARE(test_value.has_value(),true);
    QCOMPARE(test_value.value(),result);
}

void DSSettingsTest::get_QTime_from_tomlDateTime_shouldReturnAValidQTimeOptional_data(){
	QTest::addColumn<QString>("key");
	QTest::addColumn<QTime>("result");

	QTest::newRow("offset1")<<"odt1"<<QTime(7,32);
	QTest::newRow("offset2")<<"odt2"<<QTime(0,32);
	QTest::newRow("offset3")<<"odt3"<<QTime(0,32,00,999);
}

void DSSettingsTest::get_QTime_from_tomlDateTime_shouldReturnAValidQTimeOptional(){
	QFETCH(QString, key);
	QFETCH(QTime, result);

	auto test_value = test_settings->get<QTime>("test.datetimes.toml."+key.toStdString());
	QCOMPARE(test_value.has_value(),true);
	qDebug()<<test_value.value();
	QCOMPARE(test_value.value(),result);

}

void DSSettingsTest::get_QDate_from_tomlDateTime_shouldReturnAValidQDateOptional_data(){

}

void DSSettingsTest::get_QDate_from_tomlDateTime_shouldReturnAValidQDateOptional(){
qWarning() << "Not Implemented";
}

void DSSettingsTest::get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTimeOptional_data(){

}

void DSSettingsTest::get_QDateTime_from_tomlDateTime_shouldReturnAValidQDateTimeOptional(){
qWarning() << "Not Implemented";
}



void DSSettingsTest::get_shouldReturnAnEmptyOptionalWhenTheSettingDoesNotExist(){qWarning() << "Not Implemented";}
void DSSettingsTest::getWithMeta_shouldReturnAValidTupleWhenTheSettingExists(){qWarning() << "Not Implemented";}

void DSSettingsTest::read_notable()
{
    auto val = test_settings->get<std::string>("no_table");
    QVERIFY(val);
    QVERIFY(val.value() == "test value");
}

//QTEST_APPLESS_MAIN(DSSettingsTest)
QTEST_MAIN(DSSettingsTest)
#include "tst_dssettingstest.moc"
