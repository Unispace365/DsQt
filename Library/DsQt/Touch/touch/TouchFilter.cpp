#include "TouchFilter.h"
#include "settings/dsSettings.h"

#include <QCoreApplication>
#include <QPointerEvent>
#include <QQuickWindow>
#include <QTouchEvent>
#include <QtMath>

// ── Lifecycle ─────────────────────────────────────────────────────────────────

TouchFilter::TouchFilter(QObject *parent) : QObject(parent)
{
    if (auto s = dsqt::DsSettings::getSettings("engine")) {
        setTransientThresholdMs (static_cast<int>(s->getOr<int64_t>("engine.touch_filter.transientThresholdMs",  m_transientThresholdMs)));
        setSmoothingFactor      (s->getOr<double>("engine.touch_filter.smoothingFactor",       m_smoothingFactor));
        setLiftResumeThresholdMs(static_cast<int>(s->getOr<int64_t>("engine.touch_filter.liftResumeThresholdMs", m_liftResumeThresholdMs)));
        setLiftResumeDistancePx (s->getOr<double>("engine.touch_filter.liftResumeDistancePx",  m_liftResumeDistancePx));
        setProximityFilterPx    (s->getOr<double>("engine.touch_filter.proximityFilterPx",     m_proximityFilterPx));
        setFilterEnabled        (s->getOr<bool>  ("engine.touch_filter.filterEnabled",         m_filterEnabled));
    }
}

TouchFilter::~TouchFilter()
{
    reset();
    if (m_window)
        m_window->removeEventFilter(this);
}

// ── Property setters ──────────────────────────────────────────────────────────

void TouchFilter::setWindow(QQuickWindow *w)
{
    if (m_window == w) return;
    if (m_window) m_window->removeEventFilter(this);
    m_window = w;
    if (m_window) m_window->installEventFilter(this);
    emit windowChanged();
}

void TouchFilter::setTransientThresholdMs(int ms)
{
    if (m_transientThresholdMs == ms) return;
    m_transientThresholdMs = ms;
    emit transientThresholdMsChanged();
}

void TouchFilter::setSmoothingFactor(qreal factor)
{
    factor = qBound(0.0, factor, 1.0);
    if (qFuzzyCompare(m_smoothingFactor, factor)) return;
    m_smoothingFactor = factor;
    emit smoothingFactorChanged();
}

void TouchFilter::setLiftResumeThresholdMs(int ms)
{
    if (m_liftResumeThresholdMs == ms) return;
    m_liftResumeThresholdMs = ms;
    emit liftResumeThresholdMsChanged();
}

void TouchFilter::setLiftResumeDistancePx(qreal px)
{
    if (qFuzzyCompare(m_liftResumeDistancePx, px)) return;
    m_liftResumeDistancePx = px;
    emit liftResumeDistancePxChanged();
}

void TouchFilter::setProximityFilterPx(qreal px)
{
    px = qMax(0.0, px);
    if (qFuzzyCompare(m_proximityFilterPx, px)) return;
    m_proximityFilterPx = px;
    emit proximityFilterPxChanged();
}

void TouchFilter::setFilterEnabled(bool enabled)
{
    if (m_filterEnabled == enabled) return;
    m_filterEnabled = enabled;
    emit filterEnabledChanged();
}

// ── Reset ─────────────────────────────────────────────────────────────────────

void TouchFilter::reset()
{
    for (TouchData *data : std::as_const(m_touches)) {
        if (data->pendingTimer) { data->pendingTimer->stop(); delete data->pendingTimer; }
        if (data->liftTimer)    { data->liftTimer->stop();   delete data->liftTimer;    }
        delete data;
    }
    m_touches.clear();
    emit wasReset();
}

// ── QObject event filter ──────────────────────────────────────────────────────

