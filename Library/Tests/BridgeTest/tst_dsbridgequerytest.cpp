#include <QtTest/QTest>
#include <QObject>

class DSBridgeQueryTest : public QObject
{
    Q_OBJECT
public:
    explicit DSBridgeQueryTest(QObject *parent = nullptr);
    ~DSBridgeQueryTest();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void queryTables_shouldCreateAContentModelUnderRoot_data();
    void queryTables_shouldCreateAContentModelUnderRoot();

};

DSBridgeQueryTest::DSBridgeQueryTest(QObject *parent)
    : QObject{parent}
{}

DSBridgeQueryTest::~DSBridgeQueryTest() {

}

void DSBridgeQueryTest::initTestCase()
{

}

void DSBridgeQueryTest::cleanupTestCase()
{

}

void DSBridgeQueryTest::init()
{

}

void DSBridgeQueryTest::cleanup()
{

}

void DSBridgeQueryTest::queryTables_shouldCreateAContentModelUnderRoot_data()
{

}

void DSBridgeQueryTest::queryTables_shouldCreateAContentModelUnderRoot()
{
    //QVERIFY(false);
}

QTEST_MAIN(DSBridgeQueryTest)

#include "tst_dsbridgequerytest.moc"

