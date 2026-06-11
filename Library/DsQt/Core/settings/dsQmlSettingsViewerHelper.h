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

public:
    explicit DsQmlSettingsViewerHelper(QObject* parent = nullptr);
    ~DsQmlSettingsViewerHelper() override;

    // Lazily creates the SettingsViewerWidget (if needed), then shows and raises it.
    Q_INVOKABLE void show();

private:
    QPointer<SettingsViewerWidget> m_viewer;
};

} // namespace dsqt
