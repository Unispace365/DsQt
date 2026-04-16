#pragma once
#include <QEventPoint>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>

#ifdef DSQT_TOUCH_PRIVATE_REINJECT
#  include <qpa/qwindowsysteminterface_p.h>
#  include <private/qeventpoint_p.h>
#endif

class QTouchEvent;

/// A QObject event filter that intercepts QTouchEvents at the QQuickWindow
/// level and suppresses unwanted touches before Qt's pointer-dispatch system
/// delivers them to any handler in the scene.
///
/// Because it operates on raw QEvents, a filtered touch is invisible to
/// PinchHandler, TapHandler, DragHandler, MultiPointTouchArea, and every
/// other Qt Quick handler — not just to the code that listens to this object's
/// signals.
///
/// Usage
/// -----
///   1. Add TouchFilter.h / TouchFilter.cpp to your target.
///   2. Declare a TouchFilter in QML and set its window property:
///
///        TouchFilter {
///            id: filter
///            window: root.Window.window   // or bind onWindowChanged
///        }
///
///   3. Listen to touchAccepted / touchFiltered / touchReclassified for
///      visualisation or any logic that needs the filter's view of events.
///
/// Filtering stages (applied to every new press, in order)
/// --------------------------------------------------------
///   1. Proximity rejection  – immediate suppression if the press lands within
///      proximityFilterPx of any active touch.  Set to 0 to disable.
///   2. Transient suppression – the press is held for transientThresholdMs.
///      If the finger lifts before the timer fires the entire touch (press +
///      buffered moves) is suppressed.  If the timer fires the buffered events
///      are re-injected and delivered normally.
///   3. Jitter smoothing     – move events pass through unchanged; smoothed
///      positions are reported via touchAccepted signals only.
///   4. Lift-resume bridging – see note below.
///
/// Lift-resume note
/// ----------------
/// A release followed by a nearby press within liftResumeThresholdMs is
/// treated as a drag continuation in the touchAccepted / touchFiltered signal
/// stream.
///
/// Without DSQT_TOUCH_PRIVATE_REINJECT: both the release and the new press
/// events pass through to Qt's handlers unchanged (signal-level only).
///
/// With DSQT_TOUCH_PRIVATE_REINJECT: the new press is suppressed and
/// re-delivered with the old touch ID via QWindowSystemInterface, making the
/// lift-resume fully transparent to TapHandler, DragHandler, etc.
///
/// Re-injection note
/// -----------------
/// By default, confirmed touches are re-injected via QCoreApplication::sendEvent.
/// This bypasses Qt's input pipeline so TapHandler / PointerHandler subclasses
/// may not receive events reliably.  Build with DSQT_TOUCH_PRIVATE_REINJECT=ON
/// (requires Qt6::GuiPrivate) to use QWindowSystemInterface::handleTouchEvent
/// which feeds events through the full pipeline.
///
/// On Windows LWE displays, QEventPoint::scenePosition() == globalPosition()
/// (screen-absolute) rather than window-relative.  TapHandler checks scenePosition()
/// against item bounds; raw LWE events fail this check and are rejected.
/// With DSQT_TOUCH_PRIVATE_REINJECT, even when filterEnabled is false, events are
/// re-normalised through QWindowSystemInterface so TapHandler works correctly.
/// Without the flag the original event is passed through and TapHandler may not work.
class TouchFilter : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow
                   NOTIFY windowChanged FINAL)
    Q_PROPERTY(int transientThresholdMs READ transientThresholdMs
                   WRITE setTransientThresholdMs
                   NOTIFY transientThresholdMsChanged FINAL)
    Q_PROPERTY(qreal smoothingFactor READ smoothingFactor
                   WRITE setSmoothingFactor
                   NOTIFY smoothingFactorChanged FINAL)
    Q_PROPERTY(int liftResumeThresholdMs READ liftResumeThresholdMs
                   WRITE setLiftResumeThresholdMs
                   NOTIFY liftResumeThresholdMsChanged FINAL)
    Q_PROPERTY(qreal liftResumeDistancePx READ liftResumeDistancePx
                   WRITE setLiftResumeDistancePx
                   NOTIFY liftResumeDistancePxChanged FINAL)
    Q_PROPERTY(qreal proximityFilterPx READ proximityFilterPx
                   WRITE setProximityFilterPx
                   NOTIFY proximityFilterPxChanged FINAL)
    Q_PROPERTY(bool filterEnabled READ isFilterEnabled WRITE setFilterEnabled
                   NOTIFY filterEnabledChanged FINAL)
    Q_PROPERTY(bool privateReinject READ isPrivateReinject CONSTANT FINAL)

