#include "dsCaptureSource.h"
#include <QSignalSpy>
#include <QtTest>
#include <thread>

namespace {
const DsCaptureFormat hd{QSize(1920, 1080), 30, QVideoFrameFormat::Format_NV12};
const DsCaptureDevice card{QByteArray("card-id"), QString("Capture Card"), {hd}};
struct State { int starts = 0, stops = 0; QPointer<DsCaptureBackend> backend; QList<DsCaptureDevice> devices{card}; QList<DsCaptureFormat> attempts; };
class FakeBackend : public DsCaptureBackend {
public:
    explicit FakeBackend(QSharedPointer<State> state) : state(std::move(state)) { this->state->backend = this; }
    void start(const DsCaptureDevice&, const DsCaptureFormat& format) override { ++state->starts; state->attempts.append(format); running = true; }
    void stop() override { if (running) ++state->stops; running = false; }
    QSharedPointer<State> state;
    bool running = false;
};
QVideoFrame frame(QColor color) {
    QImage image(64, 36, QImage::Format_RGB32); image.fill(color); return QVideoFrame(image);
}
}

class CaptureTest : public QObject {
    Q_OBJECT
    QSharedPointer<State> state;
private slots:
    void init() {
        state = QSharedPointer<State>::create();
        DsCaptureSession::setHardwareEnabled(true);
        QVERIFY(DsCaptureSession::setBackendProvider([s = state] { return s->devices; },
            [s = state] { return std::make_unique<FakeBackend>(s); }));
    }
    void cleanup() {
        QVERIFY(DsCaptureSession::setBackendProvider());
        state.reset();
    }
    void deviceMatchingNeverFallsBack() {
        QString error;
        QVERIFY(DsCaptureSession::selectDevice({card}, "Wrong name", {}, &error).id.isEmpty());
        QVERIFY(!error.isEmpty());
        QCOMPARE(DsCaptureSession::selectDevice({card}, card.name, {}, &error).id, card.id);
        QVERIFY(error.isEmpty());
        auto duplicate = card; duplicate.id = "other-id";
        QVERIFY(DsCaptureSession::selectDevice({card, duplicate}, card.name, {}, &error).id.isEmpty());
        QCOMPARE(DsCaptureSession::selectDevice({card, duplicate}, card.name, duplicate.id.toHex(), &error).id, duplicate.id);
        QVERIFY(DsCaptureSession::selectDevice({card}, card.name, "invalid-id", &error).id.isEmpty());
    }
    void formatSelection() {
        const QList<DsCaptureFormat> formats{{QSize(640, 480), 60, QVideoFrameFormat::Format_NV12},
            {QSize(1280, 720), 30, QVideoFrameFormat::Format_NV12}, hd, {}};
        QCOMPARE(DsCaptureSession::selectFormat(formats, QSize(1920, 1080)).size, QSize(1920, 1080));
        QCOMPARE(DsCaptureSession::selectFormat(formats, QSize(1366, 768)).size, QSize(1280, 720));
        QVERIFY(DsCaptureSession::selectFormat({}, QSize(1920, 1080)).size.isEmpty());
    }
    void sharedSessionAndFanout() {
        QVideoSink oneSink, twoSink;
        DsCaptureSource one, two;
        one.setDeviceName(card.name); one.setVideoSink(&oneSink); one.setActive(true);
        two.setDeviceName(card.name); two.setVideoSink(&twoSink); two.setActive(true);
        QTRY_COMPARE(state->starts, 1);
        emit state->backend->frameReady(frame(Qt::red));
        QTRY_VERIFY(one.ready() && two.ready());
        QCOMPARE(oneSink.videoFrame().toImage().pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(twoSink.videoFrame().toImage().pixelColor(0, 0), QColor(Qt::red));
        one.setActive(false); QCOMPARE(state->stops, 0); QVERIFY(!oneSink.videoFrame().isValid());
        emit state->backend->frameReady(frame(Qt::blue));
        QTRY_COMPARE(twoSink.videoFrame().toImage().pixelColor(0, 0), QColor(Qt::blue));
        two.setActive(false); QCOMPARE(state->stops, 1); QVERIFY(!twoSink.videoFrame().isValid());
    }
    void workerFramesCoalesce() {
        auto session = DsCaptureSession::acquire(card, hd.size);
        QTRY_COMPARE(state->starts, 1);
        QSignalSpy delivered(session.get(), &DsCaptureSession::frameReady);
        auto* backend = state->backend.data();
        std::thread worker([backend] {
            for (int i = 0; i < 100; ++i) emit backend->frameReady(frame(QColor(i, 0, 0)));
        });
        worker.join();
        QTRY_COMPARE(delivered.size(), 1);
        QCOMPARE(session->frame().toImage().pixelColor(0, 0), QColor(99, 0, 0));
    }
    void lastViewerCanCloseDuringFrame() {
        QVideoSink sink;
        auto source = std::make_unique<DsCaptureSource>();
        source->setDeviceName(card.name); source->setVideoSink(&sink); source->setActive(true);
        QTRY_COMPARE(state->starts, 1);
        bool closed = false;
        connect(&sink, &QVideoSink::videoFrameChanged, this, [&](const QVideoFrame& delivered) {
            if (!delivered.isValid()) return;
            source.reset(); closed = true;
            // The session must survive until its frame notification has returned.
            QVERIFY(state->backend);
        });
        emit state->backend->frameReady(frame(Qt::red));
        QTRY_VERIFY(closed);
        QVERIFY(!state->backend); QCOMPARE(state->stops, 1);
        QVERIFY(!sink.videoFrame().isValid());
    }
    void lastLeaseCanCloseDuringStateChange_data() {
        QTest::addColumn<bool>("onFailure");
        QTest::newRow("start") << false;
        QTest::newRow("failure") << true;
    }
    void lastLeaseCanCloseDuringStateChange() {
        QFETCH(bool, onFailure);
        auto session = DsCaptureSession::acquire(card, hd.size);
        QPointer<DsCaptureSession> observed = session.get();
        connect(session.get(), &DsCaptureSession::changed, this, [&] {
            if (onFailure && observed->errorString().isEmpty()) return;
            session.reset();
            QVERIFY(observed); QVERIFY(state->backend);
        });
        if (onFailure) {
            QTRY_COMPARE(state->starts, 1);
            emit state->backend->failed("Device in use");
        }
        QTRY_VERIFY(!observed);
        QVERIFY(!state->backend); QCOMPARE(state->starts, 1); QCOMPARE(state->stops, 1);
    }
    void disconnectReconnectAndBusyRecovery() {
        QVideoSink sink;
        DsCaptureSource source; source.setDeviceName(card.name); source.setVideoSink(&sink); source.setActive(true);
        QTRY_COMPARE(state->starts, 1);
        emit state->backend->frameReady(frame(Qt::green)); QTRY_VERIFY(source.ready());
        emit state->backend->failed("Device in use");
        QTRY_COMPARE(source.errorString(), QString("Device in use"));
        QVERIFY(!source.ready()); QVERIFY(!sink.videoFrame().isValid());
        source.refresh(); QTRY_COMPARE(state->starts, 2);
        emit state->backend->frameReady(frame(Qt::red)); QTRY_VERIFY(source.ready());
        state->devices.clear(); source.refresh();
        QVERIFY(!source.ready()); QVERIFY(source.error() != 0); QCOMPARE(state->stops, 2);
        state->devices = {card};
        QTRY_COMPARE_WITH_TIMEOUT(state->starts, 3, 3500);
        emit state->backend->frameReady(frame(Qt::blue)); QTRY_VERIFY(source.ready());
        QCOMPARE(source.error(), 0);
    }
    void noFramesTimeoutAndRetry() {
        auto session = DsCaptureSession::acquire(card, hd.size);
        QTRY_COMPARE(state->starts, 1);
        QTRY_VERIFY_WITH_TIMEOUT(!session->errorString().isEmpty(), 7000);
        QVERIFY(!session->ready()); QCOMPARE(state->stops, 1);
        QTRY_COMPARE_WITH_TIMEOUT(state->starts, 2, 3500);
        emit state->backend->frameReady(frame(Qt::red)); QTRY_VERIFY(session->ready());
    }
    void failedModeTriesAnotherAtSameResolution() {
        auto device = card;
        auto fast = hd; fast.maxFrameRate = 60;
        device.formats = {fast, hd, {QSize(640, 480), 30, QVideoFrameFormat::Format_NV12}};
        auto session = DsCaptureSession::acquire(device, hd.size);
        QTRY_COMPARE(state->starts, 1); QCOMPARE(state->attempts.first().maxFrameRate, 60);
        emit state->backend->failed("Unsupported native mode");
        QTRY_VERIFY(!session->errorString().isEmpty()); session->retry();
        QTRY_COMPARE(state->starts, 2); QCOMPARE(state->attempts.last().maxFrameRate, 30);
        QCOMPARE(state->attempts.last().size, hd.size);
        emit state->backend->frameReady(frame(Qt::red)); QTRY_VERIFY(session->ready());
        emit state->backend->failed("Temporary resource error");
        QTRY_VERIFY(!session->ready()); session->retry();
        QTRY_COMPARE(state->starts, 3); QCOMPARE(state->attempts.last().maxFrameRate, 30);
    }
    void disabledCaptureAndSinkDestruction() {
        DsCaptureSession::setHardwareEnabled(false);
        DsCaptureSource source; source.setDeviceName(card.name); source.setActive(true);
        QTRY_VERIFY(source.error() != 0); QCOMPARE(state->starts, 0);
        DsCaptureSession::setHardwareEnabled(true); source.refresh(); QTRY_COMPARE(state->starts, 1);
        auto sink = std::make_unique<QVideoSink>(); source.setVideoSink(sink.get()); sink.reset();
        emit state->backend->frameReady(frame(Qt::red)); QTRY_VERIFY(source.ready());
        QVERIFY(!source.videoSink());
        DsCaptureSession::setHardwareEnabled(false); QVERIFY(!source.ready());
        QCOMPARE(state->stops, 1);
    }
};
QTEST_MAIN(CaptureTest)
#include "tst_dscapture.moc"
