#include <Dsqt/TouchEngine/DsTouchEngineSession.h>
#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>
#include <Dsqt/TouchEngine/DsTouchEngineView.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QQuickWindow>
#include <QSize>
#include <QTextStream>
#include <QThread>
#include <QVariantMap>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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
constexpr int kFramesPerEpoch = 30;
constexpr int kIdentityCaptures = 3;
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
    OutputIdentityMismatch = 17,
    NvInteropProbeFailure = 18,
};

constexpr auto kNvInteropProbePrefix = "Dsqt.TouchEngine NV interop probe:";
QMutex nvInteropProbeMutex;
QString nvInteropProbeMessage;
QtMessageHandler previousMessageHandler = nullptr;

void captureMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (message.startsWith(QLatin1String(kNvInteropProbePrefix))) {
        QMutexLocker lock(&nvInteropProbeMutex);
        nvInteropProbeMessage = message;
    }

    if (previousMessageHandler) {
        previousMessageHandler(type, context, message);
        return;
    }

    const QByteArray utf8 = message.toUtf8();
    std::fprintf(stderr, "%s\n", utf8.constData());
    std::fflush(stderr);
}

class MessageCapture final
{
public:
    MessageCapture() { previousMessageHandler = qInstallMessageHandler(captureMessage); }
    ~MessageCapture() { qInstallMessageHandler(previousMessageHandler); }

    MessageCapture(const MessageCapture &) = delete;
    MessageCapture &operator=(const MessageCapture &) = delete;
};

QString capturedNvInteropProbe()
{
    QMutexLocker lock(&nvInteropProbeMutex);
    return nvInteropProbeMessage;
}

bool validateNvInteropProbe(const QString &message, QString *error)
{
    if (message.isEmpty()) {
        if (error)
            *error = QStringLiteral("the OpenGL backend did not publish an NV interop capability probe");
        return false;
    }

    const QStringList fields = {
        QStringLiteral("usable="),
        QStringLiteral("extension="),
        QStringLiteral("entryPoints="),
        QStringLiteral("adapterMatch="),
        QStringLiteral("glRenderer="),
        QStringLiteral("d3d11Adapter="),
    };
    for (const QString &field : fields) {
        if (!message.contains(field)) {
            if (error)
                *error = QStringLiteral("the NV interop probe omitted '%1': %2").arg(field, message);
            return false;
        }
    }

    const bool usable = message.contains(QStringLiteral("usable= yes"));
    if (usable && (!message.contains(QStringLiteral("extension= yes")) ||
                   !message.contains(QStringLiteral("entryPoints= yes")) ||
                   !message.contains(QStringLiteral("adapterMatch= yes")) ||
                   message.contains(QStringLiteral("d3d11Adapter= <none>")))) {
        if (error)
            *error = QStringLiteral("the NV interop probe reported an inconsistent usable result: %1").arg(message);
        return false;
    }
    return true;
}

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
                     Predicate &&predicate,
                     bool driveWindow = true)
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

        if (window && driveWindow)
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

QStringList textureOutputLinks(const QVariantList &links)
{
    QStringList result;
    for (const QVariant &entry : links) {
        const QVariantMap link = entry.toMap();
        const auto scope = link.value(QStringLiteral("scope"))
                               .value<DsTouchEngineTypes::LinkScope>();
        const auto type = link.value(QStringLiteral("type"))
                              .value<DsTouchEngineTypes::LinkType>();
        if (scope == DsTouchEngineTypes::LinkScope::Output
            && type == DsTouchEngineTypes::LinkType::Texture) {
            result.push_back(link.value(QStringLiteral("identifier")).toString());
        }
    }
    return result;
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

QSize outputGrid(int count)
{
    int columns = 1;
    while (columns * columns < count)
        ++columns;
    return QSize(columns, (count + columns - 1) / columns);
}

QRect outputTile(const QSize &windowSize, int count, int index)
{
    const QSize grid = outputGrid(count);
    const int column = index % grid.width();
    const int row = index / grid.width();
    const int left = windowSize.width() * column / grid.width();
    const int right = windowSize.width() * (column + 1) / grid.width();
    const int top = windowSize.height() * row / grid.height();
    const int bottom = windowSize.height() * (row + 1) / grid.height();
    return QRect(left, top, right - left, bottom - top);
}

void setWindowAndViewSize(QQuickWindow &window,
                          const std::vector<DsTouchEngineView *> &views,
                          int activeViewCount,
                          const QSize &size)
{
    window.resize(size);
    for (int index = 0; index < static_cast<int>(views.size()); ++index) {
        DsTouchEngineView *view = views.at(static_cast<size_t>(index));
        const bool active = index < activeViewCount;
        view->setVisible(active);
        if (!active)
            continue;
        const QRect tile = outputTile(size, activeViewCount, index);
        view->setPosition(QPointF(tile.topLeft()));
        view->setSize(QSizeF(tile.size()));
        view->update();
    }
    window.update();
}

bool captureWindow(QQuickWindow &window, const QString &path, QImage *captured, QString *error)
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
    if (captured)
        *captured = image;
    return true;
}