bool TouchFilter::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    // Let re-injected events (from confirmPress) pass through untouched.
    if (m_reinjecting) return false;

    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        break;
    default:
        return false;
    }

    auto *te = static_cast<QTouchEvent *>(event);
    const QList<QEventPoint> &pts = te->points();
    const QPointF winOffset = m_window->position();

    // When disabled: emit signals for visualisation, then deliver the event.
    if (!m_filterEnabled) {
        for (const QEventPoint &pt : pts) {
            const QPointF sp = pt.globalPosition() - winOffset;
            const int s = stateInt(pt.state());
            emit touchRaw(pt.id(), sp.x(), sp.y(), s);
            emit touchAccepted(pt.id(), sp.x(), sp.y(), sp.x(), sp.y(), s);
        }
#ifdef DSQT_TOUCH_PRIVATE_REINJECT
        // On Windows LWE displays, QEventPoint::scenePosition() == globalPosition()
        // (screen-absolute) instead of being window-relative.  TapHandler and other
        // PointerHandlers check scenePosition() against the item's bounds; with raw LWE
        // coordinates the point appears far outside any item and the handler rejects it,
        // so double-tap, long-press, and drag gestures never activate.
        //
        // Re-delivering via QWindowSystemInterface routes the event through Qt's internal
        // coordinate-conversion pipeline, which produces correct window-relative values.
        // The original LWE event is suppressed; the re-delivered one is what the scene sees.
        QList<QWindowSystemInterface::TouchPoint> tps;
        tps.reserve(pts.size());
        for (const QEventPoint &pt : pts)
            tps.append(toTouchPoint(pt, m_window));
        m_reinjecting = true;
        QWindowSystemInterface::handleTouchEvent<QWindowSystemInterface::SynchronousDelivery>(
            m_window, te->pointingDevice(), tps, te->modifiers());
        m_reinjecting = false;
        return true;   // suppress original — re-delivered above with corrected coordinates
#else
        return false;  // pass original through unchanged (TapHandler may not work on LWE)
#endif
    }

    QList<QEventPoint> passThrough;
    bool anySuppressed = false;

    for (const QEventPoint &pt : pts) {
        // On Windows with LWE touch, scenePosition() == globalPosition() (screen-absolute).
        // Subtract the window's screen position to get window-relative coordinates.
        const QPointF sp = pt.globalPosition() - winOffset;

        emit touchRaw(pt.id(), sp.x(), sp.y(), stateInt(pt.state()));

        if (filterPoint(te, pt, sp)) {
            anySuppressed = true;
        } else {
            passThrough.append(pt);
        }
    }

    if (!anySuppressed)     return false;  // nothing changed — pass original
    if (passThrough.isEmpty()) return true; // everything suppressed — drop event

    // Some points suppressed, others not — rebuild and re-send without the
    // suppressed points, then block the original.
    QTouchEvent modified(te->type(), te->pointingDevice(),
                         te->modifiers(), passThrough);
    m_reinjecting = true;
    QCoreApplication::sendEvent(m_window, &modified);
    m_reinjecting = false;
    return true;
}

// ── Per-point state machine ───────────────────────────────────────────────────

