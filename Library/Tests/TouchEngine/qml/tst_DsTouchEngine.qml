import QtQuick
import QtTest
import Dsqt.TouchEngine

Item {
    id: root
    width: 640
    height: 360
    visible: true

    readonly property color transparentClearColor: '#00000000'
    readonly property color customClearColor: '#80446688'

    Component {
        id: sessionComponent

        DsTouchEngineSession {}
    }

    Component {
        id: viewComponent

        DsTouchEngineView {
            width: 320
            height: 180
        }
    }

    Component {
        id: sessionViewComponent

        Item {
            width: 320
            height: 180
            visible: true

            readonly property alias sessionObject: touchEngineSession
            readonly property alias viewObject: touchEngineView

            DsTouchEngineSession {
                id: touchEngineSession
            }

            DsTouchEngineView {
                id: touchEngineView
                anchors.fill: parent
                session: touchEngineSession
            }
        }
    }

    TestCase {
        name: "DsTouchEngineQmlTests"
        when: windowShown

        function test_sessionConstructionAndDefaults() {
            let session = createTemporaryObject(sessionComponent, root)
            verify(!!session, "Component exists")

            compare(session.componentPath, "")
            compare(session.preferredEnginePath, "")
            compare(session.frameRate, 60.0)
            compare(session.timeMode, DsTouchEngine.External)
            compare(session.running, true)
            compare(session.state, DsTouchEngine.Idle)
            compare(session.graphicsApi, DsTouchEngine.Unknown)
            compare(session.loaded, false)
            compare(session.ready, false)
            compare(session.errorString, "")
            compare(session.links.length, 0)
            compare(session.frameCount, 0)
            compare(session.cpuFrameTimeMs, 0.0)
            compare(session.gpuFrameTimeMs, -1.0)
            compare(session.statisticsFrames, 0)
            compare(session.framesDropped, -1)
            compare(session.cpuMemoryBytes, 0)
            compare(session.gpuMemoryBytes, 0)
            compare(session.touchDesignerVersion, "unknown")
        }

        function test_sessionWritablePropertiesWithoutLoadingComponent() {
            let session = createTemporaryObject(sessionComponent, root)
            verify(!!session, "Component exists")

            session.componentPath = "fixtures/test.tox"
            session.preferredEnginePath = "engines/TouchEngine"
            session.frameRate = 120.0
            session.timeMode = DsTouchEngine.Internal
            session.running = false

            compare(session.componentPath, "fixtures/test.tox")
            compare(session.preferredEnginePath, "engines/TouchEngine")
            compare(session.frameRate, 120.0)
            compare(session.timeMode, DsTouchEngine.Internal)
            compare(session.running, false)
            compare(session.state, DsTouchEngine.Idle)
        }

        function test_viewConstructionAndDefaults() {
            let view = createTemporaryObject(viewComponent, root)
            verify(!!view, "Component exists")

            compare(view.session, null)
            compare(view.outputLink, "output")
            compare(view.clearColor, root.transparentClearColor)
        }

        function test_viewOutputPropertiesAreWritable() {
            let view = createTemporaryObject(viewComponent, root)
            verify(!!view, "Component exists")

            view.outputLink = "out1"
            view.clearColor = root.customClearColor

            compare(view.outputLink, "out1")
            compare(view.clearColor, root.customClearColor)
        }

        function test_declarativeSessionBinding() {
            let pair = createTemporaryObject(sessionViewComponent, root)
            verify(!!pair, "Component exists")
            verify(!!pair.sessionObject, "Object exists")
            verify(!!pair.viewObject, "Object exists")

            compare(pair.viewObject.session, pair.sessionObject)
        }

        function test_destroyingSessionDetachesView() {
            let session = createTemporaryObject(sessionComponent, root)
            verify(!!session, "Component exists")
            let view = createTemporaryObject(viewComponent, root)
            verify(!!view, "Object exists")

            view.session = session
            compare(view.session, session)

            session.destroy()
            tryCompare(view, "session", null)
        }
    }
}