struct ColorSignature
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

std::vector<ColorSignature> outputSignatures(const QImage &source, int outputCount)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    std::vector<ColorSignature> result;
    result.reserve(static_cast<size_t>(outputCount));
    for (int index = 0; index < outputCount; ++index) {
        QRect sample = outputTile(image.size(), outputCount, index);
        sample.adjust(sample.width() / 8, sample.height() / 8,
                      -sample.width() / 8, -sample.height() / 8);
        ColorSignature signature;
        quint64 samples = 0;
        const int step = std::max(1, std::min(sample.width(), sample.height()) / 64);
        for (int y = sample.top(); y <= sample.bottom(); y += step) {
            const auto *pixels = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = sample.left(); x <= sample.right(); x += step) {
                const QRgb pixel = pixels[x];
                signature.red += qRed(pixel);
                signature.green += qGreen(pixel);
                signature.blue += qBlue(pixel);
                ++samples;
            }
        }
        if (samples) {
            signature.red /= static_cast<double>(samples);
            signature.green /= static_cast<double>(samples);
            signature.blue /= static_cast<double>(samples);
        }
        result.push_back(signature);
    }
    return result;
}

double signatureDistance(const ColorSignature &left, const ColorSignature &right)
{
    const double red = left.red - right.red;
    const double green = left.green - right.green;
    const double blue = left.blue - right.blue;
    return std::sqrt(red * red + green * green + blue * blue);
}

