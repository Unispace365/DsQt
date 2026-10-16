#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>

#include <QMetaEnum>
#include <QMetaType>
#include <QtTest/QTest>

using dsqt::touchengine::DsTouchEngineLinkInfo;
using dsqt::touchengine::DsTouchEngineTypes;

class DsTouchEngineTypesTest final : public QObject
{
    Q_OBJECT

private slots:
    void stateName_data();
    void stateName();
    void graphicsApiName_data();
    void graphicsApiName();
    void enumsAreRegisteredWithQtMetaObject();
    void linkInfoDefaults();
    void linkInfoConvertsToVariantMap();
};

void DsTouchEngineTypesTest::stateName_data()
{
    QTest::addColumn<DsTouchEngineTypes::State>("state");
    QTest::addColumn<QString>("name");

    QTest::newRow("idle")
        << DsTouchEngineTypes::State::Idle << QStringLiteral("idle");
    QTest::newRow("waiting-for-renderer")
        << DsTouchEngineTypes::State::WaitingForRenderer
        << QStringLiteral("waitingForRenderer");
    QTest::newRow("configuring")
        << DsTouchEngineTypes::State::Configuring << QStringLiteral("configuring");
    QTest::newRow("loading")
        << DsTouchEngineTypes::State::Loading << QStringLiteral("loading");
    QTest::newRow("ready")
        << DsTouchEngineTypes::State::Ready << QStringLiteral("ready");
    QTest::newRow("unloading")
        << DsTouchEngineTypes::State::Unloading << QStringLiteral("unloading");
    QTest::newRow("error")
        << DsTouchEngineTypes::State::Error << QStringLiteral("error");
    QTest::newRow("out-of-range")
        << static_cast<DsTouchEngineTypes::State>(-1) << QStringLiteral("unknown");
}

void DsTouchEngineTypesTest::stateName()
{
    QFETCH(DsTouchEngineTypes::State, state);
    QFETCH(QString, name);

    QCOMPARE(DsTouchEngineTypes::stateName(state), name);
}

void DsTouchEngineTypesTest::graphicsApiName_data()
{
    QTest::addColumn<DsTouchEngineTypes::GraphicsApi>("api");
    QTest::addColumn<QString>("name");

    QTest::newRow("unknown")
        << DsTouchEngineTypes::GraphicsApi::Unknown << QStringLiteral("unknown");
    QTest::newRow("opengl")
        << DsTouchEngineTypes::GraphicsApi::OpenGL << QStringLiteral("opengl");
    QTest::newRow("d3d11")
        << DsTouchEngineTypes::GraphicsApi::Direct3D11 << QStringLiteral("d3d11");
    QTest::newRow("d3d12")
        << DsTouchEngineTypes::GraphicsApi::Direct3D12 << QStringLiteral("d3d12");
    QTest::newRow("vulkan")
        << DsTouchEngineTypes::GraphicsApi::Vulkan << QStringLiteral("vulkan");
    QTest::newRow("unsupported")
        << DsTouchEngineTypes::GraphicsApi::Unsupported << QStringLiteral("unsupported");
    QTest::newRow("out-of-range")
        << static_cast<DsTouchEngineTypes::GraphicsApi>(-1) << QStringLiteral("unknown");
}

void DsTouchEngineTypesTest::graphicsApiName()
{
    QFETCH(DsTouchEngineTypes::GraphicsApi, api);
    QFETCH(QString, name);

    QCOMPARE(DsTouchEngineTypes::graphicsApiName(api), name);
}

void DsTouchEngineTypesTest::enumsAreRegisteredWithQtMetaObject()
{
    const QMetaEnum stateMetaEnum = QMetaEnum::fromType<DsTouchEngineTypes::State>();
    const QMetaEnum apiMetaEnum = QMetaEnum::fromType<DsTouchEngineTypes::GraphicsApi>();
    const QMetaEnum timeModeMetaEnum = QMetaEnum::fromType<DsTouchEngineTypes::TimeMode>();
    const QMetaEnum scopeMetaEnum = QMetaEnum::fromType<DsTouchEngineTypes::LinkScope>();
    const QMetaEnum linkTypeMetaEnum = QMetaEnum::fromType<DsTouchEngineTypes::LinkType>();

    QVERIFY(stateMetaEnum.isValid());
    QVERIFY(apiMetaEnum.isValid());
    QVERIFY(timeModeMetaEnum.isValid());
    QVERIFY(scopeMetaEnum.isValid());
    QVERIFY(linkTypeMetaEnum.isValid());
    QCOMPARE(QString::fromLatin1(stateMetaEnum.valueToKey(
                 static_cast<int>(DsTouchEngineTypes::State::WaitingForRenderer))),
             QStringLiteral("WaitingForRenderer"));
    QCOMPARE(QString::fromLatin1(apiMetaEnum.valueToKey(
                 static_cast<int>(DsTouchEngineTypes::GraphicsApi::Direct3D12))),
             QStringLiteral("Direct3D12"));

    QVERIFY(QMetaType::fromType<DsTouchEngineTypes::State>().isValid());
    QVERIFY(QMetaType::fromType<DsTouchEngineTypes::GraphicsApi>().isValid());
    QVERIFY(QMetaType::fromType<DsTouchEngineTypes::TimeMode>().isValid());
    QVERIFY(QMetaType::fromType<DsTouchEngineTypes::LinkScope>().isValid());
    QVERIFY(QMetaType::fromType<DsTouchEngineTypes::LinkType>().isValid());
    QVERIFY(QMetaType::fromType<DsTouchEngineLinkInfo>().isValid());
}

void DsTouchEngineTypesTest::linkInfoDefaults()
{
    const DsTouchEngineLinkInfo info;

    QVERIFY(info.identifier.isEmpty());
    QVERIFY(info.name.isEmpty());
    QVERIFY(info.label.isEmpty());
    QCOMPARE(info.scope, DsTouchEngineTypes::LinkScope::Input);
    QCOMPARE(info.type, DsTouchEngineTypes::LinkType::Unknown);
    QCOMPARE(info.count, 0);
}

void DsTouchEngineTypesTest::linkInfoConvertsToVariantMap()
{
    DsTouchEngineLinkInfo info;
    info.identifier = QStringLiteral("op/textureOut");
    info.name = QStringLiteral("textureOut");
    info.label = QStringLiteral("Texture Output");
    info.scope = DsTouchEngineTypes::LinkScope::Output;
    info.type = DsTouchEngineTypes::LinkType::Texture;
    info.count = 4;

    const QVariantMap map = info.toVariantMap();

    QCOMPARE(map.size(), 6);
    QCOMPARE(map.value(QStringLiteral("identifier")).toString(), info.identifier);
    QCOMPARE(map.value(QStringLiteral("name")).toString(), info.name);
    QCOMPARE(map.value(QStringLiteral("label")).toString(), info.label);
    QCOMPARE(map.value(QStringLiteral("scope")).value<DsTouchEngineTypes::LinkScope>(),
             info.scope);
    QCOMPARE(map.value(QStringLiteral("type")).value<DsTouchEngineTypes::LinkType>(),
             info.type);
    QCOMPARE(map.value(QStringLiteral("count")).toInt(), info.count);
}

QTEST_GUILESS_MAIN(DsTouchEngineTypesTest)

#include "tst_dstouchenginetypes.moc"
