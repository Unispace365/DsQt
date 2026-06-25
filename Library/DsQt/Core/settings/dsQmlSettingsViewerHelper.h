#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>

namespace dsqt {

class SettingsViewerWidget;

// Thin QML-accessible wrapper that lazily creates and shows a SettingsViewerWidget.
//
// Declare it inside DsAppBase (or any Item) and call show() from menu signals:
//
//   DsSettingsViewerHelper { id: settingsViewer }
//   onSettingsEngineTriggered: settingsViewer.show()
//
// The widget is created on the first show() call, which happens after
// QQmlApplicationEngine::loadFromModule() so the graphics context is ready.
// Subsequent calls just bring the existing window to the front.
class DsQmlSettingsViewerHelper : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsSettingsViewerHelper)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)

public:
    explicit DsQmlSettingsViewerHelper(QObject* parent = nullptr);
    ~DsQmlSettingsViewerHelper() override;

    // Lazily creates the SettingsViewerWidget (if needed), then shows and raises it.
    Q_INVOKABLE void show();
    // Hides the widget. No-op if it was never created.
    Q_INVOKABLE void hide();
    // Convenience for checkable menu actions: shows when true, hides when false.
    Q_INVOKABLE void setVisible(bool visible);

    bool isVisible() const;

signals:
    // Mirrors the widget's real on-screen state, so a menu checkbox bound to
    // this property stays correct even if the window is closed via its own
    // close button rather than through setVisible()/hide().
    void visibleChanged(bool visible);

private:
    QPointer<SettingsViewerWidget> m_viewer;
};

} // namespace dsqt
