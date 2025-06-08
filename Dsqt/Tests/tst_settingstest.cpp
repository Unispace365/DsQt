#include <QtTest>

// add necessary includes here

class SettingsTest : public QObject
{
    Q_OBJECT

public:
    SettingsTest();
    ~SettingsTest();

private slots:
    void test_case1();
};

SettingsTest::SettingsTest() {}

SettingsTest::~SettingsTest() {}

void SettingsTest::test_case1() {}

QTEST_APPLESS_MAIN(SettingsTest)

#include "tst_settingstest.moc"
