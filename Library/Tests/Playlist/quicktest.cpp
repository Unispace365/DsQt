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
};

QTEST_MAIN(PlaylistTest)
#include "quicktest.moc"
