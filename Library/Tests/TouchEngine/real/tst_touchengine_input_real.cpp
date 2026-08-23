#include <Dsqt/TouchEngine/DsTouchEngineSession.h>
#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>
#include <Dsqt/TouchEngine/DsTouchEngineView.h>

#include <QColor>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSize>
#include <QStringList>
#include <QTextStream>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

using dsqt::touchengine::DsTouchEngineSession;
using dsqt::touchengine::DsTouchEngineTypes;
using dsqt::touchengine::DsTouchEngineView;

#ifdef Q_OS_WIN
// Ask hybrid-GPU systems to place both Qt Quick and TouchEngine on the
// high-performance adapter before either one creates a graphics context.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement                  = 0x00000001;
__declspec(dllexport) int           AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

constexpr int   kQmlTimeoutMs        = 10'000;
constexpr int   kWindowTimeoutMs     = 30'000;
constexpr int   kLoadTimeoutMs       = 60'000;
constexpr int   kLinkTimeoutMs       = 15'000;
constexpr int   kFrameTimeoutMs      = 45'000;
constexpr int   kUnloadTimeoutMs     = 30'000;
constexpr int   kFramesPerValidation = 3;
constexpr int   kPresentationSwaps   = 2;
constexpr int   kColorTolerance      = 48;
constexpr qreal kPaneGap             = 32.0;

const QSize kInitialTextureSize(320, 240);
const QSize kResizedTextureSize(517, 293);
constexpr int kInitialPhase  = 0;
constexpr int kResizedPhase  = 1;
constexpr int kReboundPhase  = 2;
constexpr int kReloadedPhase = 3;

enum class ExitCode : int {
    Success                    = 0,
    InvalidArguments           = 2,
    MissingTox                 = 3,
    ArtifactDirectoryFailure   = 4,
    QmlModuleFailure           = 5,
    QmlRootFailure             = 6,
    VulkanConfigurationFailure = 7,
    SceneGraphFailure          = 10,
    EngineFailure              = 11,
    Timeout                    = 12,
    MissingTextureInput        = 13,
    MissingTextureOutput       = 14,
    GraphicsApiMismatch        = 15,
    CaptureFailure             = 16,
    SourceValidationFailure    = 17,
    OutputValidationFailure    = 18,
    ClearValidationFailure     = 19,
    ReloadLinkFailure          = 20,
    SessionDetachFailure       = 21,
};

enum class WaitFailure {
    None,
    Timeout,
    SceneGraph,
    Engine,
};

struct WaitResult {
    WaitFailure failure = WaitFailure::None;
    QString     detail;

    explicit operator bool() const noexcept { return failure == WaitFailure::None; }
};

struct WindowSignals {
    std::atomic_bool sceneGraphInitialized = false;
    std::atomic_int  frameSwaps            = 0;
    QString          sceneGraphError;
};

struct SessionSignals {
    quint64 completedFrames = 0;
};

struct TextureLinks {
    QString input;
    QString output;

    bool complete() const { return !input.isEmpty() && !output.isEmpty(); }
};

enum class SignaturePosition {
    TopLeftInterior,
    TopRightInterior,
    BottomLeftInterior,
    BottomRightInterior,
    TopLeftMarker,
    TopRightMarker,
    BottomLeftMarker,
    BottomRightMarker,
    TopLeftMarkerOutside,
    TopRightMarkerOutside,
    BottomLeftMarkerOutside,
    BottomRightMarkerOutside,
};

struct SignatureSample {
    const char*       name;
    SignaturePosition position;
    QColor            expected;
    bool              quadrantInterior;
};