bool TouchFilter::filterPoint(QTouchEvent *event, const QEventPoint &pt, const QPointF &sp)
{
    const int    id = pt.id();

    switch (pt.state()) {

    // ── PRESSED ───────────────────────────────────────────────────────────────
    case QEventPoint::Pressed: {

        // 1. Proximity rejection — immediate suppress.
        if (m_proximityFilterPx > 0.0) {
            for (const TouchData *data : std::as_const(m_touches)) {
                if (data->state == PointState::Active  ||
                    data->state == PointState::Pending ||
                    data->state == PointState::Lifting)
                {
                    if (dist(sp, data->smoothPos) <= m_proximityFilterPx) {
                        auto *filtered      = new TouchData;
                        filtered->state     = PointState::Filtered;
                        filtered->smoothPos = sp;
                        m_touches[id]       = filtered;
                        emit touchFiltered(id, sp.x(), sp.y(), 0,
                                           QStringLiteral("proximity"));
                        emit touchReclassified(id, true,
                                               QStringLiteral("proximity"));
                        return true;
                    }
                }
            }
        }

        // 2. Lift-resume detection — absorb if a nearby Lifting touch exists.
        for (auto it = m_touches.begin(); it != m_touches.end(); ++it) {
            TouchData *data = it.value();
            if (data->state != PointState::Lifting) continue;
            if (dist(sp, data->liftPos) > m_liftResumeDistancePx) continue;

            const int oldId = it.key();
            if (data->liftTimer) {
                data->liftTimer->stop();
                delete data->liftTimer;
                data->liftTimer = nullptr;
            }
            m_touches.erase(it);
            m_touches[id] = data;
            data->state   = PointState::Active;

            emit touchFiltered(oldId, data->liftPos.x(), data->liftPos.y(), 2,
                               QStringLiteral("liftResume"));
            emit touchFiltered(id, sp.x(), sp.y(), 0,
                               QStringLiteral("liftResume"));
            emit touchReclassified(id, false, QStringLiteral("liftResume"));

#ifdef DSQT_TOUCH_PRIVATE_REINJECT
            // Re-deliver the new press with the OLD touch ID so Qt handlers see
            // one continuous touch (no lift, no new press).
            QEventPoint remapped = pt;
            QMutableEventPoint::setId(remapped, oldId);
            m_reinjecting = true;
            QWindowSystemInterface::handleTouchEvent<QWindowSystemInterface::SynchronousDelivery>(
                m_window, event->pointingDevice(),
                { toTouchPoint(remapped, m_window) }, event->modifiers());
            m_reinjecting = false;
            return true;  // suppress original new-press event
#else
            // Without private headers: pass the new press through unchanged.
            // Qt handlers see a lift + new press rather than a continuous drag.
            return false;
#endif
        }

        // 3. New touch — buffer and wait for the transient threshold.
        auto *data          = new TouchData;
        data->state         = PointState::Pending;
        data->smoothPos     = sp;
        data->buffered.append({ event->type(), event->pointingDevice(),
                                event->modifiers(), pt });

        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(m_transientThresholdMs);
        QObject::connect(timer, &QTimer::timeout, this,
                         [this, id] { confirmPress(id); });
        data->pendingTimer = timer;
        m_touches[id]      = data;
        timer->start();
        return true;  // suppress until confirmed
    }

    // ── MOVED ─────────────────────────────────────────────────────────────────
    case QEventPoint::Updated: {
        TouchData *data = m_touches.value(id, nullptr);
        if (!data) return false;  // unknown ID — pass through

        switch (data->state) {
        case PointState::Pending:
            data->buffered.append({ event->type(), event->pointingDevice(),
                                    event->modifiers(), pt });
            return true;  // keep buffering

        case PointState::Active: {
            const QPointF smooth = applySmoothing(data, sp.x(), sp.y());
            emit touchAccepted(id, sp.x(), sp.y(),
                               smooth.x(), smooth.y(), 1);
            return false;  // pass move through
        }

        case PointState::Lifting:
            // Finger moved — cancel the lift timer and resume dragging.
            if (data->liftTimer) {
                data->liftTimer->stop();
                delete data->liftTimer;
                data->liftTimer = nullptr;
            }
            data->state = PointState::Active;
            {
                const QPointF smooth = applySmoothing(data, sp.x(), sp.y());
                emit touchAccepted(id, sp.x(), sp.y(),
                                   smooth.x(), smooth.y(), 1);
            }
            return false;

        case PointState::Filtered:
            return true;  // suppress moves for filtered touches

        default:
            return false;
        }
    }

    // ── RELEASED ──────────────────────────────────────────────────────────────
    case QEventPoint::Released: {
        TouchData *data = m_touches.value(id, nullptr);
        if (!data) return false;

        switch (data->state) {
        case PointState::Pending: {
            // Released before the threshold — transient touch.
            if (data->pendingTimer) {
                data->pendingTimer->stop();
                delete data->pendingTimer;
                data->pendingTimer = nullptr;
            }
            for (const StoredEvent &ev : data->buffered) {
                const QPointF bsp = ev.point.globalPosition() - m_window->position();
                emit touchFiltered(id, bsp.x(), bsp.y(),
                                   stateInt(ev.point.state()),
                                   QStringLiteral("transient"));
            }
            emit touchReclassified(id, true, QStringLiteral("transient"));
            cleanupTouch(id);
            return true;  // suppress release
        }

        case PointState::Active: {
            // Open the lift-resume window; pass the release through.
            data->state   = PointState::Lifting;
            data->liftPos = sp;

            auto *liftTimer = new QTimer(this);
            liftTimer->setSingleShot(true);
            liftTimer->setInterval(m_liftResumeThresholdMs);
            QObject::connect(liftTimer, &QTimer::timeout, this,
                             [this, id] { confirmRelease(id); });
            data->liftTimer = liftTimer;
            liftTimer->start();
            return false;  // pass release through — see header note
        }

        case PointState::Filtered:
            cleanupTouch(id);
            return true;  // suppress release of a filtered touch

        default:
            cleanupTouch(id);
            return false;
        }
    }

    // ── STATIONARY ────────────────────────────────────────────────────────────
    case QEventPoint::Stationary: {
        const TouchData *data = m_touches.value(id, nullptr);
        if (!data) return false;
        // Suppress stationary reports for pending/filtered touches so they
        // stay invisible to the scene.
        return (data->state == PointState::Pending ||
                data->state == PointState::Filtered);
    }

    default:
        return false;
    }
}

// ── Deferred actions ──────────────────────────────────────────────────────────

