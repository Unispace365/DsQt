#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QJSValue>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtQml/qqmlextensionplugin.h>
#include <core/dsQmlApplicationEngine.h>
#include <core/dsQmlEnvironment.h>
#include <capture/dsCaptureSession.h>
#include <memory>
#ifdef DSQT_TEST_WEBENGINE
#include <QtWebEngineQuick/QtWebEngineQuick>
#endif

Q_IMPORT_QML_PLUGIN(Dsqt_WafflesPlugin)
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)

// Use the real URL resolver without initializing Bridge, settings or network sync.
class ContentStateEngine : public dsqt::DsQmlApplicationEngine {
public:
    ContentStateEngine() { mQmlEnv = new dsqt::DsQmlEnvironment(this); }
};

class ContentStateTest : public QObject {
    Q_OBJECT
    ContentStateEngine engine;
    QQuickWindow window;
    QTemporaryDir files;
    std::unique_ptr<QQuickItem> view;

    QJSValue evaluate(const QString &script) {
        const auto result = engine.evaluate(script);
        if (result.isError()) QTest::qFail(qPrintable(result.toString()), __FILE__, __LINE__);
        return result;
    }
    void createView(const QString &module, const QString &type, const QString &properties = "({})") {
        QQmlComponent component(&engine);
        component.loadFromModule(module, type);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(evaluate(properties).toVariant().toMap())));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        view->setSize(QSizeF(640, 480));
        engine.globalObject().setProperty("view", engine.newQObject(view.get()));
    }
    void setMedia(const QString &type, const QString &path, bool cropped = false) {
        QVariantMap media{{"type", type}, {"filepath", QUrl::fromLocalFile(path).toString()}};
        if (cropped) media.insert("crop", QVariantList{0.1, 0.1, 0.8, 0.8});
        view->setProperty("media", media);
    }
    QQuickItem *visualChild(QQuickItem *parent, const QString &name) {
        for (auto child : parent->childItems()) {
            if (child->objectName() == name) return child;
            if (auto match = visualChild(child, name)) return match;
        }
        return nullptr;
    }