std::array<SignatureSample, 12> signatureSamples(int phase) {
    static const std::array<QColor, 4> quadrantColors = {
        QColor(QStringLiteral("#ff0000")), QColor(QStringLiteral("#00ff00")),
        QColor(QStringLiteral("#0000ff")), QColor(QStringLiteral("#ffff00"))};
    static const std::array<QColor, 4> markerColors = {
        QColor(QStringLiteral("#ffffff")), QColor(QStringLiteral("#000000")),
        QColor(QStringLiteral("#ff00ff")), QColor(QStringLiteral("#00ffff"))};
    const auto colorAt = [phase](const auto& colors, int position) -> QColor {
        return colors[static_cast<size_t>((phase + position) % 4)];
    };

    return {
        {{"top-left quadrant", SignaturePosition::TopLeftInterior, colorAt(quadrantColors, 0), true},
         {"top-right quadrant", SignaturePosition::TopRightInterior, colorAt(quadrantColors, 1), true},
         {"bottom-left quadrant", SignaturePosition::BottomLeftInterior, colorAt(quadrantColors, 2), true},
         {"bottom-right quadrant", SignaturePosition::BottomRightInterior, colorAt(quadrantColors, 3), true},
         {"top-left marker", SignaturePosition::TopLeftMarker, colorAt(markerColors, 0), false},
         {"top-right marker", SignaturePosition::TopRightMarker, colorAt(markerColors, 1), false},
         {"bottom-left marker", SignaturePosition::BottomLeftMarker, colorAt(markerColors, 2), false},
         {"bottom-right marker", SignaturePosition::BottomRightMarker, colorAt(markerColors, 3), false},
         {"top-left marker outside edge", SignaturePosition::TopLeftMarkerOutside,
          colorAt(quadrantColors, 0), false},
         {"top-right marker outside edge", SignaturePosition::TopRightMarkerOutside,
          colorAt(quadrantColors, 1), false},
         {"bottom-left marker outside edge", SignaturePosition::BottomLeftMarkerOutside,
          colorAt(quadrantColors, 2), false},
         {"bottom-right marker outside edge", SignaturePosition::BottomRightMarkerOutside,
          colorAt(quadrantColors, 3), false}}};
}

template <typename Predicate>
WaitResult waitUntil(const QString& operation, int timeoutMs, QQuickWindow* window, DsTouchEngineSession* session,
                     const WindowSignals& windowState, Predicate&& predicate) {
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

        if (!windowState.sceneGraphError.isEmpty()) {
            return {WaitFailure::SceneGraph, QStringLiteral("%1: %2").arg(operation, windowState.sceneGraphError)};
        }

        if (session) {
            if (session->state() == DsTouchEngineTypes::State::Error) {
                const QString error = session->errorString().isEmpty()
                                          ? QStringLiteral("TouchEngine entered Error state")
                                          : session->errorString();
                return {WaitFailure::Engine, QStringLiteral("%1: %2").arg(operation, error)};
            }
            if (!session->errorString().isEmpty()) {
                return {WaitFailure::Engine, QStringLiteral("%1: %2").arg(operation, session->errorString())};
            }
        }

        if (std::invoke(std::forward<Predicate>(predicate))) return {};

        if (window) window->update();
        QThread::msleep(2);
    }

    const QString state = session ? DsTouchEngineTypes::stateName(session->state()) : QStringLiteral("n/a");
    const quint64 frame = session ? session->frameCount() : 0;
    return {WaitFailure::Timeout, QStringLiteral("%1 timed out after %2 ms (state=%3, frame=%4)")
                                      .arg(operation)
                                      .arg(timeoutMs)
                                      .arg(state)
                                      .arg(frame)};
}

