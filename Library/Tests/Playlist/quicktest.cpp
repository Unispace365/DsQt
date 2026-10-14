#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QJSValue>
#include <QMediaPlayer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlextensionplugin.h>
#include <core/dsQmlApplicationEngine.h>
#include <core/dsQmlEnvironment.h>
#include <memory>

Q_IMPORT_QML_PLUGIN(Dsqt_WafflesPlugin)
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)

// Real media URL resolution requires the application's DsQt engine, which the
// stock Qt Quick Test runner does not construct. Only initialize its local
// environment service; these tests must not start app settings or networking.
class PlaylistEngine : public dsqt::DsQmlApplicationEngine {
public:
    PlaylistEngine() { mQmlEnv = new dsqt::DsQmlEnvironment(this); }
};

class PlaylistTest : public QObject {
    Q_OBJECT
    PlaylistEngine engine;
    QQuickWindow window;
    std::unique_ptr<QQuickItem> view;

    QJSValue evaluate(const QString &script) {
        auto result = engine.evaluate(script);
        if (result.isError()) QTest::qFail(qPrintable(result.toString()), __FILE__, __LINE__);
        return result;
    }
    void createView(const QString &type, const QString &properties) {
        QQmlComponent component(&engine);
        component.loadFromModule("Dsqt.Waffles", type);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(evaluate(properties).toVariant().toMap())));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        view->setSize(QSizeF(640, 480));
        engine.globalObject().setProperty("view", engine.newQObject(view.get()));
    }
    QQuickItem *visualChild(QQuickItem *parent, const QString &name) {
        for (auto child : parent->childItems()) {
            if (child->objectName() == name) return child;
            if (auto match = visualChild(child, name)) return match;
        }
        return nullptr;
    }
    QObject *video(const char *name) {
        auto viewer = visualChild(view.get(), QString::fromLatin1(name));
        return viewer ? qvariant_cast<QObject *>(viewer->property("videoItem")) : nullptr;
    }
    int state(QObject *player) { return player->property("playbackState").toInt(); }

    QPointingDevice *touchDevice = QTest::createTouchDevice();

    QPoint centerOf(QQuickItem *item) {
        return item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
    }

    QQuickItem *pdfPageView(const QString &viewerName = "pdfViewer") {
        auto viewer = visualChild(view.get(), viewerName);
        if (!viewer) return nullptr;
        auto media = qvariant_cast<QObject *>(viewer->property("mediaViewer"));
        if (!media) return nullptr;
        auto pdf = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(media->property("mediaItem")));
        if (!pdf) return nullptr;
        // PdfScrollablePageView is the direct visual child of DsPdfViewer.
        for (auto child : pdf->childItems())
            if (child->property("renderScale").isValid()) return child;
        return nullptr;
    }

    void createIdleControlsScene(bool fullscreen, const QString &mediaType = "image") {
        createView("DsWaffleStage", R"(({launcher:null,launcherButtonVisible:false,glassEnabled:false,
            controlsIdleMs:0,fullscreenDuration:0,fullscreenMargin:0,
            viewerEnterAnimation:0,viewerExitAnimation:0}))");
        QVERIFY(view);
        const QString file = mediaType == "pdf" ? "/media/sample.pdf" : "/media/earth.gif";
        engine.globalObject().setProperty("idleAsset", QUrl::fromLocalFile(
            QStringLiteral(DSQT_TEST_ASSET_DIR) + file).toString());
        engine.globalObject().setProperty("idleMediaType", mediaType);
        evaluate(R"(
            view.openViewer({stage:view,objectName:"pdfViewer",x:140,y:140,
                viewerWidth:320,viewerHeight:230,minWidth:50,minHeight:50,
                contentLocked:true,matchAspectRatio:false,
                model:{title:"Idle controls",media:{type:idleMediaType,filepath:idleAsset}}});
        )");
        auto viewer = visualChild(view.get(), "pdfViewer"); QVERIFY(viewer);
        engine.globalObject().setProperty("idleViewer", engine.newQObject(viewer));
        if (mediaType == "pdf") {
            QTRY_VERIFY_WITH_TIMEOUT(pdfPageView(), 10000);
            QTRY_COMPARE_WITH_TIMEOUT(pdfPageView()->property("status").toInt(), 1, 10000);
        }
        if (fullscreen) evaluate("view.setFullscreen(idleViewer, true)");
        QTest::qWait(300);
        QTest::mouseMove(&window, QPoint(5,5));
    }

    void clickAt(const QPoint &position, bool touch) {
        if (touch) {
            auto sequence = QTest::touchEvent(&window, touchDevice, false);
            sequence.press(0, position, &window).commit();
            QTest::qWait(20);
            sequence.release(0, position, &window).commit();
        } else {
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, position);
        }
        QCoreApplication::processEvents();
    }

