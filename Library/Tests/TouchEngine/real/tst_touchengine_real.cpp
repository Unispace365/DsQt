#include <Dsqt/TouchEngine/DsTouchEngineSession.h>
#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>
#include <Dsqt/TouchEngine/DsTouchEngineView.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QSize>
#include <QTextStream>
#include <QThread>
#include <QVariantMap>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

using dsqt::touchengine::DsTouchEngineSession;
using dsqt::touchengine::DsTouchEngineTypes;
using dsqt::touchengine::DsTouchEngineView;

#ifdef Q_OS_WIN
// Hybrid-GPU systems commonly bind desktop OpenGL to the integrated adapter,
// while TouchEngine's WGL/D3D interop is available only on the discrete GPU.
// These vendor-standard executable exports make the real backend test request
// the high-performance adapter before Qt creates its first graphics context.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

constexpr int kWindowTimeoutMs = 30'000;
constexpr int kLoadTimeoutMs = 60'000;
constexpr int kFrameTimeoutMs = 45'000;
constexpr int kUnloadTimeoutMs = 30'000;
constexpr int kFramesPerEpoch = 3;
constexpr int kReloadCycles = 3;

enum class ExitCode : int {
    Success = 0,
    InvalidArguments = 2,
    MissingTox = 3,
    ArtifactDirectoryFailure = 4,
    SceneGraphFailure = 10,
    EngineFailure = 11,
    Timeout = 12,
    MissingTextureOutput = 13,
    CaptureFailure = 14,
    SessionDetachFailure = 15,
    GraphicsApiMismatch = 16,
};

enum class WaitFailure {
    None,
    Timeout,
    SceneGraph,
    Engine,
};

struct WaitResult
{
    WaitFailure failure = WaitFailure::None;
    QString detail;

    explicit operator bool() const noexcept { return failure == WaitFailure::None; }
};

struct WindowSignals
{
    std::atomic_bool sceneGraphInitialized = false;
    std::atomic_int frameSwaps = 0;
    QString sceneGraphError;
};

template<typename Predicate>
WaitResult waitUntil(const QString &operation,
                     int timeoutMs,
                     QQuickWindow *window,
                     DsTouchEngineSession *session,
                     const WindowSignals &windowState,
                     Predicate &&predicate)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

        if (!windowState.sceneGraphError.isEmpty()) {
            return {WaitFailure::SceneGraph,
                    QStringLiteral("%1: %2").arg(operation, windowState.sceneGraphError)};
        }

        if (session) {
            if (session->state() == DsTouchEngineTypes::State::Error) {
                const QString error = session->errorString().isEmpty()
                    ? QStringLiteral("TouchEngine entered Error state")
                    : session->errorString();
                return {WaitFailure::Engine,
                        QStringLiteral("%1: %2").arg(operation, error)};
            }
            if (!session->errorString().isEmpty()) {
                return {WaitFailure::Engine,
                        QStringLiteral("%1: %2").arg(operation, session->errorString())};
            }
        }

        if (std::invoke(std::forward<Predicate>(predicate)))
            return {};

        if (window)
            window->update();
        QThread::msleep(2);
    }

    const QString state = session
        ? DsTouchEngineTypes::stateName(session->state())
        : QStringLiteral("n/a");
    const quint64 frame = session ? session->frameCount() : 0;
    return {WaitFailure::Timeout,
            QStringLiteral("%1 timed out after %2 ms (state=%3, frame=%4)")
                .arg(operation)
                .arg(timeoutMs)
                .arg(state)
                .arg(frame)};
}

int reportWaitFailure(const WaitResult &result)
{
    qCritical().noquote() << "TouchEngine real test failed:" << result.detail;
    switch (result.failure) {
    case WaitFailure::SceneGraph:
        return static_cast<int>(ExitCode::SceneGraphFailure);
    case WaitFailure::Engine:
        return static_cast<int>(ExitCode::EngineFailure);
    case WaitFailure::Timeout:
        return static_cast<int>(ExitCode::Timeout);
    case WaitFailure::None:
        break;
    }
    return static_cast<int>(ExitCode::Success);
}

