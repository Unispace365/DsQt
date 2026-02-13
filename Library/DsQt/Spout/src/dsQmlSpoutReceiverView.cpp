#include "dsQmlSpoutReceiverView.h"
#include "dsSpoutReceiver.h"
#include "dsSpoutTextureNode.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>

#include <d3d11.h>
#include <d3d11_1.h>

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
    m_currentNativeHandle = nullptr;
    m_rhiTexture.reset();

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
        m_currentNativeHandle = nullptr;
        m_rhiTexture.reset();
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
    m_rhiTexture.reset();
    m_currentNativeHandle = nullptr;
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
    if (api == QSGRendererInterface::OpenGL || api == QSGRendererInterface::OpenGLRhi) {
        node->setTextureCoordinatesTransform(DsSpoutTextureNode::MirrorVertically);
    } else {
        node->setTextureCoordinatesTransform(DsSpoutTextureNode::NoTransform);
    }

    // Check if we need to (re)create the RHI texture from the shared handle
    if (sharedHandle != m_currentNativeHandle || !m_rhiTexture) {
        m_currentNativeHandle = sharedHandle;

        // Get Qt's RHI instance
        QRhi* rhi = static_cast<QRhi*>(
            rif->getResource(window(), QSGRendererInterface::RhiResource));

        if (!rhi) {
            qWarning() << "DsQmlSpoutReceiverView: Could not get QRhi instance";
            return node;
        }

        // Open the shared DX11 texture on Qt's RHI device
        ID3D11Device* device = nullptr;

        if (api == QSGRendererInterface::Direct3D11) {
            auto nativeHandles = static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
            if (nativeHandles) {
                device = static_cast<ID3D11Device*>(nativeHandles->dev);
            }
        }

        if (!device) {
            qWarning() << "DsQmlSpoutReceiverView: Could not get D3D11 device from RHI";
            return node;
        }

        // Open the Spout shared texture handle
        ID3D11Texture2D* sharedTexture = nullptr;
        HRESULT hr = device->OpenSharedResource(
            sharedHandle,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&sharedTexture));

        if (FAILED(hr) || !sharedTexture) {
            qWarning() << "DsQmlSpoutReceiverView: Failed to open shared resource, hr ="
                       << Qt::hex << hr;
            return node;
        }

        // Query the actual texture format from the shared resource
        D3D11_TEXTURE2D_DESC desc;
        sharedTexture->GetDesc(&desc);

        QRhiTexture::Format rhiFormat;
        switch (desc.Format) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            rhiFormat = QRhiTexture::BGRA8;
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            rhiFormat = QRhiTexture::RGBA8;
            break;
        default:
            qWarning() << "DsQmlSpoutReceiverView: Unsupported DXGI format" << desc.Format;
            sharedTexture->Release();
            return node;
        }

        // Create a QRhiTexture wrapping the shared DX11 texture
        QRhiTexture* rhiTex = rhi->newTexture(
            rhiFormat,
            senderSize,
            1,
            {});

        if (!rhiTex->createFrom({quint64(sharedTexture), 0})) {
            qWarning() << "DsQmlSpoutReceiverView: Failed to create QRhiTexture from shared texture";
            sharedTexture->Release();
            delete rhiTex;
            return node;
        }

        // The QRhiTexture now holds a reference; release our local ref
        sharedTexture->Release();

        m_rhiTexture = QSharedPointer<QRhiTexture>(rhiTex);
    }

    if (m_rhiTexture) {
        node->setTexture(m_rhiTexture, senderSize);
        node->setRect(boundingRect());
    }

    return node;
}