private slots:
    void initTestCase() {
        QVERIFY(files.isValid());
        // Enumerate no devices in this suite; no physical camera is opened.
        QVERIFY(DsCaptureSession::setBackendProvider([] { return QList<DsCaptureDevice>{}; }, {}));
        window.resize(640, 480);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(files.filePath("valid.png")));
        engine.globalObject().setProperty("validImage", QUrl::fromLocalFile(files.filePath("valid.png")).toString());
    }
    void cleanup() {
        engine.globalObject().setProperty("view", QJSValue());
        view.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void missingMedia_data() {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("cropped");
        QTest::newRow("image") << QString("image") << QString("missing.png") << false;
        QTest::newRow("cropped image") << QString("image") << QString("missing.png") << true;
        QTest::newRow("animated image") << QString("image") << QString("missing.gif") << false;
        QTest::newRow("vector") << QString("vector") << QString("missing.svg") << false;
        QTest::newRow("video") << QString("video") << QString("missing.mp4") << false;
        QTest::newRow("pdf") << QString("pdf") << QString("missing.pdf") << false;
        QTest::newRow("unsupported") << QString("") << QString("missing.bin") << false;
        QTest::newRow("capture") << QString("capture") << QString("no-device") << false;
#ifdef DSQT_TEST_WEBENGINE
        QTest::newRow("web") << QString("web") << QString("missing.html") << false;
#endif
    }
    void missingMedia() {
        QFETCH(QString, type); QFETCH(QString, name); QFETCH(bool, cropped);
        createView("Dsqt.Core", "DsMediaViewer"); QVERIFY(view);
        QSignalSpy ready(view.get(), SIGNAL(mediaLoaded()));
        QSignalSpy failed(view.get(), SIGNAL(mediaFailed(QString)));
        setMedia(type, files.filePath(name), cropped);
        QTRY_COMPARE_WITH_TIMEOUT(view->property("loadState").toString(), "error", 10000);
        QVERIFY(!view->property("loading").toBool());
        QVERIFY(!view->property("errorString").toString().isEmpty());
        QCOMPARE(ready.count(), 0);
        QVERIFY(failed.count() > 0);
    }
    void sourceChangeRecoversAndClears() {
        createView("Dsqt.Core", "DsMediaViewer"); QVERIFY(view);
        setMedia("image", files.filePath("absent.png"));
        QTRY_COMPARE(view->property("loadState").toString(), "error");
        setMedia("image", files.filePath("valid.png"));
        QTRY_COMPARE(view->property("loadState").toString(), "ready");
        QVERIFY(view->property("errorString").toString().isEmpty());
        view->setProperty("media", QVariantMap{});
        QTRY_COMPARE(view->property("loadState").toString(), "empty");
        QVERIFY(!view->property("loading").toBool());
        QVERIFY(!qvariant_cast<QObject *>(view->property("mediaItem")));
    }
    void retrySameSourceAfterFileArrives() {
        createView("Dsqt.Core", "DsMediaViewer"); QVERIFY(view);
        const auto path = files.filePath("arriving.png");
        setMedia("image", path);
        QTRY_COMPARE(view->property("loadState").toString(), "error");
        QVERIFY(QFile::copy(files.filePath("valid.png"), path));
        evaluate("view.retry()");
        QTRY_COMPARE(view->property("loadState").toString(), "ready");
    }
    void latestSourceWins() {
        createView("Dsqt.Core", "DsMediaViewer"); QVERIFY(view);
        setMedia("video", files.filePath("absent.mp4"));
        setMedia("image", files.filePath("valid.png"));
        QTRY_COMPARE(view->property("loadState").toString(), "ready");
        QCOMPARE(view->property("mediaType").toString(), "image");
        QTest::qWait(100);
        QCOMPARE(view->property("loadState").toString(), "ready");
    }
    void missingFrameIsVisibleAndRecoveryHidesMessage() {
        createView("Dsqt.Waffles", "DsCustomLayoutTemplate",
            "({slide:{uid:'slide',layout:{tagline:'',headline:'',subHeadline:'',frameUid:'wide',frames:[{uid:'standard',width:640,height:480,items:[]}]}}})");
        QVERIFY(view);
        auto state = visualChild(view.get(), "contentState"); QVERIFY(state);
        QVERIFY(state->isVisible());
        QCOMPARE(state->property("title").toString(), "Layout unavailable");
        view->setProperty("frameUid", "standard");
        QTRY_VERIFY(!state->isVisible());
    }
    void mediaTemplateShowsEmptyThenContent() {
        createView("Dsqt.Waffles", "DsMediaTemplate"); QVERIFY(view);
        auto state = visualChild(view.get(), "contentState"); QVERIFY(state);
        QVERIFY(state->isVisible());
        evaluate("view.slide = {record:{},media:{type:'image',filepath:validImage}}");
        QTRY_VERIFY(!state->isVisible());
    }
    void errorWindowUsesSharedState() {
        createView("Dsqt.Waffles", "DsWaffleStage", "({launcher:null,glassEnabled:false})"); QVERIFY(view);
        evaluate("view.showContentError('Content unavailable', 'Choose another item.')");
        auto state = visualChild(view.get(), "contentState"); QVERIFY(state);
        QTRY_VERIFY(state->isVisible());
        QCOMPARE(state->property("message").toString(), "Choose another item.");
        QCOMPARE(state->property("actionText").toString(), "");
        evaluate("view.clearForeground()");
    }
    void emptyFolderCanGoBack() {
        createView("Dsqt.Waffles", "DsContentLauncher", "({stage:null,shown:true,model:{ready:true,atRoot:false,path:[],displayedItems:[],currentPlaylist:null,back:function(){this.atRoot=true}}})");
        QVERIFY(view);
        QQuickItem *state = nullptr;
        QTRY_VERIFY((state = visualChild(view.get(), "contentState")));
        QTRY_VERIFY(state->isVisible());
        QCOMPARE(state->property("title").toString(), "Empty folder");
        QVERIFY(QMetaObject::invokeMethod(state, "actionRequested"));
        QVERIFY(evaluate("view.model.atRoot").toBool());
    }
    void iconAndVisualSnapshot() {
        const QString icon = ":/qt/qml/Dsqt/Waffles/data/images/waffles/missing_content.svg";
        QVERIFY(QFile::exists(icon));
        QVERIFY(!QImage(icon).isNull());
        createView("Dsqt.Waffles", "DsContentState", "({contentState:'error',actionText:'Try again'})"); QVERIFY(view);
        QTest::qWait(150);
        const auto snapshot = window.grabWindow();
        QVERIFY(!snapshot.isNull());
        QVERIFY(snapshot.save("missing-content.png"));
    }
};

int main(int argc, char **argv) {
#ifdef DSQT_TEST_WEBENGINE
    QtWebEngineQuick::initialize();
#endif
    QApplication app(argc, argv);
    ContentStateTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "quicktest.moc"