void TouchFilter::confirmPress(int id)
{
    TouchData *data = m_touches.value(id, nullptr);
    if (!data || data->state != PointState::Pending) return;

    delete data->pendingTimer;
    data->pendingTimer = nullptr;
    data->state        = PointState::Active;

    emit touchReclassified(id, false, QString());

    // Flush and re-inject each buffered event in order.
    for (const StoredEvent &ev : data->buffered) {
        const QPointF sp = ev.point.globalPosition() - m_window->position();

        if (ev.point.state() == QEventPoint::Pressed) {
            emit touchAccepted(id, sp.x(), sp.y(),
                               data->smoothPos.x(), data->smoothPos.y(), 0);
        } else {
            const QPointF smooth = applySmoothing(data, sp.x(), sp.y());
            emit touchAccepted(id, sp.x(), sp.y(), smooth.x(), smooth.y(), 1);
        }

        reInjectEvent(ev);
    }
    data->buffered.clear();
}

void TouchFilter::confirmRelease(int id)
{
    TouchData *data = m_touches.value(id, nullptr);
    if (!data || data->state != PointState::Lifting) return;

    delete data->liftTimer;
    data->liftTimer = nullptr;

    emit touchAccepted(id,
                       data->liftPos.x(), data->liftPos.y(),
                       data->smoothPos.x(), data->smoothPos.y(), 2);
    cleanupTouch(id);
}

// ── Re-injection ──────────────────────────────────────────────────────────────

void TouchFilter::reInjectEvent(const StoredEvent &ev)
{
#ifdef DSQT_TOUCH_PRIVATE_REINJECT
    // Use QWindowSystemInterface so the event travels through the full Qt input
    // pipeline (QGuiApplicationPrivate::processTouchEvent → QPointingDevice state
    // update → QQuickDeliveryAgent).  This makes TapHandler, DragHandler, and
    // PinchHandler work correctly with re-injected events.
    m_reinjecting = true;
    QWindowSystemInterface::handleTouchEvent<QWindowSystemInterface::SynchronousDelivery>(
        m_window, ev.device, { toTouchPoint(ev.point, m_window) }, ev.modifiers);
    m_reinjecting = false;
#else
    // Fallback: sendEvent is synchronous but bypasses the full pipeline.
    // TapHandler / PointerHandler subclasses may not work reliably.
    QList<QEventPoint> pts = { ev.point };
    QTouchEvent reinjected(ev.type, ev.device, ev.modifiers, pts);
    m_reinjecting = true;
    QCoreApplication::sendEvent(m_window, &reinjected);
    m_reinjecting = false;
#endif
}

#ifdef DSQT_TOUCH_PRIVATE_REINJECT
QWindowSystemInterface::TouchPoint
TouchFilter::toTouchPoint(const QEventPoint &pt, QWindow *window)
{
    QWindowSystemInterface::TouchPoint tp;
    tp.id       = pt.id();
    tp.state    = pt.state();
    tp.pressure = pt.pressure();
    tp.rotation = pt.rotation();
    tp.velocity = pt.velocity();
    // area: 1×1 px contact patch centred on the global position
    tp.area     = QRectF(pt.globalPosition() - QPointF(0.5, 0.5), QSizeF(1.0, 1.0));
    // normalPosition: 0..1 relative to the screen geometry
    const QRectF screen = window->screen()->geometry();
    tp.normalPosition   = QPointF(pt.globalPosition().x() / screen.width(),
                                  pt.globalPosition().y() / screen.height());
    return tp;
}
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────

void TouchFilter::cleanupTouch(int id)
{
    TouchData *data = m_touches.take(id);
    if (!data) return;
    if (data->pendingTimer) { data->pendingTimer->stop(); delete data->pendingTimer; }
    if (data->liftTimer)    { data->liftTimer->stop();   delete data->liftTimer;    }
    delete data;
}

QPointF TouchFilter::applySmoothing(TouchData *data, qreal x, qreal y) const
{
    const qreal a  = m_smoothingFactor;
    data->smoothPos = QPointF(a * x + (1.0 - a) * data->smoothPos.x(),
                              a * y + (1.0 - a) * data->smoothPos.y());
    return data->smoothPos;
}

int TouchFilter::stateInt(QEventPoint::State s)
{
    switch (s) {
    case QEventPoint::Pressed:  return 0;
    case QEventPoint::Updated:  return 1;
    case QEventPoint::Released: return 2;
    default:                    return 1;
    }
}

qreal TouchFilter::dist(QPointF a, QPointF b)
{
    const qreal dx = a.x() - b.x();
    const qreal dy = a.y() - b.y();
    return qSqrt(dx * dx + dy * dy);
}
