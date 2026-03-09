#ifndef DSQMLFPSMONITOR_H
#define DSQMLFPSMONITOR_H

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>
#include <QElapsedTimer>

#include <deque>


namespace dsqt::ui {

/**
 * @brief True frame rate monitor.
 */
class DsQmlFpsMonitor : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsFpsMonitor)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool force READ force WRITE setForce NOTIFY forceChanged)
    Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)

  public:
    /**
     * @brief Constructs a DsQmlFpsMonitor instance.
     *
     * @param parent The parent QQuickItem (optional).
     */
    explicit DsQmlFpsMonitor(QQuickItem* parent = nullptr);

    /**
     * @brief Returns the refresh interval in milliseconds.
     *
     * @return The current interval.
     */
    int interval() const { return m_updateTimer.interval(); }

    /**
     * @brief Sets the refresh interval in milliseconds.
     *
     * @param interval The new interval value.
     */
    void setInterval(int interval) {
        if (m_updateTimer.interval() == interval) return;
        m_updateTimer.setInterval(interval);
        emit intervalChanged();
    }

    /**
     * @brief Returns whether forced updates are enabled.
     *
     * @return True if forced updates are enabled, false otherwise.
     */
    bool force() const { return m_force; }

    /**
     * @brief Sets whether forced updates are enabled.
     *
     * @param force True to enable forced updates, false to disable.
     */
    void setForce(bool force) {
        if (m_force == force) return;
        m_force = force;
        emit forceChanged();
    }

    /**
     * @brief Returns the (rolling) average frames per second.
     *
     * @return The current FPS value.
     */
    double fps() const { return m_fps; }

  signals:
    void fpsChanged();
    void intervalChanged();
    void forceChanged();

  private slots:
    /**
     * @brief Handles the frame swapped signal from the window.
     */
    void handleFrameSwapped();

    /**
     * @brief Updates the FPS calculation.
     */
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
