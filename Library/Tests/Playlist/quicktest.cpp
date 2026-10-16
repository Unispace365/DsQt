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
#include <ui/dsQmlClusterManager.h>
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
    bool pdfAvailable = false;
    QString pdfUnavailableReason;

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

    void createAmbientWakeScene() {
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Dsqt.Core
            import Dsqt.Waffles
            DsWaffleStage {
                id: stage
                width: 640; height: 480
                required property var inputWindow
                required property url asset
                launcher: null; launcherButtonVisible: false; glassEnabled: false
                playlistFadeMs: 20
                property int actions: 0
                property int hotspots: 0
                property int mousePresses: 0
                property int touchPresses: 0
                property int wheels: 0
                DsIdle {
                    areaItemTarget: stage.inputWindow
                    idleTimeout: 60000
                    onIdleEnded: stage.hideAmbient()
                }
                // Exercise a retained foreground above the normal viewer/modal layers.
                Item {
                    anchors.fill: parent; z: 150
                    MouseArea {
                        anchors.fill: parent; acceptedButtons: Qt.AllButtons
                        onPressed: stage.mousePresses++
                        onWheel: stage.wheels++
                    }
                    MultiPointTouchArea {
                        anchors.fill: parent; mouseEnabled: false
                        onPressed: points => stage.touchPresses += points.length
                    }
                    DsCustomLayoutMedia {
                        anchors.fill: parent
                        item: ({uid: "background", media: {type: "image", filepath: stage.asset},
                            hotspots: [{uid: "link", x: 0, y: 0, w: 0.5, h: 1, target: "destination"}]})
                        onContentRequested: stage.hotspots++
                    }
                    MouseArea {
                        x: 400; y: 160; width: 160; height: 160
                        acceptedButtons: Qt.AllButtons
                        onPressed: stage.mousePresses++
                        onClicked: stage.actions++
                    }
                }
            }
        )", QUrl("qrc:/ambient-wake-regression.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties({
            {"inputWindow", QVariant::fromValue(&window)},
            {"asset", QUrl::fromLocalFile(QStringLiteral(DSQT_TEST_ASSET_DIR) + "/media/earth.gif")}
        })));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        engine.globalObject().setProperty("view", engine.newQObject(view.get()));
        evaluate("view.showAmbient({ambient:true,slides:[{uid:'ambient'}]})");
        QVERIFY(view->property("ambientShown").toBool());
        QTest::qWait(100);
    }

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

    void createMediaScene(const QString &overlayType = {}, const QUrl &pdfUrl = {}, bool usePdf = true) {
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
                required property bool usePdf
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
                    model: ({title: "Background", media: {type: scene.usePdf ? "pdf" : "image",
                        filepath: scene.usePdf ? scene.pdfUrl : scene.imageUrl}})
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
            {"overlayType", overlayType}, {"usePdf", usePdf}
        })));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        if (usePdf) {
            QTRY_VERIFY_WITH_TIMEOUT(pdfPageView(), 10000);
            QTRY_COMPARE_WITH_TIMEOUT(pdfPageView()->property("status").toInt(), 1, 10000);
        }
        QTest::qWait(300);
    }

    void createPanelScene(const QString &type, bool overPdf = false) {
        createMediaScene({}, {}, overPdf); QVERIFY(view);
        auto mediaViewer = visualChild(view.get(), "pdfViewer"); QVERIFY(mediaViewer);
        mediaViewer->setProperty("contentLocked", true);
        mediaViewer->setVisible(overPdf);
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
                    viewer: host.mediaViewer
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
                required property Item mediaViewer
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
            {"mediaViewer", QVariant::fromValue(mediaViewer)}
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

    void createStageScene(bool pdfCreatedLast, bool locked, bool pdfForeground = false,
                          const QString &backgroundType = "pdf") {
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
        engine.globalObject().setProperty("backgroundType", backgroundType);
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
                    model:{title:"Background",media:{type:backgroundType,
                        filepath:backgroundType === "pdf" ? pdfUrl : imageUrl}}});
                view.openViewer(pdfCreatedLast ? image : pdf);
                view.openViewer(pdfCreatedLast ? pdf : image);
                view.selectViewer(view.getLayer("viewer").children.find(
                    child => child.objectName === "viewerOverlay"));
            }
        )");
        if (backgroundType == "pdf") {
            QTRY_VERIFY_WITH_TIMEOUT(pdfPageView(), 10000);
            QTRY_COMPARE_WITH_TIMEOUT(pdfPageView()->property("status").toInt(), 1, 10000);
        }
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

    void createClusterScene(const QString &receiver) {
        QQmlComponent component(&engine);
        const QByteArray input = receiver == "button" ? R"(
            Button { anchors.fill: parent; onClicked: scene.actions++ }
        )" : receiver == "handler" ? R"(
            TapHandler { onTapped: scene.actions++ }
        )" : R"(
            MouseArea { anchors.fill: parent; onClicked: scene.actions++ }
        )";
        component.setData(R"(
            import QtQuick
            import QtQuick.Controls
            import Dsqt.Core
            Item {
                id: scene
                width: 640; height: 480
                property int actions: 0
                property int menus: 0
                property int releases: 0
                Item {
                    width: 640; height: 480
        )" + input + R"(
                }
                DsClusterManager {
                    id: clusterInput
                    objectName: "clusterInput"
                    anchors.fill: parent
                    minClusterTouchCount: 5
                    minClusterSeperation: 150
                    boundingBoxSize: 200
                    triggerTime: 0.01
                    onTriggerableChanged: (ready, cluster) => { if (ready) scene.menus++; }
                    onTriggered: cluster => scene.releases++
                }
            }
        )", QUrl("qrc:/cluster-input-regression.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.create()));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        QTest::qWait(50);
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


    void createQuickMenuHostScene(bool customVisual = false) {
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Dsqt.Core
            Item {
                id: scene
                width: 640; height: 480
                property bool customVisual: false
                property int actions: 0
                property int underneath: 0
                property int timeout: 0
                property point selectedPosition
                property alias host: host
                TapHandler { onTapped: scene.underneath++ }
                DsQuickMenuHost {
                    id: host
                    anchors.fill: parent
                    triggerTime: 0.01
                    model: [{iconText:"Open"}, {iconText:"Disabled", available:false}]
                    menuComponent: scene.customVisual ? alternate : standard
                    onItemSelected: (index, position) => {
                        scene.actions++;
                        scene.selectedPosition = position;
                    }
                }
                Component {
                    id: standard
                    DsQuickMenu {
                        objectName: "hostVisual"
                        innerRadius: 60; outerRadius: 120
                        closeTimer: scene.timeout
                        revealDuration: 0; segmentStagger: 0
                    }
                }
                Component {
                    id: alternate
                    Rectangle {
                        id: visual
                        objectName: "hostVisual"
                        property list<var> model
                        property var config
                        property color highlightColor
                        property real outerRadius: 70
                        signal itemSelected(int index, point position)
                        signal closed()
                        width: 140; height: 140; color: highlightColor
                        transform: Translate { x:-70; y:-70 }
                        visible: false
                        function openMenu(touch) { visible = true; }
                        function closeMenu() {
                            if (!visible) return;
                            visible = false;
                            Qt.callLater(visual.closed);
                        }
                        function updatePoint(point) {}
                        function releaseGesture() {}
                        MouseArea {
                            anchors.fill: parent
                            onClicked: visual.itemSelected(0, visual.mapToGlobal(width/2, height/2))
                        }
                    }
                }
            }
        )", QUrl("qrc:/menu-host-regression.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties({{"customVisual",customVisual}})));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        engine.globalObject().setProperty("view", engine.newQObject(view.get()));
        QTest::qWait(30);
    }

    void createWaffleMenuScene() {
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Dsqt.Core
            import Dsqt.Waffles
            Item {
                id: scene
                width: 640; height: 480
                property alias stage: stage
                property alias menu: menu
                property int customActions: 0
                property int ambientRequests: 0
                property bool customAvailable: true
                property point customPosition
                DsWaffleStage {
                    id: stage
                    anchors.fill: parent
                    glassEnabled: false; controlsIdleMs: 0
                    launcherButtonVisible: false
                    fullscreenDuration: 0
                    viewerAnimationDuration: 0
                    viewerEnterAnimation: 0; viewerExitAnimation: 0
                    launcher: Component {
                        Item {
                            property var stage
                            property var model
                            property string viewMode: "content"
                            property bool shown: false
                            width: 100; height: 60
                        }
                    }
                }
                DsWaffleQuickMenu {
                    id: menu
                    anchors.fill: parent
                    stage: scene.stage
                    ambientAvailable: true
                    actionOverrides: ({
                        help: {iconText:"Help",available:scene.customAvailable,
                            trigger: position => {scene.customActions++;scene.customPosition=position;}}
                    })
                    onAmbientRequested: scene.ambientRequests++
                }
            }
        )", QUrl("qrc:/waffle-menu-regression.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.create()));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(), QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        engine.globalObject().setProperty("view", engine.newQObject(view.get()));
        evaluate(R"(
            function menuAction(id) {
                const index=view.menu.actionController.entries.findIndex(entry=>entry.action===id);
                view.menu.actionController.trigger(index,view.stage.mapToGlobal(320,240));
            }
        )");
        QTest::qWait(30);
    }