QString firstTextureOutputLink(const QVariantList &links)
{
    for (const QVariant &entry : links) {
        const QVariantMap link = entry.toMap();
        const auto scope = link.value(QStringLiteral("scope"))
                               .value<DsTouchEngineTypes::LinkScope>();
        const auto type = link.value(QStringLiteral("type"))
                              .value<DsTouchEngineTypes::LinkType>();
        if (scope == DsTouchEngineTypes::LinkScope::Output
            && type == DsTouchEngineTypes::LinkType::Texture) {
            return link.value(QStringLiteral("identifier")).toString();
        }
    }
    return {};
}

std::optional<DsTouchEngineTypes::GraphicsApi> expectedGraphicsApi(const QString &backend)
{
    if (backend == QStringLiteral("d3d11"))
        return DsTouchEngineTypes::GraphicsApi::Direct3D11;
    if (backend == QStringLiteral("d3d12"))
        return DsTouchEngineTypes::GraphicsApi::Direct3D12;
    if (backend == QStringLiteral("vulkan"))
        return DsTouchEngineTypes::GraphicsApi::Vulkan;
    if (backend == QStringLiteral("opengl") || backend == QStringLiteral("gl"))
        return DsTouchEngineTypes::GraphicsApi::OpenGL;
    return std::nullopt;
}

void setWindowAndViewSize(QQuickWindow &window, DsTouchEngineView &view, const QSize &size)
{
    window.resize(size);
    view.setSize(QSizeF(size));
    view.update();
    window.update();
}

