#ifndef DSQMLFPSMONITOR_H
#define DSQMLFPSMONITOR_H

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>

#include <deque>

namespace dsqt::ui {

// True frame rate monitor.
class DsQmlFpsMonitor : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsFpsMonitor)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool force READ force WRITE setForce NOTIFY forceChanged)
    Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)

  public:
    explicit DsQmlFpsMonitor(QQuickItem* parent = nullptr);

    // Returns the refresh interval in milliseconds.
    int interval() const { return m_updateTimer.interval(); }
    // Sets the refresh interval in milliseconds.
    void setInterval(int interval) {
        if (m_updateTimer.interval() == interval) return;
        m_updateTimer.setInterval(interval);
        emit intervalChanged();
    }

    // Returns whether forced updates are enabled.
    bool force() const { return m_force; }
    // Sets whether forced updates are enabled.
    void setForce(bool force) {
        if (m_force == force) return;
        m_force = force;
        emit forceChanged();
    }

    // Returns the (rolling) average frames per second.
    double fps() const { return m_fps; }

  signals:
    void fpsChanged();
    void intervalChanged();
    void forceChanged();

  private slots:
    void handleFrameSwapped();
    void updateFps();

  private:
    QQuickWindow*      m_window = nullptr;
    QTimer             m_updateTimer;
    QElapsedTimer      m_elapsedTimer;
    std::deque<qint64> m_frameTimes;
    double             m_fps   = 0.0;
    bool               m_force = false;
};

} // namespace dsqt::ui

#endif