private slots:
    void initTestCase() {
        window.resize(640, 480);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        engine.globalObject().setProperty("videoUrl", QUrl::fromLocalFile(
            QStringLiteral(DSQT_TEST_ASSET_DIR) + "/videos/big_buck_bunny.mp4").toString());
        engine.globalObject().setProperty("imageUrl", QUrl::fromLocalFile(
            QStringLiteral(DSQT_TEST_ASSET_DIR) + "/media/earth.gif").toString());
        evaluate(R"(
            function videoSlide(uid, autoplay) {
                return {uid:uid,holdTime:1,record:{autoplay:autoplay,loop:true},
                    media:{type:'video',filepath:videoUrl}};
            }
            function layoutSlide() {
                return {uid:'layout',layout:{frameUid:'test',headline:'',subHeadline:'',tagline:'',
                    background:videoSlide('background',true).media,
                    frames:[{uid:'test',width:640,height:480,items:[{
                        uid:'tile',x:0,y:0,w:1,h:1,media:videoSlide('tile',true).media,
                        autoplay:true,loop:true,volume:0,touchEvents:true,
                        hotspots:[{uid:'link',x:0.2,y:0.2,w:0.2,h:0.2,title:'Open content',target:'destination'}]
                    }]}]}};
            }
        )");
    }
    void cleanup() {
        engine.globalObject().setProperty("view", QJSValue());
        view.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void mediaPausesAndResumesAtSamePosition() {
        createView("DsMediaTemplate", "({slide:videoSlide('video',true)})"); QVERIFY(view);
        QTRY_VERIFY_WITH_TIMEOUT(video("mv"), 10000);
        auto player = video("mv"); player->setProperty("volume", 0);
        QTRY_COMPARE_WITH_TIMEOUT(state(player), QMediaPlayer::PlayingState, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(player->property("position").toInt() > 200, 10000);
        view->setProperty("paused", true);
        QTRY_COMPARE(state(player), QMediaPlayer::PausedState);
        const int position = player->property("position").toInt();
        QTest::qWait(250);
        QCOMPARE(player->property("position").toInt(), position);
        view->setProperty("paused", false);
        QTRY_COMPARE(state(player), QMediaPlayer::PlayingState);
        QTRY_VERIFY(player->property("position").toInt() > position);
    }
    void initiallyPausedMediaWaitsForResume() {
        createView("DsMediaTemplate", "({slide:videoSlide('video',true),paused:true})"); QVERIFY(view);
        QTRY_VERIFY_WITH_TIMEOUT(video("mv"), 10000);
        auto player = video("mv"); player->setProperty("volume", 0);
        QTest::qWait(250);
        QVERIFY(state(player) != QMediaPlayer::PlayingState);
        QCOMPARE(player->property("position").toInt(), 0);
        view->setProperty("paused", false);
        QTRY_COMPARE_WITH_TIMEOUT(state(player), QMediaPlayer::PlayingState, 10000);
    }
    void resumeHonorsAutoplayFalse() {
        createView("DsMediaTemplate", "({slide:videoSlide('video',false),paused:true})"); QVERIFY(view);
        QTRY_VERIFY_WITH_TIMEOUT(video("mv"), 10000);
        auto player = video("mv");
        QTRY_VERIFY_WITH_TIMEOUT(player->property("duration").toInt() > 0, 10000);
        view->setProperty("paused", false);
        QTest::qWait(250);
        QVERIFY(state(player) != QMediaPlayer::PlayingState);
        QCOMPARE(player->property("position").toInt(), 0);
    }
    void inactiveSlideStopsPlaying() {
        createView("DsMediaTemplate", "({slide:videoSlide('video',true)})"); QVERIFY(view);
        QTRY_VERIFY_WITH_TIMEOUT(video("mv"), 10000);
        auto player = video("mv"); player->setProperty("volume", 0);
        QTRY_COMPARE_WITH_TIMEOUT(state(player), QMediaPlayer::PlayingState, 10000);
        view->setProperty("active", false);
        QTRY_COMPARE(state(player), QMediaPlayer::PausedState);
        view->setProperty("paused", true);
        view->setProperty("paused", false);
        QTest::qWait(250);
        QCOMPARE(state(player), QMediaPlayer::PausedState);
    }
    void animatedImageKeepsItsFrameWhilePaused() {
        createView("DsMediaTemplate", "({slide:{uid:'gif',record:{autoplay:true},media:{type:'image',filepath:imageUrl}}})");
        QVERIFY(view);
        auto viewer = view->findChild<QObject *>("mv"); QVERIFY(viewer);
        QTRY_VERIFY(qvariant_cast<QObject *>(viewer->property("mediaItem")));
        auto animation = qvariant_cast<QObject *>(viewer->property("mediaItem"));
        QTRY_VERIFY(animation->property("playing").toBool());
        QTRY_VERIFY(animation->property("currentFrame").toInt() > 0);
        view->setProperty("paused", true);
        QTRY_COMPARE(animation->property("paused").toBool(), true);
        const auto frame = animation->property("currentFrame").toInt();
        QTest::qWait(250);
        QCOMPARE(animation->property("currentFrame").toInt(), frame);
        view->setProperty("paused", false);
        QTRY_VERIFY(animation->property("currentFrame").toInt() != frame);
    }
    void layoutPausesBackgroundAndMedia() {
        createView("DsCustomLayoutTemplate", "({slide:layoutSlide()})"); QVERIFY(view);
        QTRY_VERIFY_WITH_TIMEOUT(video("background") && video("viewer"), 10000);
        auto background = video("background"); auto media = video("viewer");
        QTRY_COMPARE_WITH_TIMEOUT(state(background), QMediaPlayer::PlayingState, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(state(media), QMediaPlayer::PlayingState, 10000);
        view->setProperty("paused", true);
        QTRY_COMPARE(state(background), QMediaPlayer::PausedState);
        QTRY_COMPARE(state(media), QMediaPlayer::PausedState);
        view->setProperty("paused", false);
        QTRY_COMPARE(state(background), QMediaPlayer::PlayingState);
        QTRY_COMPARE(state(media), QMediaPlayer::PlayingState);
    }
    void pausedLayoutStillOpensHotspots() {
        createView("DsCustomLayoutTemplate", "({slide:layoutSlide(),paused:true})"); QVERIFY(view);
        auto hotspot = visualChild(view.get(), "hotspot:link"); QVERIFY(hotspot);
        QSignalSpy spy(view.get(), SIGNAL(contentRequested(QVariant))); QVERIFY(spy.isValid());
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
            hotspot->mapToScene(QPointF(hotspot->width()/2,hotspot->height()/2)).toPoint());
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), "destination");
    }
    void pausedPlaylistStillNavigatesAndKeepsNewMediaPaused() {
        createView("DsPlaylistViewer", "({slides:[videoSlide('first',true),videoSlide('second',true)],paused:true,fadeMs:0,transitionDuration:0})");
        QVERIFY(view);
        view->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Right);
        QTRY_COMPARE(view->property("index").toInt(), 1);
        QCOMPARE(evaluate("view.currentSlide.uid").toString(), "second");
        QTRY_VERIFY_WITH_TIMEOUT(video("mv"), 10000);
        auto player = video("mv"); player->setProperty("volume", 0);
        QTest::qWait(250);
        QVERIFY(state(player) != QMediaPlayer::PlayingState);
        view->setProperty("paused", false);
        QTRY_COMPARE_WITH_TIMEOUT(state(player), QMediaPlayer::PlayingState, 10000);
    }
    void ambientAdvanceWaitsForResume() {
        createView("DsPlaylistViewer", "({slides:[{uid:'first',holdTime:1},{uid:'second',holdTime:1}],ambient:true,paused:true,fadeMs:0,transitionDuration:0})");
        QVERIFY(view);
        QTest::qWait(1200);
        QCOMPARE(evaluate("view.currentSlide.uid").toString(), "first");
        view->setProperty("paused", false);
        QTRY_COMPARE_WITH_TIMEOUT(view->property("index").toInt(), 1, 2500);
    }
    void bundledWindowIcons_data() {
        QTest::addColumn<QString>("name");
        for (const QString &base : {"close", "collapse", "controller_collapse", "controller_expand", "fullscreen", "lock", "locked"})
            for (const QString &suffix : {"", "_pressed"}) {
                const QString name = base + suffix + ".svg";
                QTest::newRow(qPrintable(name)) << name;
            }
    }
    void bundledWindowIcons() {
        QFETCH(QString, name);
        const QString path = ":/qt/qml/Dsqt/Waffles/data/images/waffles/window/" + name;
        QVERIFY(QFile::exists(path));
        QVERIFY(!QImage(path).isNull());
    }
    void stageSelectsCustomViewerByStackingOrder_data() {
        QTest::addColumn<bool>("touch");
        QTest::newRow("mouse") << false;
        QTest::newRow("touch") << true;
    }
    void stageSelectsCustomViewerByStackingOrder() {
        QFETCH(bool, touch);
        createView("DsWaffleStage", R"(({launcher:null,launcherButtonVisible:false,
            glassEnabled:false,viewerEnterAnimation:0}))"); QVERIFY(view);
        // Pluggable viewers can leave selection to the stage's tap handler.
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Dsqt.Waffles
            DsViewer {
                width: viewerWidth; height: viewerHeight
                enterAnimation: DsViewer.None
                property bool selected: false
                Rectangle { anchors.fill: parent; color: "gray" }
            }
        )", QUrl("qrc:/stage-custom-viewer.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view->setProperty("viewer", QVariant::fromValue(&component));
        evaluate(R"(
            view.openViewer({objectName:"front",x:100,y:100,viewerWidth:300,viewerHeight:200});
            view.openViewer({objectName:"rear",x:100,y:100,viewerWidth:300,viewerHeight:200});
            view.selectViewer(view.getLayer("viewer").children[0]);
        )");
        auto front = visualChild(view.get(), "front"); QVERIFY(front);
        auto rear = visualChild(view.get(), "rear"); QVERIFY(rear);
        QVERIFY(front->z() > rear->z());
        clickAt(QPoint(200, 200), touch);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), front);
        // Equal z values use sibling order; empty space still clears selection.
        front->setZ(rear->z());
        clickAt(QPoint(200, 200), touch);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), rear);
        clickAt(QPoint(50, 400), touch);
        QVERIFY(!qvariant_cast<QObject *>(view->property("_selectedViewer")));
    }

    void stageSelectAtUsesRequestedLayer_data() {
        QTest::addColumn<QString>("layer");
        QTest::addColumn<bool>("touch");
        for (const QString &layer : {"viewer", "modal1", "modal3"})
            for (bool touch : {false, true})
                QTest::newRow(qPrintable(layer + (touch ? "-touch" : "-mouse"))) << layer << touch;
    }
    void stageSelectAtUsesRequestedLayer() {
        QFETCH(QString, layer); QFETCH(bool, touch);
        createIdleControlsScene(false); QVERIFY(view);
        auto rear = visualChild(view.get(), "pdfViewer"); QVERIFY(rear);
        engine.globalObject().setProperty("selectionLayer", layer);
        evaluate(R"(
            idleViewer.parent = view.getLayer(selectionLayer);
            view.openViewer({stage:view,layer:selectionLayer,objectName:"front",
                x:140,y:140,viewerWidth:320,viewerHeight:230,
                model:{title:"Front",media:{type:"image",filepath:idleAsset}}});
        )");
        auto front = visualChild(view.get(), "front"); QVERIFY(front);
        engine.globalObject().setProperty("selectionPoint", engine.toScriptValue(
            rear->mapToGlobal(QPointF(100, 100))));
        QCOMPARE(evaluate("view.viewerAt(selectionPoint, idleViewer.parent)").toQObject(), front);
        if (layer != "viewer") QVERIFY(evaluate("view.viewerAt(selectionPoint)").isNull());

        rear->setProperty("controlsAwake", false);
        QSignalSpy rearActivity(rear, SIGNAL(controlsAwakeChanged()));
        QVERIFY(rearActivity.isValid());
        evaluate("idleViewer.selectAt(selectionPoint)");
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), front);
        QVERIFY(front->property("selected").toBool());
        QVERIFY(!rear->property("selected").toBool());
        QCOMPARE(rearActivity.count(), 0);

        front->setVisible(false);
        evaluate("idleViewer.selectAt(selectionPoint)");
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), rear);
        QVERIFY(rear->property("selected").toBool());
        QVERIFY(!front->property("selected").toBool());
        QVERIFY(rearActivity.count() > 0);
        evaluate("view.selectViewer(null)");
        QVERIFY(!rear->property("selected").toBool());
        rear->setProperty("controlsAwake", false);
        rearActivity.clear();
        evaluate("idleViewer.selectAt(view.mapToGlobal(5, 5))");
        QVERIFY(!qvariant_cast<QObject *>(view->property("_selectedViewer")));
        QCOMPARE(rearActivity.count(), 0);
        clickAt(centerOf(rear), touch);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), rear);
        QVERIFY(rear->property("selected").toBool());
    }

    void stageOpacityDoesNotDisableSelection_data() { stageSelectsCustomViewerByStackingOrder_data(); }
    void stageOpacityDoesNotDisableSelection() {
        QFETCH(bool, touch);
        createIdleControlsScene(false); QVERIFY(view);
        auto viewer = visualChild(view.get(), "pdfViewer"); QVERIFY(viewer);
        evaluate("view.selectViewer(null); idleViewer.opacity = 0");
        const QPoint point(300, 230);
        engine.globalObject().setProperty("selectionPoint", engine.toScriptValue(window.mapToGlobal(point)));
        QCOMPARE(evaluate("view.viewerAt(selectionPoint)").toQObject(), viewer);
        clickAt(point, touch);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), viewer);
        QVERIFY(viewer->property("selected").toBool());
    }

    void stageFullscreenSelectionPreservesStacking_data() {
        QTest::addColumn<bool>("touch");
        QTest::addColumn<QString>("mediaType");
        for (bool touch : {false, true})
            for (const QString &media : {"image", "pdf"})
                QTest::newRow(qPrintable(media + (touch ? "-touch" : "-mouse"))) << touch << media;
    }
    void stageFullscreenSelectionPreservesStacking() {
        QFETCH(bool, touch); QFETCH(QString, mediaType);
        if (mediaType == "pdf") {
            QQmlComponent pdfModule(&engine);
            pdfModule.setData("import QtQuick.Pdf\nPdfDocument {}", QUrl());
            if (!pdfModule.isReady()) QSKIP("Qt Quick PDF is not available.");
        }
        createIdleControlsScene(false, mediaType); QVERIFY(view);
        auto fullscreen = visualChild(view.get(), "pdfViewer"); QVERIFY(fullscreen);
        evaluate(R"(
            view.openViewer({stage:view,objectName:"backgroundViewer",
                x:140,y:140,viewerWidth:320,viewerHeight:230,
                model:{title:"Background",media:{type:"image",filepath:idleAsset}}});
            view.selectViewer(null);
            view.setFullscreen(idleViewer, true);
        )");
        auto background = visualChild(view.get(), "backgroundViewer"); QVERIFY(background);
        engine.globalObject().setProperty("backgroundViewer", engine.newQObject(background));
        QTest::qWait(300);
        const qreal fullscreenZ = fullscreen->z();
        QVERIFY(fullscreenZ > view->property("_scrimZ").toReal());
        engine.globalObject().setProperty("selectionPoint", engine.toScriptValue(
            fullscreen->mapToGlobal(QPointF(fullscreen->width() / 2, fullscreen->height() / 2))));
        fullscreen->setProperty("controlsAwake", false);
        QSignalSpy activity(fullscreen, SIGNAL(controlsAwakeChanged()));
        QVERIFY(activity.isValid());
        evaluate("idleViewer.selectAt(selectionPoint)");
        QVERIFY(activity.count() > 0);
        QVERIFY(!qvariant_cast<QObject *>(view->property("_selectedViewer")));
        QCOMPARE(fullscreen->z(), fullscreenZ);

        // Background passive handlers and direct press-selection paths must both be harmless.
        background->setProperty("controlsAwake", false);
        QSignalSpy backgroundActivity(background, SIGNAL(controlsAwakeChanged()));
        QVERIFY(backgroundActivity.isValid());
        evaluate(R"(
            backgroundViewer.selectAt(backgroundViewer.mapToGlobal(100, 100));
            view.selectViewer(backgroundViewer);
        )");
        QCOMPARE(backgroundActivity.count(), 0);
        QVERIFY(!qvariant_cast<QObject *>(view->property("_selectedViewer")));
        QVERIFY(!background->property("selected").toBool());

        clickAt(centerOf(fullscreen), touch);
        QCOMPARE(fullscreen->z(), fullscreenZ);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_fullscreenViewer")), fullscreen);
        QVERIFY(!background->property("selected").toBool());
        // Explicit selection must also keep the fullscreen viewer above its backdrop.
        evaluate("view.selectViewer(idleViewer)");
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), fullscreen);
        QVERIFY(fullscreen->property("selected").toBool());
        QCOMPARE(fullscreen->z(), fullscreenZ);
    }

};

QTEST_MAIN(PlaylistTest)
#include "quicktest.moc"