bool captureWindow(QQuickWindow &window, const QString &path, QString *error)
{
    const QImage image = window.grabWindow();
    if (image.isNull()) {
        if (error)
            *error = QStringLiteral("QQuickWindow::grabWindow returned an empty image");
        return false;
    }
    if (!image.save(path, "PNG")) {
        if (error)
            *error = QStringLiteral("could not save PNG to %1").arg(path);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("tst_touchengine_real"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Real TouchEngine RHI smoke and lifecycle stress test"));
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    const QCommandLineOption toxOption(
        {QStringLiteral("t"), QStringLiteral("tox")},
        QStringLiteral("TouchDesigner component to load."),
        QStringLiteral("path"));
    const QCommandLineOption artifactOption(
        {QStringLiteral("a"), QStringLiteral("artifact-dir")},
        QStringLiteral("Directory in which captured PNG files are written."),
        QStringLiteral("path"));
    parser.addOption(toxOption);
    parser.addOption(artifactOption);

    if (!parser.parse(QCoreApplication::arguments())) {
        qCritical().noquote() << "Invalid command line:" << parser.errorText();
        return static_cast<int>(ExitCode::InvalidArguments);
    }
    if (parser.isSet(helpOption)) {
        QTextStream(stdout) << parser.helpText();
        return static_cast<int>(ExitCode::Success);
    }
    if (parser.isSet(versionOption)) {
        QTextStream(stdout) << QCoreApplication::applicationName() << ' '
                            << QCoreApplication::applicationVersion() << '\n';
        return static_cast<int>(ExitCode::Success);
    }
    if (!parser.isSet(toxOption) || !parser.isSet(artifactOption)) {
        qCritical().noquote()
            << "Both --tox <path> and --artifact-dir <path> are required.";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    const QFileInfo toxFile(QDir::current().absoluteFilePath(parser.value(toxOption)));
    if (!toxFile.exists() || !toxFile.isFile()) {
        qCritical().noquote() << "TouchEngine .tox does not exist:" << toxFile.absoluteFilePath();
        return static_cast<int>(ExitCode::MissingTox);
    }

    const QString artifactPath =
        QDir::cleanPath(QDir::current().absoluteFilePath(parser.value(artifactOption)));
    if (!QDir().mkpath(artifactPath)) {
        qCritical().noquote() << "Could not create artifact directory:" << artifactPath;
        return static_cast<int>(ExitCode::ArtifactDirectoryFailure);
    }
    const QFileInfo artifactInfo(artifactPath);
    if (!artifactInfo.isDir()) {
        qCritical().noquote() << "Artifact path is not a directory:" << artifactPath;
        return static_cast<int>(ExitCode::ArtifactDirectoryFailure);
    }
    const QDir artifactDirectory(artifactPath);

    const QString backend = qEnvironmentVariable("QSG_RHI_BACKEND", "default").toLower();
    qInfo().noquote() << "Starting TouchEngine real test with QSG_RHI_BACKEND=" << backend
                      << "tox=" << toxFile.absoluteFilePath();

    WindowSignals windowState;
    QQuickWindow window;
    window.setTitle(QStringLiteral("DsQt TouchEngine real test (%1)").arg(backend));
    window.setColor(Qt::black);

    QObject::connect(&window, &QQuickWindow::sceneGraphInitialized, &window, [&windowState] {
        windowState.sceneGraphInitialized.store(true, std::memory_order_release);
    }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&windowState] {
        windowState.frameSwaps.fetch_add(1, std::memory_order_relaxed);
    }, Qt::DirectConnection);
    QObject::connect(
        &window, &QQuickWindow::sceneGraphError, &application,
        [&windowState](QQuickWindow::SceneGraphError code, const QString &message) {
            windowState.sceneGraphError = QStringLiteral("scene graph error %1: %2")
                                              .arg(static_cast<int>(code))
                                              .arg(message);
        });

    auto session = std::make_unique<DsTouchEngineSession>();
    auto *view = new DsTouchEngineView(window.contentItem());
    view->setClearColor(Qt::black);
    view->setSession(session.get());
    session->setComponentPath(toxFile.absoluteFilePath());

    const std::array<QSize, kReloadCycles + 1> epochSizes = {
        QSize(640, 360),
        QSize(977, 541),
        QSize(320, 240),
        QSize(801, 451),
    };
    setWindowAndViewSize(window, *view, epochSizes.front());
    window.show();

    WaitResult waitResult = waitUntil(
        QStringLiteral("initializing the Qt Quick scene graph"),
        kWindowTimeoutMs,
        &window,
        session.get(),
        windowState,
        [&] {
            return windowState.sceneGraphInitialized.load(std::memory_order_acquire)
                && window.isExposed();
        });
    if (!waitResult)
        return reportWaitFailure(waitResult);

    const auto runLoadedEpoch = [&](int epoch, const QString &description) -> int {
        WaitResult result = waitUntil(
            QStringLiteral("%1: waiting for Ready").arg(description),
            kLoadTimeoutMs,
            &window,
            session.get(),
            windowState,
            [&] { return session->isReady(); });
        if (!result)
            return reportWaitFailure(result);

        if (const auto expected = expectedGraphicsApi(backend);
            expected && session->graphicsApi() != *expected) {
            qCritical().noquote()
                << QStringLiteral("%1 selected %2, expected %3 for QSG_RHI_BACKEND=%4")
                       .arg(description,
                            DsTouchEngineTypes::graphicsApiName(session->graphicsApi()),
                            DsTouchEngineTypes::graphicsApiName(*expected),
                            backend);
            return static_cast<int>(ExitCode::GraphicsApiMismatch);
        }

        const QString outputLink = firstTextureOutputLink(session->links());
        if (outputLink.isEmpty()) {
            qCritical().noquote() << description
                                  << "did not publish an output texture link. Links:"
                                  << session->links();
            return static_cast<int>(ExitCode::MissingTextureOutput);
        }
        view->setOutputLink(outputLink);
        setWindowAndViewSize(window, *view, epochSizes.at(static_cast<size_t>(epoch)));

        const quint64 firstFrame = session->frameCount();
        result = waitUntil(
            QStringLiteral("%1: waiting for %2 completed frames")
                .arg(description)
                .arg(kFramesPerEpoch),
            kFrameTimeoutMs,
            &window,
            session.get(),
            windowState,
            [&] {
                return session->frameCount()
                    >= firstFrame + static_cast<quint64>(kFramesPerEpoch);
            });
        if (!result)
            return reportWaitFailure(result);

        const int targetSwap = windowState.frameSwaps.load(std::memory_order_relaxed) + 2;
        view->update();
        result = waitUntil(
            QStringLiteral("%1: waiting for output presentation").arg(description),
            kFrameTimeoutMs,
            &window,
            session.get(),
            windowState,
            [&] { return windowState.frameSwaps.load(std::memory_order_relaxed) >= targetSwap; });
        if (!result)
            return reportWaitFailure(result);

        const QSize size = epochSizes.at(static_cast<size_t>(epoch));
        const QString capturePath = artifactDirectory.filePath(
            QStringLiteral("touchengine-%1-epoch-%2-%3x%4.png")
                .arg(backend)
                .arg(epoch)
                .arg(size.width())
                .arg(size.height()));
        QString captureError;
        if (!captureWindow(window, capturePath, &captureError)) {
            qCritical().noquote() << description << "capture failed:" << captureError;
            return static_cast<int>(ExitCode::CaptureFailure);
        }

        qInfo().noquote()
            << QStringLiteral("%1 passed: api=%2 output=%3 frame=%4 capture=%5")
                   .arg(description,
                        DsTouchEngineTypes::graphicsApiName(session->graphicsApi()),
                        outputLink)
                   .arg(session->frameCount())
                   .arg(capturePath);
        return static_cast<int>(ExitCode::Success);
    };

    session->load();
    int result = runLoadedEpoch(0, QStringLiteral("initial load"));
    if (result != static_cast<int>(ExitCode::Success))
        return result;

    for (int cycle = 1; cycle <= kReloadCycles; ++cycle) {
        session->unload();
        waitResult = waitUntil(
            QStringLiteral("reload cycle %1: waiting for unload").arg(cycle),
            kUnloadTimeoutMs,
            &window,
            session.get(),
            windowState,
            [&] { return session->state() == DsTouchEngineTypes::State::Idle; });
        if (!waitResult)
            return reportWaitFailure(waitResult);

        session->reload();
        result = runLoadedEpoch(
            cycle, QStringLiteral("reload cycle %1").arg(cycle));
        if (result != static_cast<int>(ExitCode::Success))
            return result;
    }

    const int detachSwapTarget = windowState.frameSwaps.load(std::memory_order_relaxed) + 2;
    session.reset();
    if (view->session() != nullptr) {
        qCritical().noquote()
            << "Destroying a live DsTouchEngineSession did not detach its DsTouchEngineView.";
        return static_cast<int>(ExitCode::SessionDetachFailure);
    }

    view->update();
    window.update();
    waitResult = waitUntil(
        QStringLiteral("rendering after live session destruction"),
        kFrameTimeoutMs,
        &window,
        nullptr,
        windowState,
        [&] { return windowState.frameSwaps.load(std::memory_order_relaxed) >= detachSwapTarget; });
    if (!waitResult)
        return reportWaitFailure(waitResult);

    const QString detachedCapture = artifactDirectory.filePath(
        QStringLiteral("touchengine-%1-after-session-destruction.png").arg(backend));
    QString captureError;
    if (!captureWindow(window, detachedCapture, &captureError)) {
        qCritical().noquote() << "Post-destruction capture failed:" << captureError;
        return static_cast<int>(ExitCode::CaptureFailure);
    }

    qInfo().noquote()
        << QStringLiteral("TouchEngine real test passed for %1; completed %2 unload/reload cycles. "
                          "Artifacts: %3")
               .arg(backend)
               .arg(kReloadCycles)
               .arg(artifactDirectory.absolutePath());
    return static_cast<int>(ExitCode::Success);
}