int reportWaitFailure(const WaitResult& result) {
    qCritical().noquote() << "TouchEngine texture-input test failed:" << result.detail;
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

TextureLinks firstTextureLinks(const QVariantList& links) {
    TextureLinks result;
    for (const QVariant& entry : links) {
        const QVariantMap link  = entry.toMap();
        const auto        scope = link.value(QStringLiteral("scope")).value<DsTouchEngineTypes::LinkScope>();
        const auto        type  = link.value(QStringLiteral("type")).value<DsTouchEngineTypes::LinkType>();
        if (type != DsTouchEngineTypes::LinkType::Texture) continue;

        const QString identifier = link.value(QStringLiteral("identifier")).toString();
        if (scope == DsTouchEngineTypes::LinkScope::Input && result.input.isEmpty()) result.input = identifier;
        if (scope == DsTouchEngineTypes::LinkScope::Output && result.output.isEmpty()) result.output = identifier;
    }
    return result;
}

std::optional<DsTouchEngineTypes::GraphicsApi> expectedGraphicsApi(const QString& backend) {
    if (backend == QStringLiteral("d3d11")) return DsTouchEngineTypes::GraphicsApi::Direct3D11;
    if (backend == QStringLiteral("d3d12")) return DsTouchEngineTypes::GraphicsApi::Direct3D12;
    if (backend == QStringLiteral("vulkan")) return DsTouchEngineTypes::GraphicsApi::Vulkan;
    if (backend == QStringLiteral("opengl") || backend == QStringLiteral("gl"))
        return DsTouchEngineTypes::GraphicsApi::OpenGL;
    return std::nullopt;
}

QString componentErrors(const QQmlComponent& component) {
    QStringList messages;
    for (const QQmlError& error : component.errors())
        messages.push_back(error.toString());
    return messages.join(QLatin1Char('\n'));
}

void setSceneGeometry(QQuickWindow& window, QQuickItem& source, DsTouchEngineView& output, const QSize& textureSize) {
    source.setPosition(QPointF(0.0, 0.0));
    source.setSize(QSizeF(textureSize));
    output.setPosition(QPointF(textureSize.width() + kPaneGap, 0.0));
    output.setSize(QSizeF(textureSize));
    window.resize(textureSize.width() * 2 + static_cast<int>(kPaneGap), textureSize.height());
    output.update();
    window.update();
}

QPointF samplePoint(SignaturePosition position, const QSizeF& size) {
    constexpr qreal markerCenter = 20.0;
    constexpr qreal markerOutsideEdge = 36.0;
    switch (position) {
    case SignaturePosition::TopLeftInterior:
        return {size.width() * 0.25, size.height() * 0.25};
    case SignaturePosition::TopRightInterior:
        return {size.width() * 0.75, size.height() * 0.25};
    case SignaturePosition::BottomLeftInterior:
        return {size.width() * 0.25, size.height() * 0.75};
    case SignaturePosition::BottomRightInterior:
        return {size.width() * 0.75, size.height() * 0.75};
    case SignaturePosition::TopLeftMarker:
        return {markerCenter, markerCenter};
    case SignaturePosition::TopRightMarker:
        return {size.width() - markerCenter, markerCenter};
    case SignaturePosition::BottomLeftMarker:
        return {markerCenter, size.height() - markerCenter};
    case SignaturePosition::BottomRightMarker:
        return {size.width() - markerCenter, size.height() - markerCenter};
    case SignaturePosition::TopLeftMarkerOutside:
        return {markerOutsideEdge, markerCenter};
    case SignaturePosition::TopRightMarkerOutside:
        return {size.width() - markerOutsideEdge, markerCenter};
    case SignaturePosition::BottomLeftMarkerOutside:
        return {markerOutsideEdge, size.height() - markerCenter};
    case SignaturePosition::BottomRightMarkerOutside:
        return {size.width() - markerOutsideEdge, size.height() - markerCenter};
    }
    return {};
}

std::optional<QColor> sampledColor(const QImage& image, const QQuickWindow& window, const QQuickItem& item,
                                   const QPointF& localPoint, QPoint* imagePoint = nullptr) {
    if (image.isNull() || window.width() <= 0 || window.height() <= 0) return std::nullopt;

    const QPointF scenePoint = item.mapToScene(localPoint);
    const qreal   scaleX     = static_cast<qreal>(image.width()) / window.width();
    const qreal   scaleY     = static_cast<qreal>(image.height()) / window.height();
    const QPoint  pixel(qFloor(scenePoint.x() * scaleX), qFloor(scenePoint.y() * scaleY));
    if (!image.rect().contains(pixel)) return std::nullopt;

    if (imagePoint) *imagePoint = pixel;
    return image.pixelColor(pixel);
}

int maximumRgbDistance(const QColor& left, const QColor& right) {
    return std::max({std::abs(left.red() - right.red()), std::abs(left.green() - right.green()),
                     std::abs(left.blue() - right.blue())});
}

bool colorMatches(const QColor& actual, const QColor& expected) {
    return actual.isValid() && maximumRgbDistance(actual, expected) <= kColorTolerance;
}

bool signatureMatches(const QImage& image, const QQuickWindow& window, const QQuickItem& item, int phase) {
    return std::ranges::all_of(signatureSamples(phase), [&](const SignatureSample& sample) {
        const std::optional<QColor> actual = sampledColor(image, window, item, samplePoint(sample.position, item.size()));
        return actual && colorMatches(*actual, sample.expected);
    });
}

bool validateSignature(const QString& stage, const QString& targetName, const QImage& image, const QQuickWindow& window,
                       const QQuickItem& item, int phase) {
    bool valid = true;
    for (const SignatureSample& sample : signatureSamples(phase)) {
        const QPointF               local = samplePoint(sample.position, item.size());
        QPoint                      pixel;
        const std::optional<QColor> actual = sampledColor(image, window, item, local, &pixel);
        if (actual && colorMatches(*actual, sample.expected)) continue;

        valid = false;
        qCritical().noquote()
            << QStringLiteral(
                   "%1 %2 validation failed at %3: local=(%4,%5), image=(%6,%7), expected=%8, actual=%9, tolerance=%10")
                   .arg(stage, targetName, QString::fromLatin1(sample.name))
                   .arg(local.x(), 0, 'f', 1)
                   .arg(local.y(), 0, 'f', 1)
                   .arg(pixel.x())
                   .arg(pixel.y())
                   .arg(sample.expected.name(QColor::HexRgb),
                        actual ? actual->name(QColor::HexRgb) : QStringLiteral("out-of-bounds"))
                   .arg(kColorTolerance);
    }
    return valid;
}

int matchingQuadrantCount(const QImage& image, const QQuickWindow& window, const QQuickItem& item,
                          int phase, QStringList* observations) {
    int matches = 0;
    for (const SignatureSample& sample : signatureSamples(phase)) {
        if (!sample.quadrantInterior) continue;

        const QPointF               local  = samplePoint(sample.position, item.size());
        const std::optional<QColor> actual = sampledColor(image, window, item, local);
        if (actual && colorMatches(*actual, sample.expected)) ++matches;
        if (observations) {
            observations->push_back(
                QStringLiteral("%1=%2").arg(QString::fromLatin1(sample.name),
                                            actual ? actual->name(QColor::HexRgb) : QStringLiteral("out-of-bounds")));
        }
    }
    return matches;
}

bool captureWindow(QQuickWindow& window, const QString& path, QImage* captured, QString* error) {
    QImage image = window.grabWindow();
    if (image.isNull()) {
        if (error) *error = QStringLiteral("QQuickWindow::grabWindow returned an empty image");
        return false;
    }
    if (!image.save(path, "PNG")) {
        if (error) *error = QStringLiteral("could not save PNG to %1").arg(path);
        return false;
    }
    if (captured) *captured = std::move(image);
    return true;
}

int waitForRenderedFrames(const QString& description, int frameCount, QQuickWindow& window, DsTouchEngineView& view,
                          DsTouchEngineSession& session, SessionSignals& sessionState,
                          const WindowSignals& windowState) {
    for (int index = 0; index < frameCount; ++index) {
        const quint64 target = sessionState.completedFrames + 1;
        view.update();
        window.update();

        const WaitResult result = waitUntil(
            QStringLiteral("%1: waiting for fresh frame %2 of %3").arg(description).arg(index + 1).arg(frameCount),
            kFrameTimeoutMs, &window, &session, windowState, [&] { return sessionState.completedFrames >= target; });
        if (!result) return reportWaitFailure(result);
    }

    const int targetSwap = windowState.frameSwaps.load(std::memory_order_relaxed) + kPresentationSwaps;
    view.update();
    window.update();
    const WaitResult presentation = waitUntil(
        QStringLiteral("%1: waiting for output presentation").arg(description), kFrameTimeoutMs, &window, &session,
        windowState, [&] { return windowState.frameSwaps.load(std::memory_order_relaxed) >= targetSwap; });
    return presentation ? static_cast<int>(ExitCode::Success) : reportWaitFailure(presentation);
}

int captureAndValidate(const QString& stage, const QString& backend, const QDir& artifactDirectory,
                       QQuickWindow& window, QQuickItem& source, DsTouchEngineView& output, int phase,
                       DsTouchEngineSession& session, const WindowSignals& windowState) {
    const QString capturePath = artifactDirectory.filePath(QStringLiteral("touchengine-input-%1-%2-%3x%4.png")
                                                               .arg(backend, stage)
                                                               .arg(qRound(source.width()))
                                                               .arg(qRound(source.height())));
    QImage        image;
    QString       error;
    const WaitResult signature = waitUntil(
        QStringLiteral("%1: waiting for the exact texture signature").arg(stage), kFrameTimeoutMs, &window,
        &session, windowState, [&] {
            error.clear();
            if (!captureWindow(window, capturePath, &image, &error)) return false;
            return signatureMatches(image, window, source, phase) && signatureMatches(image, window, output, phase);
        });
    if (!signature) {
        if (signature.failure != WaitFailure::Timeout) return reportWaitFailure(signature);
        qCritical().noquote() << signature.detail;
        if (image.isNull()) {
            qCritical().noquote() << stage << "capture failed:" << error;
            return static_cast<int>(ExitCode::CaptureFailure);
        }
    }

    if (!validateSignature(stage, QStringLiteral("source"), image, window, source, phase)) {
        qCritical().noquote() << "Source validation artifact:" << capturePath;
        return static_cast<int>(ExitCode::SourceValidationFailure);
    }
    if (!validateSignature(stage, QStringLiteral("TouchEngine output"), image, window, output, phase)) {
        qCritical().noquote() << "Output validation artifact:" << capturePath;
        return static_cast<int>(ExitCode::OutputValidationFailure);
    }

    qInfo().noquote() << stage << "signature passed; artifact:" << capturePath;
    return static_cast<int>(ExitCode::Success);
}

int captureAndValidateCleared(const QString& backend, const QDir& artifactDirectory, QQuickWindow& window,
                              QQuickItem& source, DsTouchEngineView& output, int phase) {
    const QString capturePath = artifactDirectory.filePath(QStringLiteral("touchengine-input-%1-cleared-%2x%3.png")
                                                               .arg(backend)
                                                               .arg(qRound(source.width()))
                                                               .arg(qRound(source.height())));
    QImage        image;
    QString       error;
    if (!captureWindow(window, capturePath, &image, &error)) {
        qCritical().noquote() << "Clear capture failed:" << error;
        return static_cast<int>(ExitCode::CaptureFailure);
    }

    if (!validateSignature(QStringLiteral("cleared"), QStringLiteral("source"), image, window, source, phase)) {
        qCritical().noquote() << "Source validation artifact:" << capturePath;
        return static_cast<int>(ExitCode::SourceValidationFailure);
    }

    QStringList observations;
    const int   matches = matchingQuadrantCount(image, window, output, phase, &observations);
    // A cleared TouchEngine texture may be black, transparent, or another
    // component-defined empty state. Its four colored quadrant interiors must
    // no longer reproduce the bound source signature.
    if (matches > 1) {
        qCritical().noquote() << QStringLiteral("Clearing the texture input left %1 of 4 source quadrants visible; "
                                                "expected at most 1. Observed: %2. Artifact: %3")
                                     .arg(matches)
                                     .arg(observations.join(QStringLiteral(", ")), capturePath);
        return static_cast<int>(ExitCode::ClearValidationFailure);
    }

    qInfo().noquote() << "Texture clear changed the output signature; artifact:" << capturePath;
    return static_cast<int>(ExitCode::Success);
}

} // namespace

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("tst_touchengine_input_real"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Real TouchEngine QML texture-input round-trip test"));
    const QCommandLineOption helpOption    = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    const QCommandLineOption toxOption({QStringLiteral("t"), QStringLiteral("tox")},
                                       QStringLiteral("TouchDesigner component to load."), QStringLiteral("path"));
    const QCommandLineOption artifactOption({QStringLiteral("a"), QStringLiteral("artifact-dir")},
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
        QTextStream(stdout) << QCoreApplication::applicationName() << ' ' << QCoreApplication::applicationVersion()
                            << '\n';
        return static_cast<int>(ExitCode::Success);
    }
    if (!parser.isSet(toxOption) || !parser.isSet(artifactOption)) {
        qCritical().noquote() << "Both --tox <path> and --artifact-dir <path> are required.";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    const QFileInfo toxFile(QDir::current().absoluteFilePath(parser.value(toxOption)));
    if (!toxFile.exists() || !toxFile.isFile()) {
        qCritical().noquote() << "TouchEngine .tox does not exist:" << toxFile.absoluteFilePath();
        return static_cast<int>(ExitCode::MissingTox);
    }

    const QString artifactPath = QDir::cleanPath(QDir::current().absoluteFilePath(parser.value(artifactOption)));
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

    QQmlEngine    qmlEngine;
    QQmlComponent patternComponent(&qmlEngine);
    patternComponent.loadFromModule(QStringLiteral("Dsqt.TouchEngine.InputTest"),
                                    QStringLiteral("TextureInputPattern"));
    if (patternComponent.isLoading()) {
        QElapsedTimer timer;
        timer.start();
        while (patternComponent.isLoading() && timer.elapsed() < kQmlTimeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(2);
        }
    }
    if (patternComponent.isLoading() || patternComponent.isError()) {
        const QString detail = patternComponent.isLoading()
                                   ? QStringLiteral("loading timed out after %1 ms").arg(kQmlTimeoutMs)
                                   : componentErrors(patternComponent);
        qCritical().noquote() << "Could not load Dsqt.TouchEngine.InputTest/TextureInputPattern:" << detail;
        return static_cast<int>(ExitCode::QmlModuleFailure);
    }

    std::unique_ptr<QObject> createdPattern(patternComponent.create());
    if (!createdPattern) {
        qCritical().noquote() << "Could not instantiate Dsqt.TouchEngine.InputTest/TextureInputPattern:"
                              << componentErrors(patternComponent);
        return static_cast<int>(ExitCode::QmlRootFailure);
    }
    auto* source = qobject_cast<QQuickItem*>(createdPattern.get());
    if (!source) {
        qCritical().noquote() << "TextureInputPattern must have a QQuickItem root; actual meta-object:"
                              << createdPattern->metaObject()->className();
        return static_cast<int>(ExitCode::QmlRootFailure);
    }

    const QString backend = qEnvironmentVariable("QSG_RHI_BACKEND", "default").toLower();
    qInfo().noquote() << "Starting TouchEngine texture-input test with QSG_RHI_BACKEND=" << backend
                      << "tox=" << toxFile.absoluteFilePath();

    WindowSignals  windowState;
    SessionSignals sessionState;
    QQuickWindow   window;
    window.setTitle(QStringLiteral("DsQt TouchEngine input test (%1)").arg(backend));
    window.setColor(Qt::black);

    QObject::connect(
        &window, &QQuickWindow::sceneGraphInitialized, &window,
        [&windowState] { windowState.sceneGraphInitialized.store(true, std::memory_order_release); },
        Qt::DirectConnection);
    QObject::connect(
        &window, &QQuickWindow::frameSwapped, &window,
        [&windowState] { windowState.frameSwaps.fetch_add(1, std::memory_order_relaxed); }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::sceneGraphError, &application,
                     [&windowState](QQuickWindow::SceneGraphError code, const QString& message) {
                         windowState.sceneGraphError =
                             QStringLiteral("scene graph error %1: %2").arg(static_cast<int>(code)).arg(message);
                     });

    source->setParent(window.contentItem());
    source->setParentItem(window.contentItem());
    QQmlEngine::setObjectOwnership(source, QQmlEngine::CppOwnership);
    createdPattern.release();

    auto session = std::make_unique<DsTouchEngineSession>();
    session->setComponentPath(toxFile.absoluteFilePath());
    QObject::connect(session.get(), &DsTouchEngineSession::stateChanged, &application, [&] {
        qInfo().noquote() << "TouchEngine state:"
                          << DsTouchEngineTypes::stateName(session->state());
    });
    QObject::connect(session.get(), &DsTouchEngineSession::graphicsApiChanged, &application, [&] {
        qInfo().noquote() << "TouchEngine graphics API:"
                          << DsTouchEngineTypes::graphicsApiName(session->graphicsApi());
    });
    QObject::connect(session.get(), &DsTouchEngineSession::errorStringChanged, &application, [&] {
        if (!session->errorString().isEmpty())
            qWarning().noquote() << "TouchEngine diagnostic:" << session->errorString();
    });
    QObject::connect(session.get(), &DsTouchEngineSession::frameFinished, &application,
                     [&sessionState](quint64) { ++sessionState.completedFrames; });

    auto* view = new DsTouchEngineView(window.contentItem());
    view->setClearColor(Qt::black);
    view->setSession(session.get());

    setSceneGeometry(window, *source, *view, kInitialTextureSize);
    if (!DsTouchEngineView::configureVulkanInterop(&window)) {
        qCritical().noquote() << "Vulkan interop could not be configured before scene-graph initialization.";
        return static_cast<int>(ExitCode::VulkanConfigurationFailure);
    }

    // Loading before the first render deliberately exercises the durable
    // WaitingForRenderer path. The renderer replays this request as soon as
    // its QRhi and TouchEngine instance exist.
    session->load();
    window.show();

    WaitResult waitResult = waitUntil(QStringLiteral("initializing the Qt Quick scene graph and QML texture provider"),
                                      kWindowTimeoutMs, &window, session.get(), windowState, [&] {
                                          return windowState.sceneGraphInitialized.load(std::memory_order_acquire) &&
                                                 window.isExposed() && source->isTextureProvider();
                                      });
    if (!waitResult) return reportWaitFailure(waitResult);

    view->update();
    window.update();
    waitResult = waitUntil(QStringLiteral("initial load: waiting for Ready"), kLoadTimeoutMs, &window, session.get(),
                           windowState, [&] { return session->isReady(); });
    if (!waitResult) return reportWaitFailure(waitResult);

    if (const auto expected = expectedGraphicsApi(backend); expected && session->graphicsApi() != *expected) {
        qCritical().noquote() << QStringLiteral("Initial load selected %1, expected %2 for QSG_RHI_BACKEND=%3")
                                     .arg(DsTouchEngineTypes::graphicsApiName(session->graphicsApi()),
                                          DsTouchEngineTypes::graphicsApiName(*expected), backend);
        return static_cast<int>(ExitCode::GraphicsApiMismatch);
    }

    TextureLinks links;
    waitResult = waitUntil(QStringLiteral("initial load: discovering texture input and output links"), kLinkTimeoutMs,
                           &window, session.get(), windowState, [&] {
                               links = firstTextureLinks(session->links());
                               return links.complete();
                           });
    if (!waitResult) {
        if (waitResult.failure != WaitFailure::Timeout) return reportWaitFailure(waitResult);
        qCritical().noquote() << "The loaded .tox did not publish the required texture links."
                              << "texture input=" << (links.input.isEmpty() ? QStringLiteral("missing") : links.input)
                              << "texture output="
                              << (links.output.isEmpty() ? QStringLiteral("missing") : links.output)
                              << "links=" << session->links();
        return links.input.isEmpty() ? static_cast<int>(ExitCode::MissingTextureInput)
                                     : static_cast<int>(ExitCode::MissingTextureOutput);
    }

    qInfo().noquote() << "Discovered texture links: input=" << links.input << "output=" << links.output;
    view->setOutputLink(links.output);
    session->setTextureInput(links.input, source);

    int result = waitForRenderedFrames(QStringLiteral("initial round trip"), kFramesPerValidation, window, *view,
                                       *session, sessionState, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;
    result = captureAndValidate(QStringLiteral("initial"), backend, artifactDirectory, window, *source, *view,
                                kInitialPhase, *session, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;

    source->setProperty("phase", kResizedPhase);
    setSceneGeometry(window, *source, *view, kResizedTextureSize);
    result = waitForRenderedFrames(QStringLiteral("resized round trip"), kFramesPerValidation, window, *view, *session,
                                   sessionState, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;
    result = captureAndValidate(QStringLiteral("resized"), backend, artifactDirectory, window, *source, *view,
                                kResizedPhase, *session, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;

    if (backend == QLatin1String("vulkan")) {
        // The Vulkan backend intentionally bounds each link's active export pool.
        // Exercise more descriptor generations than that bound so a reusable old
        // size must be retired instead of silently preserving stale input.
        const std::array<QSize, 4> churnSizes = {
            QSize(421, 251), QSize(443, 263), QSize(467, 277), QSize(491, 283)};
        for (std::size_t i = 0; i < churnSizes.size(); ++i) {
            const int phase = (kResizedPhase + int(i) + 1) % 4;
            const QString stage = QStringLiteral("pool-resize-%1").arg(i + 1);
            source->setProperty("phase", phase);
            setSceneGeometry(window, *source, *view, churnSizes[i]);
            result = waitForRenderedFrames(stage, kFramesPerValidation, window, *view, *session,
                                           sessionState, windowState);
            if (result != static_cast<int>(ExitCode::Success)) return result;
            result = captureAndValidate(stage, backend, artifactDirectory, window, *source, *view,
                                        phase, *session, windowState);
            if (result != static_cast<int>(ExitCode::Success)) return result;
        }
    }

    source->setProperty("phase", kReboundPhase);
    session->clearTextureInput(links.input);
    result = waitForRenderedFrames(QStringLiteral("cleared texture input"), kFramesPerValidation, window, *view,
                                   *session, sessionState, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;
    result = captureAndValidateCleared(backend, artifactDirectory, window, *source, *view, kReboundPhase);
    if (result != static_cast<int>(ExitCode::Success)) return result;

    session->setTextureInput(links.input, source);
    result = waitForRenderedFrames(QStringLiteral("rebound texture input"), kFramesPerValidation, window, *view,
                                   *session, sessionState, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;
    result = captureAndValidate(QStringLiteral("rebound"), backend, artifactDirectory, window, *source, *view,
                                kReboundPhase, *session, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;

    session->unload();
    waitResult = waitUntil(QStringLiteral("reload cycle: waiting for unload"), kUnloadTimeoutMs, &window, session.get(),
                           windowState, [&] { return session->state() == DsTouchEngineTypes::State::Idle; });
    if (!waitResult) return reportWaitFailure(waitResult);

    source->setProperty("phase", kReloadedPhase);
    window.update();
    session->reload();
    waitResult = waitUntil(QStringLiteral("reload cycle: waiting for Ready"), kLoadTimeoutMs, &window, session.get(),
                           windowState, [&] { return session->isReady(); });
    if (!waitResult) return reportWaitFailure(waitResult);

    if (const auto expected = expectedGraphicsApi(backend); expected && session->graphicsApi() != *expected) {
        qCritical().noquote() << QStringLiteral("Reload selected %1, expected %2 for QSG_RHI_BACKEND=%3")
                                     .arg(DsTouchEngineTypes::graphicsApiName(session->graphicsApi()),
                                          DsTouchEngineTypes::graphicsApiName(*expected), backend);
        return static_cast<int>(ExitCode::GraphicsApiMismatch);
    }

    TextureLinks reloadedLinks;
    waitResult = waitUntil(QStringLiteral("reload cycle: rediscovering texture links"), kLinkTimeoutMs, &window,
                           session.get(), windowState, [&] {
                               reloadedLinks = firstTextureLinks(session->links());
                               return reloadedLinks.complete();
                           });
    if (!waitResult) {
        if (waitResult.failure != WaitFailure::Timeout) return reportWaitFailure(waitResult);
        qCritical().noquote() << "Reload did not restore both texture links. Links:" << session->links();
        return static_cast<int>(ExitCode::ReloadLinkFailure);
    }
    if (reloadedLinks.input != links.input || reloadedLinks.output != links.output) {
        qCritical().noquote() << "Reload changed the texture link identifiers for the same .tox:"
                              << "before input=" << links.input << "output=" << links.output
                              << "after input=" << reloadedLinks.input << "output=" << reloadedLinks.output;
        return static_cast<int>(ExitCode::ReloadLinkFailure);
    }

    view->setOutputLink(reloadedLinks.output);
    // Do not rebind here: the successful capture verifies that the session's
    // texture binding survives an explicit unload/load lifecycle.
    result = waitForRenderedFrames(QStringLiteral("reloaded round trip"), kFramesPerValidation, window, *view,
                                   *session, sessionState, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;
    result = captureAndValidate(QStringLiteral("reloaded"), backend, artifactDirectory, window, *source, *view,
                                kReloadedPhase, *session, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;

    session->clearTextureInput(reloadedLinks.input);
    result = waitForRenderedFrames(QStringLiteral("final texture-input clear"), 1, window, *view, *session,
                                   sessionState, windowState);
    if (result != static_cast<int>(ExitCode::Success)) return result;

    session->unload();
    waitResult =
        waitUntil(QStringLiteral("final teardown: waiting for unload"), kUnloadTimeoutMs, &window, session.get(),
                  windowState, [&] { return session->state() == DsTouchEngineTypes::State::Idle; });
    if (!waitResult) return reportWaitFailure(waitResult);

    const int detachSwapTarget = windowState.frameSwaps.load(std::memory_order_relaxed) + kPresentationSwaps;
    session.reset();
    if (view->session() != nullptr) {
        qCritical().noquote() << "Destroying DsTouchEngineSession did not detach its DsTouchEngineView.";
        return static_cast<int>(ExitCode::SessionDetachFailure);
    }

    view->update();
    window.update();
    waitResult = waitUntil(QStringLiteral("final teardown: rendering after session destruction"), kFrameTimeoutMs,
                           &window, nullptr, windowState,
                           [&] { return windowState.frameSwaps.load(std::memory_order_relaxed) >= detachSwapTarget; });
    if (!waitResult) return reportWaitFailure(waitResult);

    const QString detachedCapture =
        artifactDirectory.filePath(QStringLiteral("touchengine-input-%1-after-session-destruction.png").arg(backend));
    QImage  detachedImage;
    QString captureError;
    if (!captureWindow(window, detachedCapture, &detachedImage, &captureError)) {
        qCritical().noquote() << "Post-destruction capture failed:" << captureError;
        return static_cast<int>(ExitCode::CaptureFailure);
    }
    if (!validateSignature(QStringLiteral("post-destruction"), QStringLiteral("source"), detachedImage, window,
                           *source, kReloadedPhase)) {
        qCritical().noquote() << "Post-destruction source artifact:" << detachedCapture;
        return static_cast<int>(ExitCode::SourceValidationFailure);
    }

    qInfo().noquote()
        << QStringLiteral(
               "TouchEngine texture-input test passed for %1: input=%2, output=%3, completed frames=%4, artifacts=%5")
               .arg(backend, links.input, links.output)
               .arg(sessionState.completedFrames)
               .arg(artifactDirectory.absolutePath());
    return static_cast<int>(ExitCode::Success);
}
