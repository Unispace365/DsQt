#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QQmlEngine>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class WindowHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON  // Optional: makes it a singleton so you don't need to instantiate it in QML

public:
    explicit WindowHelper(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void forceToFront(QQuickWindow *window)
    {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        DWORD appThread = GetCurrentThreadId();

        if (fgThread != appThread) {
            AttachThreadInput(fgThread, appThread, TRUE);
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
            AttachThreadInput(fgThread, appThread, FALSE);
        } else {
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
        }
#else
        window->raise();
        window->requestActivate();
#endif
    }
};