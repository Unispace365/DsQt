#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>

#include <QWidget>
#include <QWindow>

namespace dsqt {

// Framework tool windows - Settings, Content Browser, the log viewers,
// TouchFilter Debug, and any an application adds - all behave the same way:
//
// - They are transient children of the main QML window: they stay above the
//   application, and only the application (unlike Qt::WindowStaysOnTopHint,
//   which puts a window above every program on the desktop), and they can be
//   moved and resized like any other window.
// - Their position and size are remembered between openings and between runs,
//   per application, under the key the window is registered with.
// - Their visibility is reported back, so a checkable menu item stays in step
//   even when the window is closed from its own title bar.
//
// There are two ways in, for the two kinds of window:
//
// - A QWidget window: derive a helper from DsQmlToolWindowHelper and
//   implement createWidget(). See DsQmlSettingsViewerHelper.
// - A QML Window: call DsToolWindows.track(window, "key") once, after creating
//   it. See DsAppBase.qml.
namespace toolwindow {

// The application's main QML window (the engine's first root object), or
// nullptr if `context` has no QQmlApplicationEngine or nothing is loaded yet.
QWindow *mainWindow(QObject *context);

// Makes `widget`'s native window a transient child of the main QML window.
void makeTransient(QWidget *widget, QObject *context);

// Restores the geometry saved under `key`; false if there was none, or it no
// longer fits on any screen.
bool restoreGeometry(QWidget *widget, const QString &key);
bool restoreGeometry(QWindow *window, const QString &key);

// Saves the geometry under `key`.
void saveGeometry(const QWidget *widget, const QString &key);
void saveGeometry(const QWindow *window, const QString &key);

} // namespace toolwindow

// Base for the QML handle of a QWidget tool window.
//
// Declare the derived helper in QML and drive it from a checkable menu item:
//
//   DsSettingsViewerHelper {
//       id: settingsViewer
//       onVisibleChanged: (isVisible) => { menu.settingsChecked = isVisible }
//   }
//   onSettingsTriggered: (isChecked) => { settingsViewer.setVisible(isChecked) }
//
// The widget is created on the first show(), which happens after the main QML
// window exists, so it can be made transient to it. From then on show() brings
// the same window back, raised and un-minimised.
class DsQmlToolWindowHelper : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)

public:
    ~DsQmlToolWindowHelper() override;

    // Creates the widget if needed, then shows, raises and activates it.
    Q_INVOKABLE void show();
    // Hides the widget. No-op if it was never created.
    Q_INVOKABLE void hide();
    // Convenience for checkable menu actions: shows when true, hides when false.
    Q_INVOKABLE void setVisible(bool visible);

    bool isVisible() const;

    // The widget, once created.
    QWidget *widget() const { return m_widget; }

signals:
    // Mirrors the widget's real on-screen state, including closing it from its
    // own title bar.
    void visibleChanged(bool visible);

protected:
    // `key` names the window for its remembered geometry; keep it stable.
    explicit DsQmlToolWindowHelper(const QString &key, QObject *parent = nullptr);

    // Creates the widget. Called once, on the first show(). The helper owns it.
    virtual QWidget *createWidget() = 0;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QString m_key;
    QPointer<QWidget> m_widget;
};

// The same behaviour for QML Windows, as a singleton:
//
//   const w = component.createObject(window)
//   DsToolWindows.track(w, "appLog")
class DsQmlToolWindows : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsToolWindows)
    QML_SINGLETON

public:
    explicit DsQmlToolWindows(QObject *parent = nullptr);

    // Makes `window` transient to the main window, restores the geometry saved
    // under `key`, and saves it whenever the window is hidden and when the
    // application quits.
    Q_INVOKABLE void track(QWindow *window, const QString &key);
};

} // namespace dsqt
