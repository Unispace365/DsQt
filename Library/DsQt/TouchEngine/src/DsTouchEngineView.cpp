#include <Dsqt/TouchEngine/DsTouchEngineView.h>

#include <Dsqt/TouchEngine/DsTouchEngineSession.h>

#include "private/DsTouchEngineSessionPrivate_p.h"
#include "private/DsTouchEngineViewRenderer_p.h"

#include <QQuickGraphicsConfiguration>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include <algorithm>

namespace dsqt::touchengine {

DsTouchEngineView::DsTouchEngineView(QQuickItem *parent)
    : QQuickRhiItem(parent)
{
    setSampleCount(1);
    setColorBufferFormat(TextureFormat::RGBA8);
    setAlphaBlending(true);
    connect(this, &QQuickItem::windowChanged, this, [](QQuickWindow *window) {
        configureVulkanInterop(window);
    });
    configureVulkanInterop(window());
}

DsTouchEngineView::~DsTouchEngineView()
{
    if (m_session) {
        auto &views = m_session->d->views;
        views.erase(std::remove(views.begin(), views.end(), this), views.end());
    }
}

bool DsTouchEngineView::configureVulkanInterop(QQuickWindow *window)
{
    if (!window)
        return false;
    if (QQuickWindow::graphicsApi() != QSGRendererInterface::Vulkan)
        return true;
    if (window->isSceneGraphInitialized())
        return false;

    QQuickGraphicsConfiguration configuration = window->graphicsConfiguration();
    QByteArrayList extensions = configuration.deviceExtensions();
    for (const QByteArray &required : {
             QByteArrayLiteral("VK_KHR_external_memory"),
             QByteArrayLiteral("VK_KHR_external_memory_win32"),
             QByteArrayLiteral("VK_KHR_external_semaphore"),
             QByteArrayLiteral("VK_KHR_external_semaphore_win32")}) {
        if (!extensions.contains(required))
            extensions.push_back(required);
    }
    configuration.setDeviceExtensions(extensions);
    window->setGraphicsConfiguration(configuration);
    return true;
}

DsTouchEngineSession *DsTouchEngineView::session() const { return m_session; }

void DsTouchEngineView::setSession(DsTouchEngineSession *session)
{
    if (m_session == session)
        return;

    if (m_session) {
        auto &views = m_session->d->views;
        views.erase(std::remove(views.begin(), views.end(), this), views.end());
    }
    m_session = session;
    if (m_session && !m_session->d->views.contains(this))
        m_session->d->views.push_back(this);

    emit sessionChanged();
    update();
}

QString DsTouchEngineView::outputLink() const { return m_outputLink; }

void DsTouchEngineView::setOutputLink(const QString &link)
{
    if (m_outputLink == link)
        return;
    m_outputLink = link;
    emit outputLinkChanged();
    update();
}

QColor DsTouchEngineView::clearColor() const { return m_clearColor; }

void DsTouchEngineView::setClearColor(const QColor &color)
{
    if (m_clearColor == color)
        return;
    m_clearColor = color;
    emit clearColorChanged();
    update();
}

QQuickRhiItemRenderer *DsTouchEngineView::createRenderer()
{
    return new detail::DsTouchEngineViewRenderer;
}

} // namespace dsqt::touchengine
