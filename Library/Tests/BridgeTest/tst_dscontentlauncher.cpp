#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QJSValue>
#include <QFile>
#include <QTemporaryDir>
#include <settings/dsSettings.h>
#include <settings/dsSettingsFile.h>
#include <memory>

class ContentLauncherTest : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir settingsDir;
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QObject> model;
    dsqt::SettingsFile *settings = nullptr;

    QString source(const char *relative) const {
        return QStringLiteral(DSQT_SOURCE_DIR) + QString::fromUtf8(relative);
    }

    QJSValue evaluate(const QString &script) {
        QJSValue result = engine->evaluate(script);
        if (result.isError())
            QTest::qFail(qPrintable(result.toString()), __FILE__, __LINE__);
        return result;
    }

    void createModel(const QVariantMap &properties = {}) {
        QQmlComponent component(engine.get(), QUrl::fromLocalFile(source(
            "/Library/DsQt/Bridge/qml/DsContentLauncherModel.qml")));
        if (component.isError()) {
            QTest::qFail(qPrintable(component.errorString()), __FILE__, __LINE__);
            return;
        }
        QVariantMap initial = properties;
        // Tests supply records directly and never query a real event schedule or database.
        initial.insert("interactiveEventTypes", QStringList{});
        model.reset(component.createWithInitialProperties(initial));
        QVERIFY2(model, qPrintable(component.errorString()));
        engine->globalObject().setProperty("model", engine->newQObject(model.get()));
    }

