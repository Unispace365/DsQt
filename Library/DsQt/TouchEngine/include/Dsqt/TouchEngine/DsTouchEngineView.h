#pragma once

#include <QColor>
#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickRhiItem>

QT_FORWARD_DECLARE_CLASS(QQuickWindow)

namespace dsqt::touchengine {

class DsTouchEngineSession;

class DsTouchEngineView : public QQuickRhiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineView)

    Q_PROPERTY(dsqt::touchengine::DsTouchEngineSession *session READ session WRITE setSession NOTIFY sessionChanged FINAL)
    Q_PROPERTY(QString outputLink READ outputLink WRITE setOutputLink NOTIFY outputLinkChanged FINAL)
    Q_PROPERTY(QColor clearColor READ clearColor WRITE setClearColor NOTIFY clearColorChanged FINAL)

public:
    explicit DsTouchEngineView(QQuickItem *parent = nullptr);
    ~DsTouchEngineView() override;

    // Adds the Vulkan external-memory/semaphore extensions required by
    // TouchEngine. Call before showing a C++-created window. QML-created views
    // invoke this automatically when they attach to their window.
    static bool configureVulkanInterop(QQuickWindow *window);

    DsTouchEngineSession *session() const;
    void setSession(DsTouchEngineSession *session);

    QString outputLink() const;
    void setOutputLink(const QString &link);

    QColor clearColor() const;
    void setClearColor(const QColor &color);

signals:
    void sessionChanged();
    void outputLinkChanged();
    void clearColorChanged();

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private:
    QPointer<DsTouchEngineSession> m_session;
    QString m_outputLink = QStringLiteral("output");
    QColor m_clearColor = Qt::transparent;
};

} // namespace dsqt::touchengine
