#include "ds_qml_fps_monitor.h"

namespace dsqt::ui {

DsQmlFpsMonitor::DsQmlFpsMonitor(QQuickItem* parent)
    : QQuickItem(parent)
    , m_updateTimer(this) {
    m_elapsedTimer.start();

    // Connect to window's frameSwapped once the component is complete.
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow* win) {
        if (m_window == win) return;
        if (m_window) {
            disconnect(m_window, &QQuickWindow::frameSwapped, this, &DsQmlFpsMonitor::handleFrameSwapped);
        }
        m_window = win;
        if (m_window) {
            connect(m_window, &QQuickWindow::frameSwapped, this, &DsQmlFpsMonitor::handleFrameSwapped);
        }
    });

    connect(&m_updateTimer, &QTimer::timeout, this, &DsQmlFpsMonitor::updateFps);
    m_updateTimer.start(250);
}

void DsQmlFpsMonitor::handleFrameSwapped() {
    m_frameTimes.push_back(m_elapsedTimer.nsecsElapsed() / 1.0e6);
    if (m_force && m_window) m_window->update(); // Force update.
}

void DsQmlFpsMonitor::updateFps() {
    qint64 now = m_elapsedTimer.elapsed();

    // Remove timestamps older than 1 second (rolling window).
    while (!m_frameTimes.empty() && m_frameTimes.front() < now - 1000) {
        m_frameTimes.pop_front();
    }

    if (m_frameTimes.size() < 2) {
        if (m_fps != 0.0) {
            m_fps = 0.0;
            emit fpsChanged();
        }
        return;
    }

    double timeSpan = (m_frameTimes.back() - m_frameTimes.front()) / 1000.0;
    double newFps;
    if (timeSpan <= 0.0) {
        newFps = 1000.0;
    } else {
        newFps = (m_frameTimes.size() - 1) / timeSpan;
    }

    if (qAbs(newFps - m_fps) > 0.01) {
        m_fps = newFps;
        emit fpsChanged();
    }
}


} // namespace dsqt::ui