bool outputIdentityIsStable(const std::vector<ColorSignature> &baseline,
                            const std::vector<ColorSignature> &current,
                            const QStringList &links,
                            QString *detail)
{
    if (baseline.size() < 2 || baseline.size() != current.size())
        return true;

    double minimumSeparation = std::numeric_limits<double>::max();
    for (size_t left = 0; left < baseline.size(); ++left) {
        for (size_t right = left + 1; right < baseline.size(); ++right) {
            minimumSeparation = std::min(minimumSeparation,
                                         signatureDistance(baseline[left], baseline[right]));
        }
    }
    if (minimumSeparation < 24.0)
        return true; // The outputs are not visually distinct enough for a reliable test.

    int crossedMappings = 0;
    QStringList mappings;
    for (size_t index = 0; index < current.size(); ++index) {
        size_t nearest = 0;
        double nearestDistance = std::numeric_limits<double>::max();
        for (size_t candidate = 0; candidate < baseline.size(); ++candidate) {
            const double distance = signatureDistance(current[index], baseline[candidate]);
            if (distance < nearestDistance) {
                nearest = candidate;
                nearestDistance = distance;
            }
        }
        const double ownDistance = signatureDistance(current[index], baseline[index]);
        if (nearest != index && nearestDistance + 12.0 < ownDistance) {
            ++crossedMappings;
            mappings.push_back(QStringLiteral("%1 -> %2")
                                   .arg(links.at(static_cast<qsizetype>(index)),
                                        links.at(static_cast<qsizetype>(nearest))));
        }
    }
    if (crossedMappings < 2)
        return true;
    if (detail)
        *detail = QStringLiteral("output signatures crossed links: %1").arg(mappings.join(", "));
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    MessageCapture messageCapture;
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
    QFile resultsFile(artifactDirectory.filePath(QStringLiteral("backend-results.txt")));
    if (!resultsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qCritical().noquote() << "Could not create backend results file:"
                              << resultsFile.fileName();
        return static_cast<int>(ExitCode::ArtifactDirectoryFailure);
    }
    QTextStream results(&resultsFile);

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
    std::vector<DsTouchEngineView *> views;
    const auto addView = [&] {
        auto *view = new DsTouchEngineView(window.contentItem());
        view->setClearColor(Qt::black);
        view->setSession(session.get());
        views.push_back(view);
        return view;
    };
    addView();
    session->setComponentPath(toxFile.absoluteFilePath());

    const std::array<QSize, kReloadCycles + 1> epochSizes = {
        QSize(640, 360),
        QSize(977, 541),
        QSize(320, 240),
        QSize(801, 451),
    };
    setWindowAndViewSize(window, views, 1, epochSizes.front());
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
            [&] { return session->isReady(); },
            false);
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

        if (backend == QLatin1String("opengl") || backend == QLatin1String("gl")) {
            const QString probe = capturedNvInteropProbe();
            QString probeError;
            if (!validateNvInteropProbe(probe, &probeError)) {
                qCritical().noquote() << description << probeError;
                return static_cast<int>(ExitCode::NvInteropProbeFailure);
            }
            results << description << " " << probe << '\n';
            results.flush();
        }

        const QStringList outputLinks = textureOutputLinks(session->links());
        if (outputLinks.isEmpty()) {
            qCritical().noquote() << description
                                  << "did not publish an output texture link. Links:"
                                  << session->links();
            return static_cast<int>(ExitCode::MissingTextureOutput);
        }
        while (views.size() < static_cast<size_t>(outputLinks.size()))
            addView();
        for (qsizetype index = 0; index < outputLinks.size(); ++index)
            views.at(static_cast<size_t>(index))->setOutputLink(outputLinks.at(index));
        setWindowAndViewSize(window,
                             views,
                             static_cast<int>(outputLinks.size()),
                             epochSizes.at(static_cast<size_t>(epoch)));

        const quint64 firstFrame = session->frameCount();
        QElapsedTimer frameTimer;
        frameTimer.start();
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

        const quint64 completedFrames = session->frameCount() - firstFrame;
        const double measuredFps = frameTimer.elapsed() > 0
            ? static_cast<double>(completedFrames) * 1000.0
                / static_cast<double>(frameTimer.elapsed())
            : 0.0;
        results << description
                << " api=" << DsTouchEngineTypes::graphicsApiName(session->graphicsApi())
                << " outputs=" << outputLinks.join(QLatin1Char(','))
                << " frames=" << completedFrames
                << " elapsedMs=" << frameTimer.elapsed()
                << " measuredFps=" << QString::number(measuredFps, 'f', 1)
                << '\n';
        results.flush();

        const QSize size = epochSizes.at(static_cast<size_t>(epoch));
        std::vector<ColorSignature> baselineSignatures;
        QStringList capturePaths;
        for (int capture = 0; capture < kIdentityCaptures; ++capture) {
            const int targetSwap = windowState.frameSwaps.load(std::memory_order_relaxed) + 2;
            for (qsizetype index = 0; index < outputLinks.size(); ++index)
                views.at(static_cast<size_t>(index))->update();
            window.update();
            result = waitUntil(
                QStringLiteral("%1: waiting for output presentation %2")
                    .arg(description)
                    .arg(capture + 1),
                kFrameTimeoutMs,
                &window,
                session.get(),
                windowState,
                [&] { return windowState.frameSwaps.load(std::memory_order_relaxed) >= targetSwap; });
            if (!result)
                return reportWaitFailure(result);

            const QString capturePath = artifactDirectory.filePath(
                QStringLiteral("touchengine-%1-epoch-%2-capture-%3-%4x%5.png")
                    .arg(backend)
                    .arg(epoch)
                    .arg(capture)
                    .arg(size.width())
                    .arg(size.height()));
            QImage captured;
            QString captureError;
            if (!captureWindow(window, capturePath, &captured, &captureError)) {
                qCritical().noquote() << description << "capture failed:" << captureError;
                return static_cast<int>(ExitCode::CaptureFailure);
            }
            capturePaths.push_back(capturePath);

            const auto signatures = outputSignatures(captured, static_cast<int>(outputLinks.size()));
            if (capture == 0) {
                baselineSignatures = signatures;
            } else {
                QString identityError;
                if (!outputIdentityIsStable(baselineSignatures,
                                            signatures,
                                            outputLinks,
                                            &identityError)) {
                    qCritical().noquote() << description << identityError
                                          << "capture=" << capturePath;
                    return static_cast<int>(ExitCode::OutputIdentityMismatch);
                }
            }
        }

        qInfo().noquote()
            << QStringLiteral("%1 passed: api=%2 outputs=[%3] frame=%4 measuredFps=%5 captures=[%6]")
                   .arg(description,
                        DsTouchEngineTypes::graphicsApiName(session->graphicsApi()),
                        outputLinks.join(QLatin1Char(',')))
                   .arg(session->frameCount())
                   .arg(measuredFps, 0, 'f', 1)
                   .arg(capturePaths.join(QLatin1Char(',')));
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
    for (DsTouchEngineView *view : views) {
        if (view->session() != nullptr) {
            qCritical().noquote()
                << "Destroying a live DsTouchEngineSession did not detach all DsTouchEngineViews.";
            return static_cast<int>(ExitCode::SessionDetachFailure);
        }
        view->update();
    }

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
    if (!captureWindow(window, detachedCapture, nullptr, &captureError)) {
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