public:
    explicit TouchFilter(QObject *parent = nullptr);
    ~TouchFilter() override;

    QQuickWindow* window()          const { return m_window;              }
    int   transientThresholdMs()    const { return m_transientThresholdMs;  }
    qreal smoothingFactor()         const { return m_smoothingFactor;        }
    int   liftResumeThresholdMs()   const { return m_liftResumeThresholdMs; }
    qreal liftResumeDistancePx()    const { return m_liftResumeDistancePx;  }
    qreal proximityFilterPx()       const { return m_proximityFilterPx;     }
    bool  isFilterEnabled()         const { return m_filterEnabled;          }
    static bool isPrivateReinject() {
#ifdef DSQT_TOUCH_PRIVATE_REINJECT
        return true;
#else
        return false;
#endif
    }

    void setWindow              (QQuickWindow* w);
    void setTransientThresholdMs(int   ms);
    void setSmoothingFactor     (qreal factor);
    void setLiftResumeThresholdMs(int  ms);
    void setLiftResumeDistancePx(qreal px);
    void setProximityFilterPx   (qreal px);
    void setFilterEnabled       (bool enabled);

    /// Discard all in-flight state (call e.g. when the visualiser is cleared).
    Q_INVOKABLE void reset();

    // QObject override — installed on the QQuickWindow.
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    /// Fired for every touch point seen by the event filter, before any
    /// filtering decision is made.  Use this to drive raw visualisation.
    /// Coordinates are in window/scene space.
    /// state: 0 = pressed, 1 = moved, 2 = released
    void touchRaw(int id, qreal x, qreal y, int state);

    /// A touch point passed all filters and its event has been (or will be)
    /// delivered to the scene.  smoothX/Y carry the EMA-smoothed position.
    /// Coordinates are in window/scene space.
    /// state: 0 = pressed, 1 = moved, 2 = released
    void touchAccepted(int id,
                       qreal rawX, qreal rawY,
                       qreal smoothX, qreal smoothY,
                       int state);

    /// A touch event was suppressed.
    /// reason: "transient" | "proximity" | "liftResume"
    /// Coordinates are in window/scene space.
    void touchFiltered(int id, qreal x, qreal y, int state, QString reason);

    /// Fired once a pending touch is conclusively classified so the UI can
    /// retroactively colour a trail.
    /// wasFiltered = true  → touch was rejected
    /// wasFiltered = false → touch was accepted (touchAccepted will also fire)
    void touchReclassified(int id, bool wasFiltered, QString reason);

    void windowChanged();
    void transientThresholdMsChanged();
    void smoothingFactorChanged();
    void liftResumeThresholdMsChanged();
    void liftResumeDistancePxChanged();
    void proximityFilterPxChanged();
    void filterEnabledChanged();

private:
    enum class PointState { Pending, Active, Lifting, Filtered };

    // One touch event's worth of data for a single point, stored while buffering.
    struct StoredEvent {
        QEvent::Type           type;
        const QPointingDevice *device;
        Qt::KeyboardModifiers  modifiers;
        QEventPoint            point;   // QEventPoint is copyable in Qt 6
    };

    struct TouchData {
        PointState         state { PointState::Pending };
        QPointF            smoothPos;
        QPointF            liftPos;
        QList<StoredEvent> buffered;       ///< press + moves held during Pending
        QTimer            *pendingTimer { nullptr };
        QTimer            *liftTimer   { nullptr };
    };

    QHash<int, TouchData *> m_touches;
    QQuickWindow           *m_window     { nullptr };
    bool                    m_reinjecting  { false };
    bool                    m_filterEnabled{ true  };

    int   m_transientThresholdMs  {  80  };
    qreal m_smoothingFactor       { 0.30 };
    int   m_liftResumeThresholdMs { 120  };
    qreal m_liftResumeDistancePx  { 25.0 };
    qreal m_proximityFilterPx     {  0.0 };

    // Returns true if the point should be suppressed, false to pass through.
    // Also emits signals and updates internal state.
    bool filterPoint(QTouchEvent *event, const QEventPoint &pt, const QPointF &windowPos);

    void confirmPress  (int id);  ///< Pending→Active: re-inject buffered events
    void confirmRelease(int id);  ///< Lifting timer expired: emit signal release
    void cleanupTouch  (int id);

    /// Re-inject a single buffered event so Qt handlers see the confirmed touch.
    void reInjectEvent(const StoredEvent &ev);

    QPointF applySmoothing(TouchData *data, qreal x, qreal y) const;

    static int   stateInt(QEventPoint::State s);
    static qreal dist(QPointF a, QPointF b);

#ifdef DSQT_TOUCH_PRIVATE_REINJECT
    /// Convert a QEventPoint to the structure QWindowSystemInterface expects.
    static QWindowSystemInterface::TouchPoint toTouchPoint(const QEventPoint &pt,
                                                           QWindow *window);
#endif
};