private slots:
    void initTestCase() {
        engine.addUrlInterceptor(&assetUrls);
        // Probe once. CI requires a working PDF plugin; local kits may omit this optional module.
        QQmlComponent pdfModule(&engine);
        pdfModule.setData("import QtQuick.Pdf\nPdfDocument {}", QUrl());
        QTRY_VERIFY_WITH_TIMEOUT(pdfModule.status() != QQmlComponent::Loading, 10000);
        pdfAvailable = pdfModule.isReady();
        pdfUnavailableReason = "Qt Quick PDF is unavailable: " + pdfModule.errorString();
        // An installed but broken plugin is a setup failure, not an optional-feature skip.
        QVERIFY2(pdfAvailable || pdfModule.errorString().contains("module \"QtQuick.Pdf\" is not installed"),
                 qPrintable(pdfUnavailableReason));
        if (qEnvironmentVariableIntValue("DSQT_TEST_REQUIRE_PDF") != 0)
            QVERIFY2(pdfAvailable, qPrintable(pdfUnavailableReason));
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
    void ambientWakeConsumesGesture_data() {
        QTest::addColumn<bool>("touch");
        QTest::addColumn<bool>("hold");
        QTest::addColumn<bool>("hotspot");
        for (bool touch : {false, true})
            for (bool hold : {false, true})
                for (bool hotspot : {false, true}) {
                    const QByteArray name = QByteArray(touch ? "touch" : "mouse")
                        + (hold ? "-held" : "-tap") + (hotspot ? "-hotspot" : "-button");
                    QTest::newRow(name.constData()) << touch << hold << hotspot;
                }
    }
    void ambientWakeConsumesGesture() {
        QFETCH(bool, touch); QFETCH(bool, hold); QFETCH(bool, hotspot);
        createAmbientWakeScene(); QVERIFY(view);
        const QPoint position = hotspot ? QPoint(160, 240) : QPoint(480, 240);
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        if (touch) sequence.press(0, position, &window).commit();
        else QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, position);
        QTRY_VERIFY(view->property("_ambientClosing").toBool());
        // Always release the synthetic input, even if the held-gesture check fails.
        if (hold) QTest::qWait(200);
        const bool shieldRetained = view->property("ambientShown").toBool();
        if (touch) sequence.release(0, position, &window).commit();
        else QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, position);
        QVERIFY(shieldRetained);
        QTRY_VERIFY(!view->property("ambientShown").toBool());
        QCOMPARE(view->property("actions").toInt(), 0);
        QCOMPARE(view->property("hotspots").toInt(), 0);
        QCOMPARE(view->property("mousePresses").toInt(), 0);
        QCOMPARE(view->property("touchPresses").toInt(), 0);
        clickAt(position, touch);
        QCOMPARE(view->property(hotspot ? "hotspots" : "actions").toInt(), 1);
    }
    void ambientWakeHoldsUntilLastTouch() {
        createAmbientWakeScene(); QVERIFY(view);
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        sequence.press(0, QPoint(160, 240), &window).commit();
        QTest::qWait(200);
        sequence.stationary(0).press(1, QPoint(480, 240), &window).commit();
        sequence.release(0, QPoint(160, 240), &window).stationary(1).commit();
        QTest::qWait(30);
        const bool shieldRetained = view->property("ambientShown").toBool();
        sequence.move(1, QPoint(500, 260), &window).commit();
        sequence.release(1, QPoint(500, 260), &window).commit();
        QVERIFY(shieldRetained);
        QTRY_VERIFY(!view->property("ambientShown").toBool());
        QCOMPARE(view->property("actions").toInt(), 0);
        QCOMPARE(view->property("hotspots").toInt(), 0);
        QCOMPARE(view->property("touchPresses").toInt(), 0);
        clickAt(QPoint(480, 240), true);
        QCOMPARE(view->property("actions").toInt(), 1);
    }
    void ambientWakeCancellationReleasesShield() {
        createAmbientWakeScene(); QVERIFY(view);
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        sequence.press(0, QPoint(160, 240), &window).commit();
        QTest::qWait(200);
        QTouchEvent cancel(QEvent::TouchCancel, touchDevice);
        QCoreApplication::sendEvent(&window, &cancel);
        QTest::qWait(30);
        const bool shieldReleased = !view->property("ambientShown").toBool();
        sequence.release(0, QPoint(160, 240), &window).commit();
        QVERIFY(shieldReleased);
        QCOMPARE(view->property("hotspots").toInt(), 0);
        QCOMPARE(view->property("touchPresses").toInt(), 0);
        clickAt(QPoint(160, 240), true);
        QCOMPARE(view->property("hotspots").toInt(), 1);
    }
    void ambientWakeConsumesOtherMouseButtons_data() {
        QTest::addColumn<Qt::MouseButton>("button");
        QTest::newRow("right") << Qt::RightButton;
        QTest::newRow("middle") << Qt::MiddleButton;
    }
    void ambientWakeConsumesOtherMouseButtons() {
        QFETCH(Qt::MouseButton, button);
        createAmbientWakeScene(); QVERIFY(view);
        const QPoint position(480, 240);
        QTest::mousePress(&window, button, Qt::NoModifier, position);
        QTest::qWait(200);
        const bool shieldRetained = view->property("ambientShown").toBool();
        QTest::mouseRelease(&window, button, Qt::NoModifier, position);
        QVERIFY(shieldRetained);
        QTRY_VERIFY(!view->property("ambientShown").toBool());
        QCOMPARE(view->property("actions").toInt(), 0);
        QCOMPARE(view->property("mousePresses").toInt(), 0);
        QTest::mouseClick(&window, button, Qt::NoModifier, position);
        QCOMPARE(view->property("actions").toInt(), 1);
    }
    void ambientBlocksWheelInput() {
        createAmbientWakeScene(); QVERIFY(view);
        wheelAt(QPoint(600, 400));
        QCOMPARE(view->property("wheels").toInt(), 0);
        evaluate("view.hideAmbient()");
        QTRY_VERIFY(!view->property("ambientShown").toBool());
        wheelAt(QPoint(600, 400));
        QCOMPARE(view->property("wheels").toInt(), 1);
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

    void quickMenuHostLifecycle_data() {
        QTest::addColumn<bool>("customVisual");
        QTest::newRow("radial") << false;
        QTest::newRow("replacement") << true;
    }
    void quickMenuHostLifecycle() {
        QFETCH(bool, customVisual);
        createQuickMenuHostScene(customVisual); QVERIFY(view);
        QTest::mouseClick(&window,Qt::RightButton,Qt::NoModifier,QPoint(635,475));
        QTRY_VERIFY(visualChild(view.get(),"hostVisual"));
        auto menu=visualChild(view.get(),"hostVisual");
        const int radius=customVisual ? 70 : 120;
        const QPointF center=menu->mapToScene(QPointF(radius+(!customVisual),radius+(!customVisual)));
        QVERIFY(center.x() <= 640-radius-7);
        QVERIFY(center.y() <= 480-radius-7);
        evaluate("view.host.model=[{iconText:'Changed'}, {iconText:'Disabled',available:false}]");
        auto menuJs=engine.newQObject(menu);
        QCOMPARE(menuJs.property("model").property(0).property("iconText").toString(),QString("Changed"));
        const QPoint select=(center+QPointF(customVisual?0:85,0)).toPoint();
        clickAt(select,false);
        QCOMPARE(view->property("actions").toInt(),1);
        QCOMPARE(view->property("underneath").toInt(),0);
        QTRY_VERIFY(!visualChild(view.get(),"hostVisual"));
        // Global selection position can be mapped back into the application.
        const auto global=view->property("selectedPosition").toPointF();
        QVERIFY(qAbs(window.mapFromGlobal(global.toPoint()).x()-select.x())<2);
        evaluate("view.host.openAt(Qt.point(320,240))");
        QTRY_VERIFY(visualChild(view.get(),"hostVisual"));
        evaluate("view.host.enabled=false");
        QTRY_VERIFY(!visualChild(view.get(),"hostVisual"));
        clickAt(QPoint(320,240),false);
        QCOMPARE(view->property("underneath").toInt(),1);
        evaluate("view.host.enabled=true;view.host.rightClickEnabled=false");
        QTest::mouseClick(&window,Qt::RightButton,Qt::NoModifier,QPoint(320,240));
        QVERIFY(!visualChild(view.get(),"hostVisual"));
    }

    void quickMenuHostTimeout() {
        createQuickMenuHostScene(); QVERIFY(view);
        view->setProperty("timeout",60);
        evaluate("view.host.openAt(Qt.point(320,240))");
        QTRY_VERIFY(visualChild(view.get(),"hostVisual"));
        QTest::mouseMove(&window,QPoint(10,10));
        QTRY_VERIFY(!visualChild(view.get(),"hostVisual"));
    }

    void quickMenuHostTouchReopens_data() {
        QTest::addColumn<bool>("simulated");
        QTest::newRow("five-finger") << false;
        QTest::newRow("shift-mouse") << true;
    }
    void quickMenuHostTouchReopens() {
        QFETCH(bool,simulated);
        createQuickMenuHostScene(); QVERIFY(view);
        auto sequence=QTest::touchEvent(&window,touchDevice,false);
        const QPoint center(320,240);
        for(int cycle=0;cycle<3;++cycle) {
            if(simulated) {
                QTest::mousePress(&window,Qt::LeftButton,Qt::ShiftModifier,center);
                QTest::qWait(30);
                QTest::mouseMove(&window,center+QPoint(2,0));
            } else {
                for(int i=0;i<5;++i) sequence.press(i,center+QPoint((i-2)*10,0),&window);
                sequence.commit();
                QTest::qWait(30);
                for(int i=0;i<5;++i) sequence.move(i,center+QPoint((i-2)*10+2,0),&window);
                sequence.commit();
            }
            QTRY_VERIFY(visualChild(view.get(),"hostVisual"));
            auto menu=visualChild(view.get(),"hostVisual");
            evaluate("view.host.model=[{iconText:'Touch update'}, {iconText:'Disabled',available:false}]");
            QCOMPARE(engine.newQObject(menu).property("model").property(0).property("iconText").toString(),
                     QString("Touch update"));
            if(simulated) QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,center+QPoint(2,0));
            else {
                for(int i=4;i>=0;--i) {
                    for(int j=0;j<i;++j) sequence.stationary(j);
                    sequence.release(i,center+QPoint((i-2)*10+2,0),&window).commit();
                }
            }
            QCOMPARE(view->property("underneath").toInt(),0);
            QCOMPARE(view->property("actions").toInt(),0);
            clickAt(QPoint(405,240),false);
            QCOMPARE(view->property("actions").toInt(),1);
            QTRY_VERIFY(!visualChild(view.get(),"hostVisual"));
            view->setProperty("actions",0);
        }
    }


    void quickMenuCancellationNeverSelects_data() { quickMenuHostTouchReopens_data(); }
    void quickMenuCancellationNeverSelects() {
        QFETCH(bool,simulated);
        createQuickMenuHostScene(); QVERIFY(view);
        auto sequence=QTest::touchEvent(&window,touchDevice,false);
        const QPoint center(320,240);
        if(simulated) QTest::mousePress(&window,Qt::LeftButton,Qt::ShiftModifier,center);
        else {
            for(int i=0;i<5;++i) sequence.press(i,center+QPoint((i-2)*10,0),&window);
            sequence.commit();
        }
        QTest::qWait(30);
        if(simulated) QTest::mouseMove(&window,center+QPoint(90,0));
        else {
            for(int i=0;i<5;++i) sequence.move(i,center+QPoint((i-2)*10+90,0),&window);
            sequence.commit();
        }
        QTRY_VERIFY(visualChild(view.get(),"hostVisual"));
        engine.globalObject().setProperty("cancelMenu",engine.newQObject(visualChild(view.get(),"hostVisual")));
        evaluate("cancelMenu.updatePoint(Qt.point(cancelMenu.width/2+90,cancelMenu.height/2))");
        QCOMPARE(visualChild(view.get(),"hostVisual")->property("_clusterIndex").toInt(),0);
        if(simulated) {
            auto manager=view->findChild<dsqt::ui::DsQmlClusterManager *>();
            QVERIFY(manager); manager->ungrabMouse();
            QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,center+QPoint(90,0));
        } else {
            QTouchEvent cancel(QEvent::TouchCancel,touchDevice);
            QCoreApplication::sendEvent(&window,&cancel);
            for(int i=0;i<5;++i) sequence.release(i,center+QPoint((i-2)*10+90,0),&window);
            sequence.commit();
        }
        QCOMPARE(view->property("actions").toInt(),0);
        QTRY_VERIFY(!visualChild(view.get(),"hostVisual"));
        evaluate("view.host.openAt(Qt.point(320,240))");
        clickAt(QPoint(405,240),false);
        QCOMPARE(view->property("actions").toInt(),1);
    }

    void ambientBackgroundCoversForegroundOnlyWhileShown() {
        createAmbientWakeScene(); QVERIFY(view);
        auto background=visualChild(view.get(),"ambientBackground"); QVERIFY(background);
        QVERIFY(background->isVisible());
        QVERIFY(background->z()>150);
        QVERIFY(background->z()<evaluate("view.getLayer('ambient').z").toNumber());
        evaluate("view.hideAmbient()");
        QTRY_VERIFY(!view->property("ambientShown").toBool());
        QVERIFY(!background->isVisible());
    }

    void quickMenuHostGestureSelection_data() { quickMenuHostTouchReopens_data(); }
    void quickMenuHostGestureSelection() {
        QFETCH(bool,simulated);
        createQuickMenuHostScene(); QVERIFY(view);
        const QPoint center(320,240), delta(90,0);
        auto sequence=QTest::touchEvent(&window,touchDevice,false);
        if(simulated) QTest::mousePress(&window,Qt::LeftButton,Qt::ShiftModifier,center);
        else {
            for(int i=0;i<5;++i) sequence.press(i,center+QPoint((i-2)*10,0),&window);
            sequence.commit();
        }
        QTest::qWait(30);
        if(simulated) QTest::mouseMove(&window,center+QPoint(2,0));
        else {
            for(int i=0;i<5;++i) sequence.move(i,center+QPoint((i-2)*10+2,0),&window);
            sequence.commit();
        }
        QTRY_VERIFY(visualChild(view.get(),"hostVisual"));
        if(simulated) {
            QTest::mouseMove(&window,center+delta);
            QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,center+delta);
        } else {
            for(int i=0;i<5;++i) sequence.move(i,center+QPoint((i-2)*10,0)+delta,&window);
            sequence.commit();
            for(int i=0;i<5;++i) sequence.release(i,center+QPoint((i-2)*10,0)+delta,&window);
            sequence.commit();
        }
        QCOMPARE(view->property("actions").toInt(),1);
        QCOMPARE(view->property("underneath").toInt(),0);
        QTRY_VERIFY(!visualChild(view.get(),"hostVisual"));
    }

    void waffleMenuBundledVisual() {
        createWaffleMenuScene(); QVERIFY(view);
        evaluate("view.menu.openAt(Qt.point(320,240))");
        QTRY_VERIFY(visualChild(view.get(),"quickMenuLabelIcon"));
        QList<QQuickItem *> icons;
        std::function<void(QQuickItem *)> collect=[&](QQuickItem *item) {
            if(item->objectName()=="quickMenuLabelIcon") icons.append(item);
            for(auto child:item->childItems()) collect(child);
        };
        collect(view.get());
        QCOMPARE(icons.size(),9);
        for(auto icon:icons) {
            QTRY_COMPARE(icon->property("status").toInt(),1);
            QVERIFY(icon->property("source").toUrl().toString().startsWith("qrc:/qt/qml/Dsqt/Waffles/"));
        }
        for(const QString &name:{"close.svg","close_pressed.svg","quickMenuSelectionArrowwt.svg"})
            QVERIFY(QFile::exists(":/qt/qml/Dsqt/Core/res/quick_menu/"+name));
        const auto snapshot=qEnvironmentVariable("DSQT_MENU_INTEGRATION_SNAPSHOT");
        if(!snapshot.isEmpty()) {
            QTest::qWait(650);
            QVERIFY(window.grabWindow().save(snapshot));
        }
    }

    void waffleMenuStandardAndCustomActions() {
        createWaffleMenuScene(); QVERIFY(view);
        QCOMPARE(evaluate("view.menu.model.length").toInt(),9);
        QVERIFY(evaluate("view.stage.activeLauncher !== null").toBool());
        QVERIFY(!evaluate("view.menu.model.find(e=>e.action==='forward').available").toBool());
        QVERIFY(!evaluate("view.menu.model.find(e=>e.action==='arrange').available").toBool());
        evaluate("menuAction('content')");
        QCOMPARE(evaluate("view.stage.activeLauncher.viewMode").toString(),QString("content"));
        QVERIFY(evaluate("view.stage.activeLauncher.shown").toBool());
        evaluate("menuAction('asset_browsing')");
        QCOMPARE(evaluate("view.stage.activeLauncher.viewMode").toString(),QString("search"));
        evaluate("menuAction('ambient')");
        QCOMPARE(view->property("ambientRequests").toInt(),1);
        evaluate(R"(
            view.menu.actionIds=["help","content"];
            view.menu.actionController.trigger(0,view.stage.mapToGlobal(123,145));
        )");
        QCOMPARE(view->property("customActions").toInt(),1);
        QCOMPARE(evaluate("view.menu.model[0].iconText").toString(),QString("Help"));
        const auto position=view->property("customPosition").toPointF();
        QCOMPARE(window.mapFromGlobal(position.toPoint()),QPoint(123,145));
        view->setProperty("customAvailable",false);
        evaluate("view.menu.actionController.trigger(0,Qt.point(0,0))");
        QCOMPARE(view->property("customActions").toInt(),1);
        // A standard action can also be replaced without changing the menu's dispatch code.
        evaluate(R"(
            view.menu.actionOverrides={content:{iconText:"Browse",trigger:()=>view.customActions++}};
            view.stage.activeLauncher.shown=false;
            menuAction("content");
        )");
        QCOMPARE(view->property("customActions").toInt(),2);
        QVERIFY(!evaluate("view.stage.activeLauncher.shown").toBool());
        evaluate("view.menu.actionIds=['whiteboard'];menuAction('whiteboard')");
        QVERIFY(evaluate("view.stage.whiteboardShown").toBool());
        QVERIFY(!evaluate("view.menu.enabled").toBool());
        evaluate("view.stage.closeWhiteboard()");
        QVERIFY(evaluate("view.menu.enabled").toBool());
    }

    void waffleMenuStageOperations() {
        createWaffleMenuScene(); QVERIFY(view);
        evaluate(R"(
            for(let i=0;i<2;i++) view.stage.openViewer({
                objectName:"menuMedia"+i,stage:view.stage,x:10,y:10,
                viewerWidth:200,viewerHeight:140,minWidth:40,minHeight:40,
                model:{title:"Media",media:{type:"image",filepath:imageUrl}}
            });
        )");
        QTRY_COMPARE(evaluate("view.stage.mediaViewers.length").toInt(),2);
        QTest::qWait(50); // Finish the media loaders before testing immediate close/layout.
        QVERIFY(evaluate("view.menu.model.find(e=>e.action==='arrange').available").toBool());
        evaluate("menuAction('arrange')");
        QVERIFY(evaluate("view.stage.mediaViewers[0].x !== view.stage.mediaViewers[1].x").toBool());
        evaluate("menuAction('gather')");
        QVERIFY(evaluate("Math.abs(view.stage.mediaViewers[1].x-view.stage.mediaViewers[0].x)===36").toBool());
        evaluate("view.stage.setFullscreen(view.stage.mediaViewers[0],true);menuAction('arrange')");
        QTRY_VERIFY(!evaluate("view.stage.mediaLayoutPending").toBool());
        QTRY_VERIFY(evaluate("view.stage.mediaViewers.every(v=>!v.fullscreen)").toBool());
        evaluate("menuAction('close_assets')");
        QTRY_COMPARE(evaluate("view.stage.mediaViewers.length").toInt(),0);
        QVERIFY(!evaluate("view.menu.model.find(e=>e.action==='close_assets').available").toBool());
        evaluate(R"(
            view.stage.openPlaylist({slides:[
                {uid:"a",media:{type:"image",filepath:imageUrl}},
                {uid:"b",media:{type:"image",filepath:imageUrl}}
            ]});
        )");
        QTRY_VERIFY(evaluate("view.stage.canNavigate").toBool());
        QTest::qWait(50);
        evaluate("menuAction('forward')");
        QCOMPARE(evaluate("view.stage.activePlaylist.index").toInt(),1);
        QTest::qWait(50);
        evaluate("menuAction('back')");
        QCOMPARE(evaluate("view.stage.activePlaylist.index").toInt(),0);
        QTest::qWait(50); // Drain the last slide loader before fixture teardown.
    }

    void quickMenuLabelsStayInWedges_data() {
        QTest::addColumn<int>("fontSize");
        QTest::addColumn<int>("itemCount");
        QTest::addColumn<qreal>("angle");
        QTest::addColumn<qreal>("inner");
        QTest::addColumn<qreal>("outer");
        QTest::addColumn<bool>("longLabels");
        QTest::newRow("presenter-nine-labels") << 16 << 9 << 0.0 << 125.0 << 230.0 << false;
        QTest::newRow("rotated") << 16 << 9 << 73.0 << 125.0 << 230.0 << false;
        QTest::newRow("larger-font") << 32 << 9 << 0.0 << 125.0 << 230.0 << false;
        QTest::newRow("compact-long-labels") << 16 << 9 << 13.0 << 60.0 << 120.0 << true;
        QTest::newRow("more-wedges") << 16 << 18 << 0.0 << 125.0 << 230.0 << true;
        QTest::newRow("single-wedge") << 16 << 1 << 0.0 << 125.0 << 230.0 << false;
        QTest::newRow("two-wedges") << 16 << 2 << 0.0 << 125.0 << 230.0 << false;
    }
    void quickMenuLabelsStayInWedges() {
        QFETCH(int,fontSize); QFETCH(int,itemCount); QFETCH(qreal,angle);
        QFETCH(qreal,inner); QFETCH(qreal,outer); QFETCH(bool,longLabels);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Dsqt.Core
            DsQuickMenu {
                id: menu
                required property url fontAsset
                required property string iconBase
                required property int testFontSize
                required property int itemCount
                required property bool longLabels
                x: 320; y: 240
                revealDuration: 0; segmentStagger: 0; closeTimer: 0
                state: "On"
                FontLoader { id: menuFont; objectName: "menuFont"; source: menu.fontAsset }
                config: ({fontSize:menu.testFontSize,fontFamily:menuFont.name,
                    iconPath:menu.iconBase})
                readonly property var labels: ["Content","Whiteboard","Forward","Arrange","Close Assets",
                    "Gather","Back","Asset Browsing","Ambient"]
                readonly property var icons: ["content","whiteboard","forward","arrange","close_assets",
                    "gather","back","asset_browsing","ambient"]
                model: Array.from({length:itemCount}, (_,i) => ({
                    icon:icons[i%9]+".svg",
                    iconText:longLabels ? "An extremely long Asset Browsing label with extra words" : labels[i%9]
                }))
            }
        )",QUrl("qrc:/quick-menu-label-regression.qml"));
        QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        view.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties({
            {"fontAsset",QUrl::fromLocalFile(QStringLiteral(DSQT_TEST_ASSET_DIR)
                + "/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf")},
            {"iconBase",QStringLiteral("file:///data/images/waffles/quick_menu/")},
            {"testFontSize",fontSize},{"itemCount",itemCount},{"longLabels",longLabels},
            {"startAngle",angle},{"innerRadius",inner},{"outerRadius",outer}
        })));
        QVERIFY2(view,qPrintable(component.errorString()));
        QQmlEngine::setObjectOwnership(view.get(),QQmlEngine::CppOwnership);
        view->setParentItem(window.contentItem());
        auto font=view->findChild<QObject *>("menuFont"); QVERIFY(font);
        QTRY_COMPARE(font->property("status").toInt(),1);
        QList<QQuickItem *> labels;
        QList<QQuickItem *> pending{view.get()};
        while (!pending.isEmpty()) {
            auto item=pending.takeLast();
            if (item->objectName()=="quickMenuLabel") labels.append(item);
            pending.append(item->childItems());
        }
        QCOMPARE(labels.size(),itemCount);
        for (auto label : labels) {
            auto text=label->findChild<QQuickItem *>("quickMenuLabelText"); QVERIFY(text);
            auto icon=label->findChild<QQuickItem *>("quickMenuLabelIcon"); QVERIFY(icon);
            QTRY_COMPARE(icon->property("status").toInt(),1);
            QVERIFY(QMetaObject::invokeMethod(text,"forceLayout"));
            QVERIFY(label->isVisible());
            QVERIFY(label->clip());
            QVERIFY(text->property("lineCount").toInt()<=2);
            QVERIFY(text->property("contentWidth").toReal()<=text->width()+0.1);
            QVERIFY(text->property("contentHeight").toReal()<=text->height()+0.1);
            if (!longLabels) QVERIFY(!text->property("truncated").toBool());
            engine.globalObject().setProperty("menuLabelText",engine.newQObject(text));
            const int fittedSize=evaluate("menuLabelText.fontInfo.pixelSize").toInt();
            QVERIFY(fittedSize<=fontSize);
            if (fontSize==16 && itemCount==9 && !longLabels)
                QVERIFY2(fittedSize>=12,qPrintable(text->property("text").toString()));
            if (text->property("text").toString()=="Asset Browsing" && itemCount==9)
                QCOMPARE(text->property("lineCount").toInt(),2);

            // Sample the complete clipped label area, not just the text's anchor or corners:
            // the inner circular boundary can cut through the middle of a rectangle edge.
            auto segment=label->parentItem();
            const QPointF center(segment->width()/2,segment->height()/2);
            const qreal start=segment->property("startAngle").toReal();
            const qreal sweep=segment->property("sweepAngle").toReal();
            for (int row=0; row<=16; ++row) {
                for (int column=0; column<=16; ++column) {
                    const QPointF point=label->mapToItem(segment,
                        QPointF(label->width()*column/16,label->height()*row/16))-center;
                    const qreal radius=std::hypot(point.x(),point.y());
                    QVERIFY(radius>=inner+5.9);
                    QVERIFY(radius<=outer-5.9);
                    const qreal degrees=qRadiansToDegrees(std::atan2(point.y(),point.x()))+90-start;
                    const qreal localAngle=std::fmod(std::fmod(degrees,360.0)+360.0,360.0);
                    QVERIFY2(localAngle<=sweep,qPrintable(text->property("text").toString()));
                }
            }
        }
        engine.globalObject().setProperty("menuLabelText",QJSValue());
        const QString snapshot=qEnvironmentVariable("DSQT_MENU_LABEL_SNAPSHOT");
        if (!snapshot.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window.grabWindow().save(snapshot));
        }
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene(); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene({}, QUrl::fromLocalFile(file.fileName())); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene(); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene(); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene(); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene(overlayType); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene(overlayType); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createStageScene(pdfCreatedLast, locked); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createStageScene(pdfCreatedLast, locked, true); QVERIFY(view); QVERIFY(pdfPageView());
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
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createStageScene(true, locked); QVERIFY(view); QVERIFY(pdfPageView());
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
        createStageScene(true, true, false, "image"); QVERIFY(view);
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
        if (mediaType == "pdf" && !pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
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

    void customMenuSurvivesHostReattachment() {
        createQuickMenuHostScene(true); QVERIFY(view);
        evaluate("view.host.openAt(Qt.point(320,240))");
        auto host = qobject_cast<QQuickItem *>(view->property("host").value<QObject *>());
        QVERIFY(host);
        QVERIFY(host->property("mouseMenu").value<QObject *>());
        host->setParentItem(nullptr);
        QVERIFY(!host->property("mouseMenu").value<QObject *>());
        host->setParentItem(view.get());
        evaluate("view.host.openAt(Qt.point(320,240))");
        QTest::qWait(50); // Deliver any delayed closed signal from the previous visual.
        QVERIFY(host->property("mouseMenu").value<QObject *>());
        clickAt(QPoint(320,240),false);
        QCOMPARE(view->property("actions").toInt(),1);
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
        QTest::newRow("fullscreen-scrim") << QString("fullscreen-stage") << QPoint(110,240);
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
        QTest::newRow("fullscreen-scrim") << QString("fullscreen-stage") << QPoint(110,240);
    }
    void panelsBlockPdfGestures() {
        QFETCH(QString, type); QFETCH(QPoint, position);
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createPanelScene(type, true); QVERIFY(view); QVERIFY(pdfPageView());
        auto pdf = pdfPageView();
        pdf->setProperty("renderScale", 2.0);
        QTest::qWait(100);
        QTRY_VERIFY(!pdf->property("moving").toBool());
        if (type == "fullscreen-stage") {
            auto stage = visualChild(view.get(), "panel"); QVERIFY(stage);
            QTRY_VERIFY(stage->property("_fullscreenViewer").value<QObject *>());
            auto fullscreen = qobject_cast<QQuickItem *>(stage->property("_fullscreenViewer").value<QObject *>());
            QVERIFY(fullscreen);
            // This row targets the scrim outside the viewer, not the controller bar.
            QVERIFY(!fullscreen->contains(fullscreen->mapFromItem(view.get(),position)));
        }
        const auto scale = pdf->property("renderScale").toReal();
        const auto scroll = pdf->property("contentY").toReal();
        wheelAt(position);
        QCOMPARE(pdf->property("contentY").toReal(), scroll);
        pinchAt(position);
        QCOMPARE(pdf->property("renderScale").toReal(), scale);
        QCOMPARE(view->property("underneath").toInt(), 0);
        if (type == "fullscreen-stage") {
            auto stage = visualChild(view.get(), "panel"); QVERIFY(stage);
            QVERIFY(stage->property("_fullscreenViewer").value<QObject *>());
            engine.globalObject().setProperty("scrimStage",engine.newQObject(stage));
            evaluate("scrimStage.closeViewer(scrimStage._fullscreenViewer)");
            QTRY_VERIFY(!stage->property("_fullscreenViewer").value<QObject *>());
            QTest::qWait(100);
            wheelAt(position);
            QVERIFY(pdf->property("contentY").toReal() > scroll);
        }
    }

    void fullscreenScrimTouchTapDismisses() {
        createPanelScene("fullscreen-stage"); QVERIFY(view);
        auto stage=visualChild(view.get(),"panel"); QVERIFY(stage);
        QTRY_VERIFY(stage->property("_fullscreenViewer").value<QObject *>());
        clickAt(QPoint(110,240),true);
        QTRY_VERIFY(!stage->property("_fullscreenViewer").value<QObject *>());
        QCOMPARE(view->property("underneath").toInt(),0);
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

    void whiteboardToolbarMoves_data() { stageSelectsCustomViewerByStackingOrder_data(); }
    void whiteboardToolbarMoves() {
        QFETCH(bool, touch);
        createPanelScene("whiteboard"); QVERIFY(view);
        auto board = visualChild(view.get(), "panel"); QVERIFY(board);
        // Exercise the fit-to-window transform used by ECPresenter.
        board->setTransformOrigin(QQuickItem::TopLeft);
        board->setScale(0.25);
        board->setSize(QSizeF(1600, 1000));
        auto toolbar = visualChild(board, "whiteboardToolbar"); QVERIFY(toolbar);
        auto handle = visualChild(toolbar, "whiteboardDragHandle"); QVERIFY(handle);
        QTRY_VERIFY(qAbs(toolbar->x() + toolbar->width() + 32 - board->width()) < 1);
        QTRY_VERIFY(qAbs(toolbar->y() + toolbar->height()/2 - board->height()/2) < 1);
        const auto initial = toolbar->position();
        dragAt(centerOf(handle), QPoint(-50,-20), touch);
        QVERIFY(qAbs(toolbar->x() - (initial.x() - 200)) < 5);
        QVERIFY(qAbs(toolbar->y() - (initial.y() - 80)) < 5);
        QVERIFY(!board->property("hasDrawing").toBool());
        QVERIFY(!board->property("drawing").toBool());
        const auto moved = toolbar->position();
        board->setProperty("shown", false);
        board->setProperty("shown", true);
        QCOMPARE(toolbar->position(), moved);
        auto highlighter = visualChild(toolbar, "highlighterButton"); QVERIFY(highlighter);
        const auto previousTool = board->property("tool");
        clickAt(centerOf(highlighter), touch);
        QVERIFY(board->property("tool") != previousTool);
        QCOMPARE(toolbar->position(), moved);
        QVERIFY(!board->property("hasDrawing").toBool());
        dragAt(centerOf(handle), QPoint(70,40) - centerOf(handle), touch);
        QVERIFY(qAbs(toolbar->x()) < 1);
        QVERIFY(qAbs(toolbar->y()) < 1);
        dragAt(centerOf(handle), QPoint(620,440) - centerOf(handle), touch);
        QVERIFY(qAbs(toolbar->x() + toolbar->width() - board->width()) < 1);
        QVERIFY(qAbs(toolbar->y() + toolbar->height() - board->height()) < 1);
        board->setSize(QSizeF(1200,700));
        QTRY_VERIFY(toolbar->x() >= 0 && toolbar->x() + toolbar->width() <= board->width() + 1);
        QTRY_VERIFY(toolbar->y() >= 0 && toolbar->y() + toolbar->height() <= board->height() + 1);
        QVERIFY(!board->property("hasDrawing").toBool());
        QCOMPARE(view->property("underneath").toInt(), 0);
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

    void simulatedClusterConsumesInput_data() {
        QTest::addColumn<QString>("receiver");
        QTest::addColumn<bool>("releaseShift");
        for (const QString &receiver : {"button", "handler", "mouseArea"})
            for (bool releaseShift : {false, true})
                QTest::newRow(qPrintable(receiver + (releaseShift ? "-release-shift" : "-hold-shift")))
                    << receiver << releaseShift;
    }
    void simulatedClusterConsumesInput() {
        QFETCH(QString, receiver); QFETCH(bool, releaseShift);
        createClusterScene(receiver); QVERIFY(view);
        const QPoint position(250, 200);
        clickAt(position, false);
        QCOMPARE(view->property("actions").toInt(), 1);
        view->setProperty("actions", 0);
        QTest::mousePress(&window, Qt::LeftButton, Qt::ShiftModifier, position);
        QTest::qWait(30);
        QTest::mouseMove(&window, position + QPoint(2, 0));
        QTRY_COMPARE(view->property("menus").toInt(), 1);
        QTest::mouseRelease(&window, Qt::LeftButton,
            releaseShift ? Qt::NoModifier : Qt::ShiftModifier, position + QPoint(2, 0));
        QCOMPARE(view->property("releases").toInt(), 1);
        QCOMPARE(view->property("actions").toInt(), 0);
        clickAt(position, false);
        QCOMPARE(view->property("actions").toInt(), 1);
    }

    void touchClusterConsumesInput_data() {
        QTest::addColumn<QString>("receiver");
        QTest::addColumn<bool>("staggered");
        for (const QString &receiver : {"button", "handler", "mouseArea"})
            for (bool staggered : {false, true})
                QTest::newRow(qPrintable(receiver + (staggered ? "-staggered" : "-together")))
                    << receiver << staggered;
    }
    void touchClusterConsumesInput() {
        QFETCH(QString, receiver); QFETCH(bool, staggered);
        createClusterScene(receiver); QVERIFY(view);
        const QPoint center(250, 200);
        clickAt(center, true);
        QCOMPARE(view->property("actions").toInt(), 1);
        view->setProperty("actions", 0);
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        const QList<QPoint> points{center, center+QPoint(30,0), center+QPoint(0,30),
                                  center+QPoint(30,30), center+QPoint(15,15)};
        if (staggered) {
            for (int i=0; i<5; ++i) {
                for (int j=0; j<i; ++j) sequence.stationary(j);
                sequence.press(i, points[i], &window).commit();
                QTest::qWait(10);
            }
        } else {
            for (int i=0; i<5; ++i) sequence.press(i, points[i], &window);
            sequence.commit();
        }
        QTest::qWait(30);
        for (int i=0; i<5; ++i) sequence.move(i, points[i]+QPoint(2,0), &window);
        sequence.commit();
        QTRY_COMPARE(view->property("menus").toInt(), 1);
        // Releasing one at a time must not turn the last finger into a normal tap.
        for (int i=4; i>=0; --i) {
            for (int j=0; j<i; ++j) sequence.stationary(j);
            sequence.release(i, points[i]+QPoint(2,0), &window).commit();
            QTest::qWait(10);
        }
        QCOMPARE(view->property("releases").toInt(), 1);
        QCOMPARE(view->property("actions").toInt(), 0);
        clickAt(center, true);
        QCOMPARE(view->property("actions").toInt(), 1);
    }

    void touchClusterKeepsUnrelatedInput() {
        createClusterScene("handler"); QVERIFY(view);
        auto sequence=QTest::touchEvent(&window,touchDevice,false);
        const QPoint center(100,100), other(550,400);
        for (int i=0; i<5; ++i) sequence.press(i,center+QPoint(i*10,0),&window);
        sequence.commit();
        for (int i=0; i<5; ++i) sequence.stationary(i);
        sequence.press(5,other,&window).commit();
        for (int i=0; i<5; ++i) sequence.stationary(i);
        sequence.release(5,other,&window).commit();
        QCOMPARE(view->property("actions").toInt(),1);
        for (int i=0; i<5; ++i) sequence.release(i,center+QPoint(i*10,0),&window);
        sequence.commit();
        QCOMPARE(view->property("actions").toInt(),1);
    }

    void touchClusterBlocksPdfGestures() {
        if (!pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createMediaScene(); QVERIFY(view); QVERIFY(pdfPageView());
        visualChild(view.get(), "pdfViewer")->setProperty("contentLocked", true);
        auto manager = new dsqt::ui::DsQmlClusterManager(view.get());
        manager->setSize(view->size());
        manager->setMinClusterTouchCount(5);
        manager->setMinClusterSeperation(150);
        manager->setBoundingBoxSize(250);
        manager->setTriggerTime(0);
        auto pdf = pdfPageView();
        QVERIFY(QMetaObject::invokeMethod(pdf, "selectAll"));
        const auto selected = pdf->property("selectedText").toString();
        QVERIFY(!selected.isEmpty());
        const auto initialScale = pdf->property("renderScale").toReal();
        auto sequence = QTest::touchEvent(&window, touchDevice, false);
        const QPoint center(320,220);
        for (int i=0; i<5; ++i) sequence.press(i,center+QPoint((i-2)*15,0),&window);
        sequence.commit();
        for (int distance : {20,30,40}) {
            for (int i=0; i<5; ++i) sequence.move(i,center+QPoint((i-2)*distance,0),&window);
            sequence.commit();
            QTest::qWait(20);
        }
        for (int i=0; i<5; ++i) sequence.release(i,center+QPoint((i-2)*40,0),&window);
        sequence.commit();
        QCOMPARE(pdf->property("renderScale").toReal(),initialScale);
        QCOMPARE(pdf->property("selectedText").toString(),selected);
        QCOMPARE(view->property("underneath").toInt(),0);
        // Two-finger PDF zoom remains available.
        pinchAt(center);
        QVERIFY(pdf->property("renderScale").toReal()>initialScale);
    }

    void clusterCancellationReleasesInput_data() {
        QTest::addColumn<bool>("touch");
        QTest::newRow("mouse") << false;
        QTest::newRow("touch") << true;
    }
    void clusterCancellationReleasesInput() {
        QFETCH(bool,touch);
        createClusterScene("button"); QVERIFY(view);
        auto manager=view->findChild<dsqt::ui::DsQmlClusterManager *>("clusterInput");
        QVERIFY(manager);
        auto sequence=QTest::touchEvent(&window,touchDevice,false);
        const QPoint point(250,200);
        if (touch) {
            for (int i=0; i<5; ++i) sequence.press(i,point+QPoint(i*10,0),&window);
            sequence.commit();
        } else QTest::mousePress(&window,Qt::LeftButton,Qt::ShiftModifier,point);
        QCOMPARE(manager->clusters()->rowCount(),1);
        if (touch) {
            QTouchEvent cancel(QEvent::TouchCancel,touchDevice);
            QCoreApplication::sendEvent(&window,&cancel);
            // Finish the synthetic device's sequence after the platform cancellation.
            for (int i=0; i<5; ++i) sequence.release(i,point+QPoint(i*10,0),&window);
            sequence.commit();
        } else {
            manager->ungrabMouse();
            QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,point);
        }
        QCOMPARE(manager->clusters()->rowCount(),0);
        QCOMPARE(view->property("releases").toInt(),0);
        QCOMPARE(view->property("actions").toInt(),0);
        clickAt(point,touch);
        QCOMPARE(view->property("actions").toInt(),1);
    }

    void allMediaControlsTimeout_data() {
        QTest::addColumn<bool>("fullscreen");
        QTest::addColumn<bool>("touch");
        QTest::addColumn<QString>("mediaType");
        for (bool fullscreen : {false,true})
            for (bool touch : {false,true})
                for (const QString &media : {"image","pdf"})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(fullscreen ? "fullscreen" : "windowed",
                        touch ? "touch" : "mouse",media))) << fullscreen << touch << media;
    }
    void allMediaControlsTimeout() {
        QFETCH(bool, fullscreen); QFETCH(bool, touch); QFETCH(QString, mediaType);
        if (mediaType == "pdf" && !pdfAvailable) QSKIP(qPrintable(pdfUnavailableReason));
        createIdleControlsScene(fullscreen,mediaType); QVERIFY(view);
        auto viewer = visualChild(view.get(),"pdfViewer"); QVERIFY(viewer);
        auto controller = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(view->property("_fsController")));
        const QPoint point = fullscreen ? QPoint(320,170) : QPoint(300,230);
        // Leave a stationary mouse over the media: timeout must still work.
        QTest::mouseMove(&window,point);
        QString selection;
        if (mediaType == "pdf") {
            QVERIFY(QMetaObject::invokeMethod(pdfPageView(),"selectAll"));
            QTRY_VERIFY(!pdfPageView()->property("selectedText").toString().isEmpty());
            selection=pdfPageView()->property("selectedText").toString();
        }
        view->setProperty("controlsIdleMs",100);
        QTRY_VERIFY(viewer->property("controlsHidden").toBool());
        if (fullscreen) {
            QVERIFY(controller);
            QTRY_VERIFY(!controller->isVisible());
        } else {
            QTRY_VERIFY(evaluate("idleViewer.controls.every(c => c.opacity === 0 && !c.visible)").toBool());
        }
        clickAt(point,touch);
        QTRY_VERIFY(!viewer->property("controlsHidden").toBool());
        // Disable timeout after reveal so assertions cannot race the short test timer.
        view->setProperty("controlsIdleMs",0);
        if (fullscreen) QTRY_COMPARE(controller->opacity(),1.0);
        else QTRY_VERIFY(evaluate("idleViewer.controls.every(c => c.opacity === 1)").toBool());
        if (mediaType == "pdf") {
            QCOMPARE(pdfPageView()->property("selectedText").toString(),selection);
            clickAt(point,touch);
            QTRY_VERIFY(pdfPageView()->property("selectedText").toString()!=selection);
        }
    }

    void fullscreenIdleModes_data() {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<int>("timeout");
        QTest::newRow("hide") << QString("hide") << 100;
        QTest::newRow("collapse") << QString("collapse") << 100;
        QTest::newRow("none") << QString("none") << 100;
        QTest::newRow("disabled") << QString("hide") << 0;
        QTest::newRow("invalid-stays-visible") << QString("invalid") << 100;
    }
    void fullscreenIdleModes() {
        QFETCH(QString,mode); QFETCH(int,timeout);
        createIdleControlsScene(true); QVERIFY(view);
        auto controller=qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(view->property("_fsController")));
        QVERIFY(controller);
        view->setProperty("fullscreenControllerIdleBehavior",mode);
        view->setProperty("controlsIdleMs",timeout);
        QTest::qWait(500);
        QCOMPARE(controller->property("controlsHidden").toBool(),mode=="hide" && timeout>0);
        QCOMPARE(controller->property("collapsed").toBool(),mode=="collapse");
        if (mode=="collapse") {
            view->setProperty("controlsIdleMs",0);
            auto tab=itemWithProperty(controller,"icon",
                QUrl("qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/window/controller_expand.svg"));
            QVERIFY(tab);
            clickAt(centerOf(tab),false);
            QVERIFY(!controller->property("collapsed").toBool());
        }
        // Changing settings wakes existing controllers and updates their timeout binding.
        view->setProperty("controlsIdleMs",0);
        QTRY_COMPARE(controller->opacity(),1.0);
        QCOMPARE(controller->property("controlsIdleMs").toInt(),0);
        evaluate("view.setFullscreen(idleViewer,false)");
        QTRY_VERIFY(!controller->isVisible());
        evaluate("view.setFullscreen(idleViewer,true)");
        QTRY_COMPARE(controller->opacity(),1.0);
        QVERIFY(!controller->property("collapsed").toBool());
    }

    void controlsStayAwakeDuringPress_data() {
        QTest::addColumn<bool>("fullscreen");
        QTest::addColumn<bool>("touch");
        for (bool fullscreen : {false,true})
            for (bool touch : {false,true})
                QTest::newRow(qPrintable(QString("%1-%2").arg(fullscreen?"fullscreen":"windowed",
                    touch?"touch":"mouse"))) << fullscreen << touch;
    }
    void controlsStayAwakeDuringPress() {
        QFETCH(bool,fullscreen); QFETCH(bool,touch);
        createIdleControlsScene(fullscreen); QVERIFY(view);
        auto viewer=visualChild(view.get(),"pdfViewer"); QVERIFY(viewer);
        const QPoint point = fullscreen ? QPoint(290,290) : QPoint(270,120);
        QTest::mouseMove(&window,point);
        view->setProperty("controlsIdleMs",150);
        auto sequence=QTest::touchEvent(&window,touchDevice,false);
        if (touch) sequence.press(0,point,&window).commit();
        else QTest::mousePress(&window,Qt::LeftButton,Qt::NoModifier,point);
        QTest::qWait(450);
        QVERIFY(!viewer->property("controlsHidden").toBool());
        if (touch) sequence.release(0,point,&window).commit();
        else QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,point);
        QTRY_VERIFY(viewer->property("controlsHidden").toBool());
        // Moving the mouse must restore controls; a stationary pointer must not keep them alive.
        QTest::mouseMove(&window,QPoint(5,5),20);
        QTest::mouseMove(&window,QPoint(330,210),20);
        QTRY_VERIFY(!viewer->property("controlsHidden").toBool());
        QTRY_VERIFY(viewer->property("controlsHidden").toBool());
    }

    void fullscreenControllerStaysHiddenUnderStationaryMouse() {
        createIdleControlsScene(true); QVERIFY(view);
        auto viewer=visualChild(view.get(),"pdfViewer"); QVERIFY(viewer);
        QTest::mouseMove(&window,QPoint(290,290));
        view->setProperty("controlsIdleMs",100);
        QSignalSpy hiddenChanges(viewer,SIGNAL(controlsHiddenChanged()));
        QVERIFY(hiddenChanges.isValid());
        QTest::qWait(800);
        QVERIFY(viewer->property("controlsHidden").toBool());
        QCOMPARE(hiddenChanges.count(),1);
    }

    void windowedControlsDisabledTimeout() {
        createIdleControlsScene(false); QVERIFY(view);
        view->setProperty("controlsIdleMs",100);
        QTRY_VERIFY(evaluate("idleViewer.controlsHidden").toBool());
        view->setProperty("controlsIdleMs",0);
        QTRY_VERIFY(evaluate("idleViewer.controls.every(c => c.opacity === 1)").toBool());
        QTest::qWait(500);
        QVERIFY(!evaluate("idleViewer.controlsHidden").toBool());
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