private slots:
    void initTestCase() {
        QVERIFY(settingsDir.isValid());
        dsqt::Settings::instance().setSearchPaths({settingsDir.path()});
        dsqt::Settings::add("app_settings");
        settings = dsqt::Settings::instance().settingsFile("app_settings");
        QVERIFY(settings);
        engine = std::make_unique<QQmlEngine>();
    }

    void init() {
        settings->setExtraFiles({});
        settings->resetOverrides();
    }

    void cleanup() {
        model.reset();
        engine->globalObject().setProperty("model", QJSValue());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void legacyDefaultsAndQmlOverrides() {
        createModel({{"mediaField", "asset"}, {"platformContentField", "app_menu"}});
        QVERIFY(model);
        QCOMPARE(model->property("platformContentField").toString(), "app_menu");
        QCOMPARE(model->property("eventContentField").toString(), "content");
        QCOMPARE(model->property("unknownTypePolicy").toString(), "show");
        const auto item = evaluate(R"(model.itemFor({uid:'a', type_name:'Media',
            asset:{type:'pdf',filepath:'document.pdf'}}))");
        QCOMPARE(item.property("kind").toString(), "pdf");
        QVERIFY(item.property("launcherVisible").toBool());
        QVERIFY(evaluate("model.itemFor({uid:'p',type_name:'Interactive Playlist'}).hasChildren").toBool());
    }

    void starterLoadsAsExtraAppSettings() {
        settings->setExtraFiles({source("/Examples/ClonerSource/settings/waffles_content.toml")});
        QCOMPARE(settings->getObj("waffles.content").value("unknownTypePolicy").toString(), "hide");
        createModel();
        QVERIFY(model);
        QCOMPARE(model->property("frameUid").toString(), "t1bv0BSfh7g6");
        QCOMPARE(evaluate("model.contentConfig.events.typeNames[0]").toString(), "Scheduled Content");
        QCOMPARE(evaluate("model.searchFilters.length").toInt(), 7);
        QVERIFY(evaluate("model.itemFor({uid:'a',type_uid:'YhPdk0XdFnRD'}).hasChildren").toBool());
        QVERIFY(!evaluate("model.itemFor({uid:'p',type_uid:'bweKU2WOurPH'}).hasChildren").toBool());
        QCOMPARE(evaluate("model.itemFor({uid:'p',type_uid:'TybD4YgiVHB3'}).playlistMode").toString(), "ambient");
    }

    void multipleTypesAndIndependentFields() {
        QFile file(settingsDir.filePath("multiple-types.toml"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"(
[waffles.content]
unknownTypePolicy = "hide"
[waffles.content.types.photo]
kind = "media"
mediaField = "picture"
titleFields = ["caption"]
tapAction = "open"
[waffles.content.types.document]
kind = "media"
mediaField = "document"
titleFields = ["document_title"]
thumbnailFields = ["cover"]
tapAction = "open"
[waffles.content.types.collection]
kind = "folder"
tapAction = "browse"
[waffles.content.types.presentation]
kind = "playlist"
tapAction = "open"
playlistMode = "ambient"
[waffles.content.platform]
contentField = "custom_menu"
[waffles.content.playback]
defaultHoldTimeSeconds = 12
)");
        file.close();
        settings->setExtraFiles({"multiple-types.toml"});
        createModel();
        QVERIFY(model);
        QCOMPARE(model->property("platformContentField").toString(), "custom_menu");
        QCOMPARE(model->property("defaultHoldTimeSeconds").toDouble(), 12.0);
        auto photo = evaluate(R"(model.itemFor({uid:'1',type_uid:'photo',caption:'Lake',
            picture:{type:'image',filepath:'lake.jpg'}}))");
        QCOMPARE(photo.property("kind").toString(), "image");
        QCOMPARE(photo.property("title").toString(), "Lake");
        QCOMPARE(photo.property("media").property("filepath").toString(), "lake.jpg");
        auto document = evaluate(R"(model.itemFor({uid:'2',type_uid:'document',document_title:'Guide',
            document:{type:'pdf'},cover:{filepath:'cover.jpg'}}))");
        QCOMPARE(document.property("kind").toString(), "pdf");
        QCOMPARE(document.property("title").toString(), "Guide");
        QCOMPARE(document.property("thumbnail").toString(), "cover.jpg");
        QVERIFY(!document.property("hasChildren").toBool());
        QVERIFY(evaluate("model.itemFor({uid:'3',type_uid:'collection'}).hasChildren").toBool());
        auto playlist = evaluate("model.playlistFor({uid:'4',type_uid:'presentation'})");
        QVERIFY(!playlist.property("hasChildren").toBool());
        QCOMPARE(playlist.property("playlistMode").toString(), "ambient");
    }

    void searchByExactTypeAndHiddenChildren() {
        createModel();
        QVERIFY(model);
        evaluate(R"(
            model.contentConfig = {unknownTypePolicy:'hide', types:{
                first:{kind:'media'}, second:{kind:'media'}, folder:{kind:'folder'}}};
            const children = [
                {uid:'one',type_uid:'first',record_name:'One',media:{type:'image'}},
                {uid:'two',type_uid:'second',record_name:'Two',media:{type:'image'}},
                {uid:'hidden',type_uid:'unregistered',record_name:'Hidden',media:{type:'pdf'}}];
            const folder = model.itemFor({uid:'root',type_uid:'folder',children:children});
            model._libraryItems = [folder];
            model.library = model._launcherItems([folder]);
            model.enter(folder);
        )");
        QCOMPARE(evaluate("model.displayedItems.length").toInt(), 2);
        evaluate("model.setSearchFilter({typeUids:['second']})");
        QCOMPARE(evaluate("model.searchResults.length").toInt(), 1);
        QCOMPARE(evaluate("model.searchResults[0].uid").toString(), "two");
        evaluate("model.setSearchFilter({kinds:['image']})");
        QCOMPARE(evaluate("model.searchResults.length").toInt(), 2);
        evaluate("model.setSearchFilter({})");
        QCOMPARE(evaluate("model.searchResults.length").toInt(), 0);
        evaluate("model.searchQuery = 'Hidden'");
        QCOMPARE(evaluate("model.searchResults.length").toInt(), 0);
        // Unregistered structural records remain available to playlists and layouts.
        QCOMPARE(evaluate("model._resolveChildren(model._libraryItems[0].record).length").toInt(), 3);
    }

    void layoutUsesConfiguredTypesAndKeepsUnlistedMedia() {
        createModel();
        QVERIFY(model);
        auto layout = evaluate(R"(
            model.contentConfig = {unknownTypePolicy:'hide', layout:{frameUid:'wide'},
                templates:{custom:{renderer:'customLayout',frames:[{uid:'wide',width:80,height:27}]}}};
            model.layoutFor({uid:'slide',type_uid:'custom',children:[
                {uid:'part',type_uid:'internal',media:{type:'image',filepath:'a.png'},
                 wide_x:0,wide_y:0,wide_w:1,wide_h:1,children:[
                    {uid:'hotspot',destination:'target',hotspot_x:0,hotspot_y:0,hotspot_w:1,hotspot_h:1}]}]})
        )");
        QCOMPARE(layout.property("frameUid").toString(), "wide");
        QCOMPARE(layout.property("frames").property("length").toInt(), 1);
        QCOMPARE(layout.property("frames").property(0).property("items").property("length").toInt(), 1);
        QCOMPARE(layout.property("frames").property(0).property("items").property(0)
                     .property("hotspots").property("length").toInt(), 1);
    }

    void settingsReloadAndExplicitOverrides() {
        createModel({{"platformContentField", "qml_override"}});
        QVERIFY(model);
        settings->setOverride("waffles.content.platform.contentField", "settings_menu");
        settings->setOverride("waffles.content.layout.frameUid", "wide");
        settings->setOverride("waffles.content.playback.defaultHoldTimeSeconds", 17);
        QTRY_COMPARE(model->property("frameUid").toString(), "wide");
        QCOMPARE(model->property("platformContentField").toString(), "qml_override");
        QCOMPARE(model->property("defaultHoldTimeSeconds").toDouble(), 17.0);
    }

    void rendererRegistration() {
        createModel();
        QVERIFY(model);
        QVERIFY(evaluate(R"(
            model.contentConfig = {templates:{typeA:{renderer:'media'},typeB:{renderer:'customLayout'}}};
            const mediaRenderer = {}; const layoutRenderer = {};
            const registry = model.templateComponents({media:mediaRenderer,customLayout:layoutRenderer});
            registry.typeA === mediaRenderer && registry.typeB === layoutRenderer
        )").toBool());
    }

    void explicitFrameNeverFallsBack() {
        QQmlComponent component(engine.get(), QUrl::fromLocalFile(source(
            "/Library/DsQt/Waffles/qml/waffles/playlist/DsCustomLayoutTemplate.qml")));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> view(component.create());
        QVERIFY2(view, qPrintable(component.errorString()));
        engine->globalObject().setProperty("layoutView", engine->newQObject(view.get()));
        evaluate(R"(
            layoutView.width = 1600; layoutView.height = 900;
            layoutView.slide = {uid:'test',layout:{frameUid:'wide',headline:'',subHeadline:'',tagline:'',frames:[
                {uid:'normal',width:16,height:9,items:[]},
                {uid:'wide',width:80,height:27,items:[]}]}};
        )");
        QCOMPARE(evaluate("layoutView.frame.uid").toString(), "wide");
        evaluate("layoutView.frameUid = ''");
        QCOMPARE(evaluate("layoutView.frame.uid").toString(), "normal");
        QTest::ignoreMessage(QtWarningMsg,
            "[DsCustomLayoutTemplate] No authored content for frame missing on slide test");
        evaluate("layoutView.frameUid = 'missing'");
        QVERIFY(evaluate("layoutView.frame === null").toBool());
        QVERIFY(evaluate("layoutView.missingFrame").toBool());
        const auto canvas = view->findChild<QObject *>("customLayoutCanvas");
        QVERIFY(canvas);
        QVERIFY(!canvas->property("visible").toBool());
        QCoreApplication::processEvents();
        engine->globalObject().setProperty("layoutView", QJSValue());
    }
};

QTEST_MAIN(ContentLauncherTest)
#include "tst_dscontentlauncher.moc"
