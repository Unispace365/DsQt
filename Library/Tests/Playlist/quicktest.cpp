#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QInputMethod>
#include <QJSValue>
#include <QMediaPlayer>
#include <QQmlComponent>
#include <QQmlAbstractUrlInterceptor>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryFile>
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
    // Resolve menu icons from the existing test assets without initializing app settings.
    class AssetUrls : public QQmlAbstractUrlInterceptor {
        QUrl intercept(const QUrl &url, DataType) override {
            const QString prefix = QStringLiteral("file:///data/");
            const QString path = url.toString();
            return path.startsWith(prefix)
                ? QUrl::fromLocalFile(QStringLiteral(DSQT_TEST_ASSET_DIR) + "/" + path.mid(prefix.size()))
                : url;
        }
    } assetUrls;
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

    void createInputScene(bool menu) {
        const QByteArray foreground = menu ? R"(
            DsQuickMenu {
                id: overlay
                objectName: "overlay"
                x: 320; y: 240
                innerRadius: 60; outerRadius: 120
                revealDuration: 0; segmentStagger: 0; closeTimer: 0
                model: [{iconText: "Open"}, {iconText: "Unavailable", available: false}]
                state: "On"
                onItemSelected: scene.actions++
            }
        )" : R"(
            DsTitledMediaViewer {
                id: overlay
                objectName: "overlay"
                x: 160; y: 140
                viewerWidth: 300; viewerHeight: 200
                minWidth: 50; minHeight: 50
                selected: true; controlsIdleMs: 0
                glassEnabled: false; pinchEnabled: true
                model: ({title: "Viewer", media: {type: "image", filepath: scene.asset}})
                controls: [
                    DsControlSet {
                        edge: DsControlSet.TopOuter
                        height: 44
                        Rectangle { anchors.fill: parent; color: "gray" }
                        DragHandler { target: overlay }
                        Button {
                            objectName: "viewerButton"
                            anchors.right: parent.right
                            width: 44; height: 44
                            onClicked: scene.actions++
                        }
                    }
                ]
            }
        )";
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import QtQuick.Controls.Basic
            import Dsqt.Core
            import Dsqt.Waffles
            Item {
                id: scene
                width: 640; height: 480
                required property url asset
                property int underneath: 0
                property int actions: 0
                DsCustomLayoutMedia {
                    anchors.fill: parent
                    item: ({uid: "background", media: {type: "image", filepath: scene.asset},
                        hotspots: [{uid: "link", x: 0, y: 0, w: 1, h: 1, target: "destination"}]})
                    onContentRequested: scene.underneath++
                }
        )" + foreground + "\n}", QUrl("qrc:/input-regression.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties({
            {"asset", QUrl::fromLocalFile(QStringLiteral(DSQT_TEST_ASSET_DIR) + "/media/earth.gif")}
        })));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        QTest::qWait(300); // Settle control/reveal animations before hit testing.
    }

    void createPdfScene(const QString &overlayType = {}, const QUrl &pdfUrl = {}) {
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Dsqt.Core
            import Dsqt.Waffles
            Item {
                id: scene
                width: 640; height: 480
                required property url pdfUrl
                required property url imageUrl
                required property string overlayType
                property int underneath: 0
                property int underneathWheels: 0
                MouseArea {
                    anchors.fill: parent
                    onWheel: scene.underneathWheels++
                }
                DsCustomLayoutMedia {
                    anchors.fill: parent
                    item: ({uid: "background", media: {type: "image", filepath: scene.imageUrl},
                        hotspots: [{uid: "link", x: 0, y: 0, w: 1, h: 1, target: "destination"}]})
                    onContentRequested: scene.underneath++
                }
                TitledMediaViewer {
                    objectName: "pdfViewer"
                    x: 80; y: 90
                    viewerWidth: 480; viewerHeight: 340
                    minWidth: 50; minHeight: 50
                    selected: true; controlsIdleMs: 0
                    glassEnabled: false; pinchEnabled: true
                    mediaFillMode: Image.PreserveAspectFit
                    model: ({title: "PDF", media: {type: "pdf", filepath: scene.pdfUrl}})
                }
                DsQuickMenu {
                    objectName: "menuOverlay"
                    visible: scene.overlayType === "menu"
                    x: 320; y: 220
                    innerRadius: 60; outerRadius: 120
                    revealDuration: 0; segmentStagger: 0; closeTimer: 0
                    model: [{iconText: "Open"}, {iconText: "Unavailable", available: false}]
                    state: scene.overlayType === "menu" ? "On" : "Off"
                }
                TitledMediaViewer {
                    objectName: "viewerOverlay"
                    visible: scene.overlayType === "viewer"
                    x: 160; y: 140
                    viewerWidth: 300; viewerHeight: 200
                    minWidth: 50; minHeight: 50
                    selected: true; controlsIdleMs: 0
                    glassEnabled: false; pinchEnabled: true
                    model: ({title: "Overlay", media: {type: "image", filepath: scene.imageUrl}})
                }
            }
        )", QUrl("qrc:/pdf-input-regression.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const QString assets = QStringLiteral(DSQT_TEST_ASSET_DIR);
        view.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties({
            {"pdfUrl", pdfUrl.isEmpty() ? QUrl::fromLocalFile(assets + "/media/sample.pdf") : pdfUrl},
            {"imageUrl", QUrl::fromLocalFile(assets + "/media/earth.gif")},
            {"overlayType", overlayType}
        })));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        QTRY_VERIFY_WITH_TIMEOUT(pdfPageView(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(pdfPageView()->property("status").toInt(), 1, 10000);
        QTest::qWait(300);
    }

    void createPanelScene(const QString &type, bool overPdf = false) {
        createPdfScene(); QVERIFY(view); QVERIFY(pdfPageView());
        auto pdfViewer = visualChild(view.get(), "pdfViewer");
        pdfViewer->setProperty("contentLocked", true);
        pdfViewer->setVisible(overPdf);
        const QMap<QString, QByteArray> panels = {
            {"launcher", R"(
                DsContentLauncher {
                    id: overlay; objectName: "panel"
                    x: 100; y: 20; viewerWidth: 400; viewerHeight: 400
                    stage: null; model: adapter; glassEnabled: false
                    Component.onCompleted: { x = 100; y = 20 }
                    onOpenRequested: host.actions++
                }
            )"},
            {"presentation", R"(
                PresentationController {
                    id: overlay; objectName: "panel"
                    _placed: true; x: 150; y: 220; shown: true
                    playlist: playlistStub
                    onCloseRequested: { host.actions++; shown = false }
                }
            )"},
            {"fullscreen", R"(
                FullscreenController {
                    id: overlay; objectName: "panel"
                    _placed: true; x: 100; y: 220; shown: true
                    viewer: host.pdfViewer
                }
            )"},
            {"keyboard", R"(
                DsFloatingKeyboard {
                    id: overlay; objectName: "panel"
                    x: 100; y: 100; kbWidth: 400
                    onCloseRequested: { host.actions++; visible = false }
                }
            )"},
            {"viewer", R"(
                TitledMediaViewer {
                    id: overlay; objectName: "panel"
                    x: 160; y: 140; viewerWidth: 300; viewerHeight: 200
                    minWidth: 50; minHeight: 50; selected: true; controlsIdleMs: 0
                    glassEnabled: false; pinchEnabled: true
                    model: ({title:"Overlay",media:{type:"image",filepath:host.asset}})
                }
            )"},
            {"menu", R"(
                DsQuickMenu {
                    id: overlay; objectName: "panel"
                    x: 320; y: 220; innerRadius: 60; outerRadius: 120
                    revealDuration: 0; segmentStagger: 0; closeTimer: 0
                    model: [{iconText:"Open"},{iconText:"Unavailable",available:false}]
                    state: "On"
                    onItemSelected: host.actions++
                }
            )"},
            {"whiteboard", R"(
                DsWhiteboard {
                    id: overlay; objectName: "panel"
                    x: 100; y: 60; width: 440; height: 340; solidBackground: false
                    controls: [WhiteboardControls {}]
                }
            )"},
            {"fullscreen-stage", R"(
                DsWaffleStage {
                    id: overlay; objectName: "panel"
                    anchors.fill: parent; launcher: null; glassEnabled: false
                    fullscreenMargin: 150; fullscreenDuration: 0
                    viewerEnterAnimation: 0; viewerExitAnimation: 0
                    Component.onCompleted: Qt.callLater(() => {
                        overlay.openViewer({stage:overlay,model:{title:"Fullscreen",
                            media:{type:"image",filepath:host.asset}}});
                        overlay.setFullscreen(overlay._selectedViewer, true);
                    })
                }
            )"},
            {"launcher-button", R"(
                DsWaffleStage {
                    id: overlay; objectName: "panel"
                    anchors.fill: parent; glassEnabled: false
                    launcher: Component { DsContentLauncher { shown: false; glassEnabled: false } }
                    launcherModel: adapter
                }
            )"}
        };
        QVERIFY(panels.contains(type));
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Dsqt.Core
            import Dsqt.Waffles
            Item {
                id: host; objectName: "panelHost"
                anchors.fill: parent
                required property url asset
                required property Item pdfViewer
                property int actions: 0
                QtObject {
                    id: playlistStub; objectName: "playlistStub"
                    property int index: 1
                    property int count: 3
                    property bool paused: false
                    function prev() { index--; host.actions++ }
                    function next() { index++; host.actions++ }
                }
                QtObject {
                    id: adapter; objectName: "launcherAdapter"
                    property var displayedItems: Array.from({length:20}, (_, i) => ({
                        uid:"item"+i,title:"Item "+i,kind:"image",hasChildren:false,
                        thumbnail:"",media:{type:"image",filepath:host.asset}}))
                    property var currentPlaylist: null
                    property var path: []
                    property bool atRoot: true
                    property string searchQuery: ""
                    property var searchKinds: []
                    property var searchTypeUids: []
                    property var searchResults: displayedItems
                    function enter(item) { host.actions++ }
                    function back() { host.actions++ }
                    function setSearchFilter(filter) { searchKinds = filter.kinds; searchTypeUids = filter.typeUids }
                }
        )" + panels.value(type) + "\n}", QUrl("qrc:/panel-input-regression.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        auto host = qobject_cast<QQuickItem *>(component.createWithInitialProperties({
            {"asset", QUrl::fromLocalFile(QStringLiteral(DSQT_TEST_ASSET_DIR) + "/media/earth.gif")},
            {"pdfViewer", QVariant::fromValue(pdfViewer)}
        }));
        QVERIFY2(host, qPrintable(component.errorString()));
        host->setParent(view.get());
        host->setParentItem(view.get());
        QTest::qWait(300);
    }

    QQuickItem *itemWithProperty(QQuickItem *parent, const char *name, const QVariant &value) {
        for (auto child : parent->childItems()) {
            if (child->isVisible() && child->property(name) == value) return child;
            if (auto found = itemWithProperty(child, name, value)) return found;
        }
        return nullptr;
    }
    QPoint centerOf(QQuickItem *item) {
        return item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
    }

    QObject *hotspotHover() {
        auto hotspot = visualChild(view.get(), "hotspot:link");
        if (hotspot)
            for (auto child : hotspot->children())
                if (child->property("hovered").isValid()) return child;
        return nullptr;
    }

    void createStagePdfScene(bool pdfCreatedLast, bool locked, bool pdfForeground = false) {
        createView("DsWaffleStage", R"(({launcher:null,launcherButtonVisible:false,
            glassEnabled:false,controlsIdleMs:0,viewerEnterAnimation:0,viewerExitAnimation:0}))");
        QVERIFY(view);
        QQmlComponent backdropComponent(&engine);
        backdropComponent.loadFromModule("Dsqt.Waffles", "DsCustomLayoutMedia");
        QVERIFY2(backdropComponent.isReady(), qPrintable(backdropComponent.errorString()));
        auto backdrop = qobject_cast<QQuickItem *>(backdropComponent.createWithInitialProperties({
            {"objectName", "hotspotBackdrop"}, {"width", 640}, {"height", 480},
            {"item", evaluate(R"(({uid:"backdrop",media:{type:"image",filepath:imageUrl},
                hotspots:[{uid:"link",x:0,y:0,w:1,h:1,target:"destination"}]}))").toVariant()}
        }));
        QVERIFY(backdrop);
        backdrop->setParent(view.get());
        backdrop->setParentItem(qobject_cast<QQuickItem *>(evaluate("view.getLayer('presentation')").toQObject()));
        engine.globalObject().setProperty("pdfUrl", QUrl::fromLocalFile(
            QStringLiteral(DSQT_TEST_ASSET_DIR) + "/media/sample.pdf").toString());
        engine.globalObject().setProperty("pdfCreatedLast", pdfCreatedLast);
        engine.globalObject().setProperty("pdfLocked", locked);
        engine.globalObject().setProperty("pdfForeground", pdfForeground);
        evaluate(R"(
            {
                const common = {stage:view, minWidth:50, minHeight:50,
                    mediaFillMode:1, pinchEnabled:true};
                const image = Object.assign({}, common, {objectName:"viewerOverlay",
                    x:160,y:140,viewerWidth:300,viewerHeight:200,contentLocked:pdfForeground,
                    model:{title:"Foreground",media:{type:pdfForeground ? "pdf" : "image",
                        filepath:pdfForeground ? pdfUrl : imageUrl}}});
                const pdf = Object.assign({}, common, {objectName:"pdfViewer",
                    x:80,y:90,viewerWidth:480,viewerHeight:340,contentLocked:pdfLocked,
                    model:{title:"PDF",media:{type:"pdf",filepath:pdfUrl}}});
                view.openViewer(pdfCreatedLast ? image : pdf);
                view.openViewer(pdfCreatedLast ? pdf : image);
                view.selectViewer(view.getLayer("viewer").children.find(
                    child => child.objectName === "viewerOverlay"));
            }
        )");
        QTRY_VERIFY_WITH_TIMEOUT(pdfPageView(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(pdfPageView()->property("status").toInt(), 1, 10000);
        if (pdfForeground) {
            QTRY_VERIFY_WITH_TIMEOUT(pdfPageView("viewerOverlay"), 10000);
            QTRY_COMPARE_WITH_TIMEOUT(pdfPageView("viewerOverlay")->property("status").toInt(), 1, 10000);
        }
        QTest::qWait(300);
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

    void dragAt(const QPoint &position, const QPoint &delta, bool touch) {
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        if (touch) sequence.press(0, position, &window).commit();
        else QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, position);
        for (int step = 1; step <= 4; ++step) {
            const QPoint next = position + delta * step / 4;
            if (touch) {
                sequence.move(0, next, &window).commit();
                QTest::qWait(20);
            } else QTest::mouseMove(&window, next, 20);
        }
        if (touch) sequence.release(0, position + delta, &window).commit();
        else QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, position + delta);
        QCoreApplication::processEvents();
    }

    void pinchAt(const QPoint &center) {
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        sequence.press(0, center - QPoint(20, 0), &window)
                .press(1, center + QPoint(20, 0), &window).commit();
        for (int offset : {30, 40, 50, 60}) {
            sequence.move(0, center - QPoint(offset, 0), &window)
                    .move(1, center + QPoint(offset, 0), &window).commit();
            QTest::qWait(20);
        }
        sequence.release(0, center - QPoint(60, 0), &window)
                .release(1, center + QPoint(60, 0), &window).commit();
        QCoreApplication::processEvents();
    }

    void wheelAt(const QPoint &position, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QTest::mouseMove(&window, position, 20);
        QWheelEvent event(position, window.mapToGlobal(position), QPoint(), QPoint(0, -120),
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        // Keep synthetic wheel time after preceding QTest mouse events.
        event.setTimestamp(QTest::lastMouseTimestamp += 20);
        QCoreApplication::sendEvent(&window, &event);
        QTest::qWait(250);
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
        engine.addUrlInterceptor(&assetUrls);
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
        // Virtual keyboards share the window's input context across test fixtures.
        window.contentItem()->forceActiveFocus();
        QGuiApplication::inputMethod()->hide();
        QCoreApplication::processEvents();
        engine.globalObject().setProperty("view", QJSValue());
        view.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        window.resize(640, 480);
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
    void overlaysBlockUnderlyingHotspots_data() {
        QTest::addColumn<bool>("menu");
        QTest::addColumn<bool>("touch");
        QTest::addColumn<QPoint>("position");
        QTest::addColumn<int>("actions");
        QTest::addColumn<int>("underneath");
        for (bool touch : {false, true}) {
            const QByteArray suffix = touch ? "-touch" : "-mouse";
            QTest::newRow(("menu-action" + suffix).constData()) << true << touch << QPoint(410, 240) << 1 << 0;
            QTest::newRow(("menu-disabled" + suffix).constData()) << true << touch << QPoint(230, 240) << 0 << 0;
            QTest::newRow(("menu-center" + suffix).constData()) << true << touch << QPoint(350, 240) << 0 << 0;
            QTest::newRow(("outside-menu" + suffix).constData()) << true << touch << QPoint(470, 240) << 0 << 1;
            QTest::newRow(("menu-corner" + suffix).constData()) << true << touch << QPoint(430, 350) << 0 << 1;
            QTest::newRow(("viewer-body" + suffix).constData()) << false << touch << QPoint(260, 250) << 0 << 0;
            QTest::newRow(("viewer-title" + suffix).constData()) << false << touch << QPoint(200, 118) << 0 << 0;
            QTest::newRow(("viewer-button" + suffix).constData()) << false << touch << QPoint(440, 118) << 1 << 0;
            QTest::newRow(("outside-viewer" + suffix).constData()) << false << touch << QPoint(100, 250) << 0 << 1;
        }
    }
    void overlaysBlockUnderlyingHotspots() {
        QFETCH(bool, menu); QFETCH(bool, touch); QFETCH(QPoint, position);
        QFETCH(int, actions); QFETCH(int, underneath);
        createInputScene(menu); QVERIFY(view);
        clickAt(position, touch);
        QCOMPARE(view->property("actions").toInt(), actions);
        QCOMPARE(view->property("underneath").toInt(), underneath);
    }






















    void menuCloseDoesNotClickThrough_data() {
        QTest::addColumn<bool>("touch");
        QTest::newRow("mouse") << false;
        QTest::newRow("touch") << true;
    }
    void menuCloseDoesNotClickThrough() {
        QFETCH(bool, touch);
        createInputScene(true); QVERIFY(view);
        auto overlay = visualChild(view.get(), "overlay"); QVERIFY(overlay);
        auto close = visualChild(overlay, "quickMenuCloseButton"); QVERIFY(close);
        QVERIFY(close->width() > 0 && close->height() > 0);
        clickAt(close->mapToScene(QPointF(close->width()/2, close->height()/2)).toPoint(), touch);
        QCOMPARE(overlay->property("state").toString(), "Off");
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void overlaysBlockMultipleTouches_data() {
        QTest::addColumn<bool>("menu");
        QTest::newRow("menu-center") << true;
        QTest::newRow("viewer-body") << false;
    }
    void overlaysBlockMultipleTouches() {
        QFETCH(bool, menu);
        createInputScene(menu); QVERIFY(view);
        const QPoint first = menu ? QPoint(290, 240) : QPoint(240, 220);
        const QPoint second = menu ? QPoint(350, 240) : QPoint(340, 260);
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        sequence.press(0, first, &window).press(1, second, &window).commit();
        sequence.release(0, first, &window).stationary(1).commit();
        sequence.release(1, second, &window).commit();
        QCoreApplication::processEvents();
        QCOMPARE(view->property("underneath").toInt(), 0);
        QCOMPARE(view->property("actions").toInt(), 0);
    }
    void viewerStillDrags_data() {
        QTest::addColumn<bool>("touch");
        QTest::addColumn<QPoint>("position");
        for (bool touch : {false, true}) {
            const QByteArray suffix = touch ? "-touch" : "-mouse";
            QTest::newRow(("body" + suffix).constData()) << touch << QPoint(260, 250);
            QTest::newRow(("title" + suffix).constData()) << touch << QPoint(200, 118);
        }
    }
    void viewerStillDrags() {
        QFETCH(bool, touch); QFETCH(QPoint, position);
        createInputScene(false); QVERIFY(view);
        auto overlay = visualChild(view.get(), "overlay"); QVERIFY(overlay);
        const auto initial = overlay->position();
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        if (touch) sequence.press(0, position, &window).commit();
        else QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, position);
        for (int offset : {20, 40, 70}) {
            const QPoint next = position + QPoint(offset, 0);
            if (touch) {
                sequence.move(0, next, &window).commit();
                QTest::qWait(20);
            } else QTest::mouseMove(&window, next, 20);
        }
        if (touch) sequence.release(0, position + QPoint(70, 0), &window).commit();
        else QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, position + QPoint(70, 0));
        QVERIFY(overlay->x() > initial.x() + 20);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void viewerStillPinches() {
        createInputScene(false); QVERIFY(view);
        auto overlay = visualChild(view.get(), "overlay"); QVERIFY(overlay);
        const auto initial = overlay->property("viewerWidth").toReal();
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        sequence.press(0, QPoint(260, 250), &window).press(1, QPoint(340, 250), &window).commit();
        for (int offset : {20, 40, 60}) {
            sequence.move(0, QPoint(260 - offset, 250), &window)
                    .move(1, QPoint(340 + offset, 250), &window).commit();
            QTest::qWait(20);
        }
        sequence.release(0, QPoint(200, 250), &window).release(1, QPoint(400, 250), &window).commit();
        QVERIFY(overlay->property("viewerWidth").toReal() > initial);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void pdfTapRequiresLock_data() {
        QTest::addColumn<bool>("locked");
        QTest::addColumn<bool>("touch");
        for (bool locked : {false, true})
            for (bool touch : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2").arg(locked ? "locked" : "unlocked",
                    touch ? "touch" : "mouse"))) << locked << touch;
    }
    void pdfTapRequiresLock() {
        QFETCH(bool, locked); QFETCH(bool, touch);
        createPdfScene(); QVERIFY(view); QVERIFY(pdfPageView());
        auto viewer = visualChild(view.get(), "pdfViewer");
        viewer->setProperty("contentLocked", locked);
        auto pdf = pdfPageView();
        QVERIFY(QMetaObject::invokeMethod(pdf, "selectAll"));
        QTRY_VERIFY(!pdf->property("selectedText").toString().isEmpty());
        const auto selected = pdf->property("selectedText").toString();
        clickAt(QPoint(320, 220), touch);
        if (locked) QTRY_VERIFY_WITH_TIMEOUT(pdf->property("selectedText").toString() != selected, 1000);
        else QCOMPARE(pdf->property("selectedText").toString(), selected);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void pdfLinksRequireLock_data() { pdfTapRequiresLock_data(); }
    void pdfLinksRequireLock() {
        QFETCH(bool, locked); QFETCH(bool, touch);
        // Two local pages with an internal link: test real PDF link dispatch without
        // opening a browser or depending on annotations in an external fixture.
        const auto stream = [](const QByteArray &text) {
            const QByteArray content = "BT /F1 20 Tf 220 220 Td (" + text + ") Tj ET\n";
            return "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n"
                   + content + "endstream";
        };
        const QList<QByteArray> objects = {
            "<< /Type /Catalog /Pages 2 0 R >>",
            "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 400] /Contents 5 0 R "
                "/Resources << /Font << /F1 7 0 R >> >> /Annots [8 0 R] >>",
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 400] /Contents 6 0 R "
                "/Resources << /Font << /F1 7 0 R >> >> >>",
            stream("Go to page two"), stream("Second page"),
            "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
            "<< /Type /Annot /Subtype /Link /Rect [220 200 380 240] /Border [0 0 0] "
                "/Dest [4 0 R /Fit] >>"
        };
        QByteArray document = "%PDF-1.4\n";
        QList<qsizetype> offsets;
        for (const auto &object : objects) {
            offsets.append(document.size());
            document += QByteArray::number(offsets.size()) + " 0 obj\n" + object + "\nendobj\n";
        }
        const auto xref = document.size();
        document += "xref\n0 9\n0000000000 65535 f \n";
        for (auto offset : offsets)
            document += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
        document += "trailer\n<< /Size 9 /Root 1 0 R >>\nstartxref\n"
                    + QByteArray::number(xref) + "\n%%EOF\n";
        QTemporaryFile file;
        QVERIFY(file.open());
        QCOMPARE(file.write(document), document.size());
        QVERIFY(file.flush());
        createPdfScene({}, QUrl::fromLocalFile(file.fileName())); QVERIFY(view); QVERIFY(pdfPageView());
        visualChild(view.get(), "pdfViewer")->setProperty("contentLocked", locked);
        QCOMPARE(pdfPageView()->property("currentPage").toInt(), 0);
        clickAt(QPoint(320, 240), touch);
        QTRY_COMPARE_WITH_TIMEOUT(pdfPageView()->property("currentPage").toInt(), locked ? 1 : 0, 1000);
        QCOMPARE(view->property("underneath").toInt(), 0);
        view.reset(); // Release the PDF before its temporary file is removed on Windows.
    }

    void pdfLockControlsDragging_data() {
        QTest::addColumn<bool>("locked");
        QTest::addColumn<bool>("touch");
        QTest::addColumn<bool>("title");
        for (bool locked : {false, true})
            for (bool touch : {false, true})
                for (bool title : {false, true})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(locked ? "locked" : "unlocked",
                        touch ? "touch" : "mouse", title ? "title" : "body"))) << locked << touch << title;
    }
    void pdfLockControlsDragging() {
        QFETCH(bool, locked); QFETCH(bool, touch); QFETCH(bool, title);
        createPdfScene(); QVERIFY(view); QVERIFY(pdfPageView());
        auto viewer = visualChild(view.get(), "pdfViewer");
        viewer->setProperty("contentLocked", locked);
        const auto initial = viewer->position();
        dragAt(title ? QPoint(220, 70) : QPoint(320, 220), QPoint(80, 0), touch);
        if (locked) QCOMPARE(viewer->position(), initial);
        else QVERIFY(viewer->x() > initial.x() + 20);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void pdfLockControlsPinching_data() {
        QTest::addColumn<bool>("locked");
        QTest::newRow("unlocked") << false;
        QTest::newRow("locked") << true;
    }
    void pdfLockControlsPinching() {
        QFETCH(bool, locked);
        createPdfScene(); QVERIFY(view); QVERIFY(pdfPageView());
        auto viewer = visualChild(view.get(), "pdfViewer");
        viewer->setProperty("contentLocked", locked);
        auto pdf = pdfPageView();
        const auto initialPosition = viewer->position();
        const auto initialWidth = viewer->property("viewerWidth").toReal();
        const auto initialScale = pdf->property("renderScale").toReal();
        pinchAt(QPoint(320, 220));
        if (locked) {
            QCOMPARE(viewer->position(), initialPosition);
            QCOMPARE(viewer->property("viewerWidth").toReal(), initialWidth);
            QTRY_VERIFY(pdf->property("renderScale").toReal() > initialScale);
        } else {
            QVERIFY(viewer->property("viewerWidth").toReal() > initialWidth);
        }
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void pdfLockControlsWheel_data() {
        QTest::addColumn<bool>("locked");
        QTest::addColumn<bool>("alt");
        for (bool locked : {false, true})
            for (bool alt : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2").arg(locked ? "locked" : "unlocked",
                    alt ? "alt-wheel" : "wheel"))) << locked << alt;
    }
    void pdfLockControlsWheel() {
        QFETCH(bool, locked); QFETCH(bool, alt);
        createPdfScene(); QVERIFY(view); QVERIFY(pdfPageView());
        auto viewer = visualChild(view.get(), "pdfViewer");
        viewer->setProperty("contentLocked", locked);
        auto pdf = pdfPageView();
        pdf->setProperty("renderScale", 2.0); // Make scrolling possible in either lock state.
        QTest::qWait(100);
        const auto initialWidth = viewer->property("viewerWidth").toReal();
        const auto initialY = pdf->property("contentY").toReal();
        wheelAt(QPoint(320, 220), alt ? Qt::AltModifier : Qt::NoModifier);
        if (locked) {
            QCOMPARE(viewer->property("viewerWidth").toReal(), initialWidth);
            QVERIFY(pdf->property("contentY").toReal() > initialY);
        } else if (alt) {
            QVERIFY(viewer->property("viewerWidth").toReal() < initialWidth);
        } else {
            QCOMPARE(viewer->property("viewerWidth").toReal(), initialWidth);
            QCOMPARE(pdf->property("contentY").toReal(), initialY);
        }
        QCOMPARE(view->property("underneathWheels").toInt(), 0);
    }
    void overlaysBlockPdfTaps_data() {
        QTest::addColumn<QString>("overlayType");
        QTest::addColumn<bool>("touch");
        for (const QString &type : {"menu", "viewer"})
            for (bool touch : {false, true})
                QTest::newRow(qPrintable(type + (touch ? "-touch" : "-mouse"))) << type << touch;
    }
    void overlaysBlockPdfTaps() {
        QFETCH(QString, overlayType); QFETCH(bool, touch);
        createPdfScene(overlayType); QVERIFY(view); QVERIFY(pdfPageView());
        visualChild(view.get(), "pdfViewer")->setProperty("contentLocked", true);
        auto pdf = pdfPageView();
        QVERIFY(QMetaObject::invokeMethod(pdf, "selectAll"));
        QTRY_VERIFY(!pdf->property("selectedText").toString().isEmpty());
        const auto selected = pdf->property("selectedText").toString();
        clickAt(QPoint(340, 220), touch);
        QCOMPARE(pdf->property("selectedText").toString(), selected);
        QCOMPARE(view->property("underneath").toInt(), 0);
        // Positive control: the same point must reach the PDF once the overlay is hidden.
        visualChild(view.get(), overlayType + "Overlay")->setVisible(false);
        clickAt(QPoint(340, 220), touch);
        QTRY_VERIFY_WITH_TIMEOUT(pdf->property("selectedText").toString() != selected, 1000);
    }
    void overlaysBlockPdfGestures_data() {
        QTest::addColumn<QString>("overlayType");
        QTest::newRow("menu") << QString("menu");
        QTest::newRow("viewer") << QString("viewer");
    }
    void overlaysBlockPdfGestures() {
        QFETCH(QString, overlayType);
        createPdfScene(overlayType); QVERIFY(view); QVERIFY(pdfPageView());
        visualChild(view.get(), "pdfViewer")->setProperty("contentLocked", true);
        auto pdf = pdfPageView();
        pdf->setProperty("renderScale", 2.0);
        QTest::qWait(100);
        const auto initialY = pdf->property("contentY").toReal();
        const auto initialScale = pdf->property("renderScale").toReal();
        wheelAt(QPoint(320, 220));
        QCOMPARE(pdf->property("contentY").toReal(), initialY);
        auto foreground = visualChild(view.get(), overlayType + "Overlay");
        const auto initialWidth = foreground->width();
        pinchAt(QPoint(320, 220));
        if (overlayType == "viewer") QVERIFY(foreground->width() > initialWidth);
        QCOMPARE(pdf->property("renderScale").toReal(), initialScale);
        QCOMPARE(pdf->property("contentY").toReal(), initialY);
        QCOMPARE(view->property("underneath").toInt(), 0);
        QCOMPARE(view->property("underneathWheels").toInt(), 0);
        visualChild(view.get(), overlayType + "Overlay")->setVisible(false);
        wheelAt(QPoint(320, 220));
        QVERIFY(pdf->property("contentY").toReal() > initialY);
    }

    void stageOverlapKeepsFrontViewer_data() {
        QTest::addColumn<bool>("pdfCreatedLast");
        QTest::addColumn<bool>("locked");
        QTest::addColumn<bool>("touch");
        QTest::addColumn<bool>("title");
        for (bool pdfCreatedLast : {false, true})
            for (bool locked : {false, true})
                for (bool touch : {false, true})
                    for (bool title : {false, true})
                        QTest::newRow(qPrintable(QString("%1-%2-%3-%4").arg(
                            pdfCreatedLast ? "pdf-newer" : "pdf-older", locked ? "locked" : "unlocked",
                            touch ? "touch" : "mouse", title ? "title" : "body")))
                            << pdfCreatedLast << locked << touch << title;
    }
    void stageOverlapKeepsFrontViewer() {
        QFETCH(bool, pdfCreatedLast); QFETCH(bool, locked); QFETCH(bool, touch); QFETCH(bool, title);
        createStagePdfScene(pdfCreatedLast, locked); QVERIFY(view); QVERIFY(pdfPageView());
        auto foreground = visualChild(view.get(), "viewerOverlay"); QVERIFY(foreground);
        auto background = visualChild(view.get(), "pdfViewer"); QVERIFY(background);
        QVERIFY(foreground->z() > background->z());
        QVERIFY(!background->property("selected").toBool());
        auto pdf = pdfPageView();
        QVERIFY(QMetaObject::invokeMethod(pdf, "selectAll"));
        QTRY_VERIFY(!pdf->property("selectedText").toString().isEmpty());
        const auto selectedText = pdf->property("selectedText").toString();
        const auto pdfZ = background->z();
        QSignalSpy underneath(visualChild(view.get(), "hotspotBackdrop"), SIGNAL(contentRequested(QVariant)));
        QVERIFY(underneath.isValid());
        const auto point = title ? QPoint(220, 118) : QPoint(320, 220);
        engine.globalObject().setProperty("hitPoint", engine.toScriptValue(window.mapToGlobal(point)));
        QCOMPARE(evaluate("view.viewerAt(hitPoint)").toQObject(), foreground);
        clickAt(point, touch);
        QCOMPARE(underneath.count(), 0);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), foreground);
        QCOMPARE(background->z(), pdfZ);
        QVERIFY(!background->property("selected").toBool());
        QCOMPARE(pdf->property("selectedText").toString(), selectedText);
    }
    void stageOverlappingPdfs_data() { stageOverlapKeepsFrontViewer_data(); }
    void stageOverlappingPdfs() {
        QFETCH(bool, pdfCreatedLast); QFETCH(bool, locked); QFETCH(bool, touch); QFETCH(bool, title);
        createStagePdfScene(pdfCreatedLast, locked, true); QVERIFY(view); QVERIFY(pdfPageView());
        auto foreground = visualChild(view.get(), "viewerOverlay"); QVERIFY(foreground);
        auto background = visualChild(view.get(), "pdfViewer"); QVERIFY(background);
        auto pdf = pdfPageView();
        QVERIFY(QMetaObject::invokeMethod(pdf, "selectAll"));
        QTRY_VERIFY(!pdf->property("selectedText").toString().isEmpty());
        const auto selectedText = pdf->property("selectedText").toString();
        clickAt(title ? QPoint(220, 118) : QPoint(320, 220), touch);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), foreground);
        QVERIFY(!background->property("selected").toBool());
        QCOMPARE(pdf->property("selectedText").toString(), selectedText);
    }

    void stageCanSelectExposedPdf_data() { pdfTapRequiresLock_data(); }
    void stageCanSelectExposedPdf() {
        QFETCH(bool, touch); QFETCH(bool, locked);
        createStagePdfScene(true, locked); QVERIFY(view); QVERIFY(pdfPageView());
        auto pdf = visualChild(view.get(), "pdfViewer");
        auto image = visualChild(view.get(), "viewerOverlay");
        QSignalSpy underneath(visualChild(view.get(), "hotspotBackdrop"), SIGNAL(contentRequested(QVariant)));
        QVERIFY(underneath.isValid());
        clickAt(QPoint(110, 240), touch);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), pdf);
        QVERIFY(pdf->z() > image->z());
        QCOMPARE(underneath.count(), 0);
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

    void stageIgnoresUnavailableViewers_data() {
        QTest::addColumn<bool>("touch");
        QTest::addColumn<QString>("property");
        for (bool touch : {false, true})
            for (const QString &property : {"visible", "enabled"})
                QTest::newRow(qPrintable(property + (touch ? "-touch" : "-mouse"))) << touch << property;
    }
    void stageIgnoresUnavailableViewers() {
        QFETCH(bool, touch); QFETCH(QString, property);
        createStagePdfScene(true, true); QVERIFY(view);
        auto pdf = visualChild(view.get(), "pdfViewer"); QVERIFY(pdf);
        auto image = visualChild(view.get(), "viewerOverlay"); QVERIFY(image);
        pdf->setZ(image->z() + 1);
        pdf->setProperty(qPrintable(property), false);
        QCOMPARE(evaluate("view.viewerAt(view.mapToGlobal(320,220))").toQObject(), image);
        clickAt(QPoint(320, 220), touch);
        QCOMPARE(qvariant_cast<QObject *>(view->property("_selectedViewer")), image);
        QVERIFY(!pdf->property("selected").toBool());
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

    void panelsBlockHover_data() {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QPoint>("position");
        QTest::newRow("launcher-header") << QString("launcher") << QPoint(120,35);
        QTest::newRow("launcher-row") << QString("launcher") << QPoint(280,220);
        QTest::newRow("launcher-close") << QString("launcher") << QPoint(300,430);
        QTest::newRow("presentation-bar") << QString("presentation") << QPoint(300,220);
        QTest::newRow("presentation-tab") << QString("presentation") << QPoint(150,220);
        QTest::newRow("fullscreen-title") << QString("fullscreen") << QPoint(320,190);
        QTest::newRow("fullscreen-tab") << QString("fullscreen") << QPoint(100,220);
        QTest::newRow("keyboard-header") << QString("keyboard") << QPoint(180,120);
        QTest::newRow("keyboard-close") << QString("keyboard") << QPoint(474,122);
        QTest::newRow("viewer-body") << QString("viewer") << QPoint(320,220);
        QTest::newRow("viewer-title") << QString("viewer") << QPoint(220,118);
        QTest::newRow("menu") << QString("menu") << QPoint(340,220);
        QTest::newRow("whiteboard") << QString("whiteboard") << QPoint(320,200);
        QTest::newRow("whiteboard-toolbar") << QString("whiteboard") << QPoint(320,230);
        QTest::newRow("launcher-button") << QString("launcher-button") << QPoint(88,392);
        QTest::newRow("fullscreen-backdrop") << QString("fullscreen-stage") << QPoint(110,240);
    }
    void panelsBlockHover() {
        QFETCH(QString, type); QFETCH(QPoint, position);
        createPanelScene(type); QVERIFY(view); QVERIFY(hotspotHover());
        QTest::mouseMove(&window, QPoint(2,2));
        QTest::mouseMove(&window, position, 20);
        QTest::qWait(30);
        QVERIFY(!hotspotHover()->property("hovered").toBool());
        visualChild(view.get(), "panelHost")->setVisible(false);
        QTest::mouseMove(&window, QPoint(2,2));
        QTest::mouseMove(&window, position, 20);
        QTRY_VERIFY(hotspotHover()->property("hovered").toBool());
    }
    void panelsBlockClicks_data() {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QPoint>("position");
        QTest::addColumn<bool>("touch");
        const QList<QPair<QString,QPoint>> targets = {
            {"launcher",{120,35}}, {"launcher",{280,220}}, {"launcher",{300,430}},
            {"presentation",{300,220}}, {"presentation",{150,220}},
            {"fullscreen",{320,190}}, {"fullscreen",{100,220}},
            {"keyboard",{180,120}}, {"keyboard",{474,122}},
            {"launcher-button",{88,392}}, {"whiteboard",{320,200}}, {"fullscreen-stage",{110,240}}
        };
        for (auto target : targets)
            for (bool touch : {false,true})
                QTest::newRow(qPrintable(QString("%1-%2-%3-%4").arg(target.first)
                    .arg(target.second.x()).arg(target.second.y()).arg(touch ? "touch" : "mouse")))
                    << target.first << target.second << touch;
    }
    void panelsBlockClicks() {
        QFETCH(QString, type); QFETCH(QPoint, position); QFETCH(bool, touch);
        createPanelScene(type); QVERIFY(view);
        clickAt(position, touch);
        QCOMPARE(view->property("underneath").toInt(), 0);
        visualChild(view.get(), "panelHost")->setVisible(false);
        clickAt(position, touch);
        QCOMPARE(view->property("underneath").toInt(), 1);
    }
    void panelsBlockPdfGestures_data() {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QPoint>("position");
        QTest::newRow("launcher") << QString("launcher") << QPoint(250,110);
        QTest::newRow("presentation") << QString("presentation") << QPoint(300,220);
        QTest::newRow("fullscreen") << QString("fullscreen") << QPoint(320,190);
        QTest::newRow("keyboard") << QString("keyboard") << QPoint(260,120);
        QTest::newRow("viewer-controls") << QString("viewer") << QPoint(320,118);
        QTest::newRow("whiteboard") << QString("whiteboard") << QPoint(320,200);
        QTest::newRow("whiteboard-toolbar") << QString("whiteboard") << QPoint(320,230);
        QTest::newRow("fullscreen-backdrop") << QString("fullscreen-stage") << QPoint(110,240);
    }
    void panelsBlockPdfGestures() {
        QFETCH(QString, type); QFETCH(QPoint, position);
        createPanelScene(type, true); QVERIFY(view); QVERIFY(pdfPageView());
        auto pdf = pdfPageView();
        pdf->setProperty("renderScale", 2.0);
        QTest::qWait(100);
        const auto scale = pdf->property("renderScale").toReal();
        const auto scroll = pdf->property("contentY").toReal();
        wheelAt(position);
        QCOMPARE(pdf->property("contentY").toReal(), scroll);
        pinchAt(position);
        QCOMPARE(pdf->property("renderScale").toReal(), scale);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }

    void panelsStillDrag_data() {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QPoint>("position");
        QTest::addColumn<bool>("touch");
        const QList<QPair<QString,QPoint>> targets = {
            {"launcher",{120,35}}, {"presentation",{300,220}},
            {"fullscreen",{320,190}}, {"keyboard",{180,120}}
        };
        for (auto target : targets)
            for (bool touch : {false,true})
                QTest::newRow(qPrintable(target.first + (touch ? "-touch" : "-mouse")))
                    << target.first << target.second << touch;
    }
    void panelsStillDrag() {
        QFETCH(QString, type); QFETCH(QPoint, position); QFETCH(bool, touch);
        createPanelScene(type); QVERIFY(view);
        auto panel = visualChild(view.get(), "panel"); QVERIFY(panel);
        const auto initial = panel->position();
        dragAt(position, QPoint(50,0), touch);
        QVERIFY(panel->x() > initial.x()+20);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void launcherStillOpensRowsAndScrolls_data() { stageSelectsCustomViewerByStackingOrder_data(); }
    void launcherStillOpensRowsAndScrolls() {
        QFETCH(bool, touch);
        createPanelScene("launcher"); QVERIFY(view);
        auto host = visualChild(view.get(), "panelHost"); QVERIFY(host);
        auto panel = visualChild(view.get(), "panel"); QVERIFY(panel);
        clickAt(QPoint(280,220), touch);
        QCOMPARE(host->property("actions").toInt(), 1);
        auto list = itemWithProperty(panel, "count", 20); QVERIFY(list);
        const auto y = list->property("contentY").toReal();
        const auto panelPosition = panel->position();
        if (touch) dragAt(QPoint(280,310), QPoint(0,-70), true);
        else wheelAt(QPoint(280,310));
        QTRY_VERIFY(list->property("contentY").toReal() > y);
        QCOMPARE(panel->position(), panelPosition);
        QCOMPARE(host->property("actions").toInt(), 1);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void launcherSearchStillWorks_data() { stageSelectsCustomViewerByStackingOrder_data(); }
    void launcherSearchStillWorks() {
        QFETCH(bool, touch);
        createPanelScene("launcher"); QVERIFY(view);
        window.resize(640,900); view->setHeight(900);
        auto panel = visualChild(view.get(), "panel"); QVERIFY(panel);
        panel->setProperty("viewerHeight", 762);
        auto tab = itemWithProperty(panel, "label", "Search"); QVERIFY(tab);
        clickAt(centerOf(tab), touch);
        QCOMPARE(panel->property("viewMode").toString(), "search");
        QTRY_VERIFY(visualChild(panel, "launcherSearchField"));
        auto field = visualChild(panel, "launcherSearchField");
        QTRY_VERIFY(field->hasActiveFocus());
        for (char key : QByteArray("test")) QTest::keyClick(&window, key);
        QCOMPARE(field->property("text").toString(), "test");
        auto adapter = visualChild(view.get(), "panelHost")->findChild<QObject *>("launcherAdapter");
        QVERIFY(adapter);
        QCOMPARE(adapter->property("searchQuery").toString(), "test");
        QTRY_VERIFY_WITH_TIMEOUT(itemWithProperty(panel, "key", int(Qt::Key_A)), 3000);
        QTest::qWait(300); // Wait for the docked keyboard's height animation.
        clickAt(centerOf(itemWithProperty(panel, "key", int(Qt::Key_A))), touch);
        QTRY_COMPARE(field->property("text").toString().toLower(), "testa");
        auto chip = itemWithProperty(panel, "label", "Images"); QVERIFY(chip);
        clickAt(centerOf(chip), touch);
        QCOMPARE(engine.newQObject(adapter).property("searchKinds").property(0).toString(), "image");
        const auto start = panel->position();
        dragAt(QPoint(120,35), QPoint(50,0), touch);
        QVERIFY(panel->x() > start.x()+20);
        QCOMPARE(view->property("underneath").toInt(), 0);
    }
    void controllerButtonsStillWork_data() { stageSelectsCustomViewerByStackingOrder_data(); }
    void controllerButtonsStillWork() {
        QFETCH(bool, touch);
        createPanelScene("presentation"); QVERIFY(view);
        auto panel = visualChild(view.get(), "panel"); QVERIFY(panel);
        auto button = itemWithProperty(panel, "icon", QUrl("qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/media/arrow_right.svg"));
        QVERIFY(button);
        clickAt(centerOf(button), touch);
        auto host = visualChild(view.get(), "panelHost");
        QCOMPARE(host->property("actions").toInt(), 1);
        QCOMPARE(host->findChild<QObject *>("playlistStub")->property("index").toInt(), 2);
        clickAt(QPoint(150,220), touch);
        QVERIFY(panel->property("collapsed").toBool());
        // The hidden bar must release its old footprint to the presentation.
        clickAt(QPoint(300,220), touch);
        QCOMPARE(view->property("underneath").toInt(), 1);
    }

    void launcherCloseButtonWorks_data() {
        QTest::addColumn<bool>("touch");
        QTest::addColumn<bool>("search");
        QTest::addColumn<int>("part");
        for (bool touch : {false, true})
            for (bool search : {false, true})
                for (int part : {-1, 0, 1})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(touch ? "touch" : "mouse")
                        .arg(search ? "search" : "content").arg(part))) << touch << search << part;
    }
    void launcherCloseButtonWorks() {
        QFETCH(bool, touch); QFETCH(bool, search); QFETCH(int, part);
        createPanelScene("launcher"); QVERIFY(view);
        auto panel = visualChild(view.get(), "panel"); QVERIFY(panel);
        if (search) {
            auto tab = itemWithProperty(panel, "label", "Search"); QVERIFY(tab);
            clickAt(centerOf(tab), touch);
            QTRY_COMPARE(panel->property("viewMode").toString(), "search");
            QTest::qWait(300);
        }
        auto button = visualChild(panel, "launcherCloseButton"); QVERIFY(button);
        clickAt(centerOf(button) + QPoint(0, part * 10), touch);
        QVERIFY(!panel->property("shown").toBool());
        QTRY_VERIFY(!panel->isVisible());
        QCOMPARE(view->property("underneath").toInt(), 0);
        panel->setProperty("shown", true);
        QTRY_VERIFY(panel->isVisible());
    }




    void whiteboardStillDraws_data() { stageSelectsCustomViewerByStackingOrder_data(); }
    void whiteboardStillDraws() {
        QFETCH(bool, touch);
        createPanelScene("whiteboard"); QVERIFY(view);
        auto panel = visualChild(view.get(), "panel"); QVERIFY(panel);
        dragAt(QPoint(320,150), QPoint(50,30), touch);
        QVERIFY(panel->property("hasDrawing").toBool());
        QVERIFY(panel->property("canUndo").toBool());
        QVERIFY(!panel->property("drawing").toBool());
        QCOMPARE(view->property("underneath").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(panel, "undo"));
        QVERIFY(!panel->property("hasDrawing").toBool());
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
