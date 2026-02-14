#include "dsQmlSpoutReceiverView.h"
#include "dsSpoutReceiver.h"
#include "dsSpoutTextureNode.h"
#include "dsSpoutTextureImporter.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>

DsQmlSpoutReceiverView::DsQmlSpoutReceiverView(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);

    // Release GPU resources before QRhi is destroyed
    auto connectInvalidated = [this](QQuickWindow* win) {
        if (win) {
            connect(win, &QQuickWindow::sceneGraphInvalidated,
                    this, &DsQmlSpoutReceiverView::releaseResources,
                    Qt::DirectConnection);
        }
    };
    connectInvalidated(window());
    connect(this, &QQuickItem::windowChanged, this, connectInvalidated);
}

DsQmlSpoutReceiverView::~DsQmlSpoutReceiverView()
{
    disconnectReceiver();
}

void DsQmlSpoutReceiverView::setReceiver(DsSpoutReceiver* receiver)
{
    if (m_receiver == receiver)
        return;

    disconnectReceiver();
    m_receiver = receiver;
    connectReceiver();

    m_resetNode = true;
    if (m_importer)
        m_importer->releaseResources();

    emit receiverChanged();
    update();
}

void DsQmlSpoutReceiverView::connectReceiver()
{
    if (!m_receiver)
        return;

    connect(m_receiver, &DsSpoutReceiver::frameReceived,
            this, &DsQmlSpoutReceiverView::onFrameReceived, Qt::QueuedConnection);

    connect(m_receiver, &DsSpoutReceiver::connectedChanged,
            this, [this]() {
        m_resetNode = true;
        if (m_importer)
            m_importer->releaseResources();
        update();
    });

    // Continuous rendering — request update after each frame is presented
    if (window()) {
        connect(window(), &QQuickWindow::frameSwapped,
                this, &QQuickItem::update, Qt::QueuedConnection);
    }
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow* win) {
        if (win) {
            connect(win, &QQuickWindow::frameSwapped,
                    this, &QQuickItem::update, Qt::QueuedConnection);
        }
    });
}

void DsQmlSpoutReceiverView::disconnectReceiver()
{
    if (!m_receiver)
        return;


    disconnect(m_receiver, nullptr, this, nullptr);
    m_receiver = nullptr;
}

void DsQmlSpoutReceiverView::onFrameReceived()
{
    update();
}

void DsQmlSpoutReceiverView::releaseResources()
{
    if (m_importer)
        m_importer->releaseResources();
}

QSGNode* DsQmlSpoutReceiverView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    Q_UNUSED(data);

    if (!m_receiver || !m_receiver->isConnected() || !window()) {
        delete oldNode;
        return nullptr;
    }

    HANDLE sharedHandle = m_receiver->sharedTextureHandle();
    if (!sharedHandle) {
        delete oldNode;
        return nullptr;
    }

    QSize senderSize = m_receiver->senderSize();
    if (senderSize.isEmpty()) {
        delete oldNode;
        return nullptr;
    }

    // Get or create the scene graph node
    DsSpoutTextureNode* node = static_cast<DsSpoutTextureNode*>(oldNode);
    if (!node || m_resetNode) {
        delete oldNode;
        node = new DsSpoutTextureNode(window());
        m_resetNode = false;
    }

    // Determine texture coordinate transform based on RHI backend
    QSGRendererInterface* rif = window()->rendererInterface();
    QSGRendererInterface::GraphicsApi api = rif ? rif->graphicsApi() : QSGRendererInterface::Unknown;
    if (api == QSGRendererInterface::OpenGL) {
        node->setTextureCoordinatesTransform(DsSpoutTextureNode::MirrorVertically);
    } else {
        node->setTextureCoordinatesTransform(DsSpoutTextureNode::NoTransform);
    }

    // Lazily create the importer for the detected backend
    if (!m_importer) {
        m_importer = DsSpoutTextureImporter::create(api);
        if (!m_importer) {
            qWarning() << "DsQmlSpoutReceiverView: No importer for graphics API" << api;
            delete node;
            return nullptr;
        }
    }

    // Get RHI instance
    QRhi* rhi = static_cast<QRhi*>(
        rif->getResource(window(), QSGRendererInterface::RhiResource));
    if (!rhi) {
        qWarning() << "DsQmlSpoutReceiverView: Could not get QRhi instance";
        delete node;
        return nullptr;
    }

    // Import the texture via the backend-specific importer
    DXGI_FORMAT senderFormat = m_receiver->senderFormat();

    // Only fetch the CPU fallback QImage for OpenGL (other backends don't need it)
    QImage cpuFallback;
    if (api == QSGRendererInterface::OpenGL)
        cpuFallback = m_receiver->lastFrame();

    QSharedPointer<QRhiTexture> rhiTexture = m_importer->import(
        rhi, sharedHandle, senderSize, senderFormat, cpuFallback);

    if (!rhiTexture) {
        delete node;
        return nullptr;
    }

    node->setTexture(rhiTexture, senderSize);
    node->setRect(boundingRect());
    return node;
}
