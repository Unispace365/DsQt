#include "dsQmlTouchEngineOutputView.h"
#include "dsTouchEngineTextureNode.h"
#include "dsQmlTouchEngineManager.h"
#include "dsQmlTouchEngineInstance.h"
#include <QQuickWindow>

DsQmlTouchEngineOutputView::DsQmlTouchEngineOutputView(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);


}

DsQmlTouchEngineOutputView::~DsQmlTouchEngineOutputView()
{
    disconnectInstance();
}

void DsQmlTouchEngineOutputView::setInstanceId(const QString& id)
{
    if (m_instanceId != id) {
        disconnectInstance();
        m_instanceId = id;
        connectInstance();
        emit instanceIdChanged();
        //update();
    }
}

void DsQmlTouchEngineOutputView::setOutputLink(const QString& link)
{
    if (m_outputLink != link) {
        m_outputLink = link;
        emit outputLinkChanged();
        //update();
    }
}

void DsQmlTouchEngineOutputView::setAutoUpdate(bool enable)
{
    if (m_autoUpdate != enable) {
        m_autoUpdate = enable;
        emit autoUpdateChanged();
    }
}

void DsQmlTouchEngineOutputView::requestFrame()
{
    if (m_instance) {
        //m_instance->startFrame();
    }
}

QSGNode* DsQmlTouchEngineOutputView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{


    auto callUpdate = [this]() {
        update();
    };

    //update();
    auto* node = static_cast<DsTouchEngineTextureNode*>(oldNode);
    //return node;

    // Create or update node
    if (!node) {
        node = new DsTouchEngineTextureNode(window(),m_instanceId,m_outputLink);
    }

    auto rhi = window()->rhi();
    if (!m_instance || !m_instance->isLoaded() || !rhi) {
        delete node;
        QMetaObject::invokeMethod(this,callUpdate,Qt::QueuedConnection);
        return nullptr;
    }

    // OpenGL needs vertical flip due to coordinate system differences
    if (rhi->backend() == QRhi::OpenGLES2) {
        node->setTextureCoordinatesTransform(DsTouchEngineTextureNode::MirrorVertically);
    } else {
        node->setTextureCoordinatesTransform(DsTouchEngineTextureNode::NoTransform);
    }

    //auto rhiTexture = m_instance->getRhiTexture(m_outputLink);
    m_rhiTexture = m_instance->getRhiTexture(m_outputLink);
    QSGTexture* tex = node->texture();
    if(tex && m_rhiTexture && tex->rhiTexture()->nativeTexture().object == m_rhiTexture->nativeTexture().object){
        //    //no need to update
        //node->markDirty(QSGNode::DirtyMaterial);
        return node;
    }

    // Get output texture from instance
    //SharedD3DTexture* textureHandle = m_instance->getSharedD3DTexture(m_outputLink);
    //SharedD3DTexture* textureHandle = m_instance->getOutputTexture(rhi,m_outputLink);
    //m_rhiTexture = rhiTexture;
    //if(rhiTexture){
    //    m_rhiTexture.reset(rhi->newTexture(rhiTexture->format(),rhiTexture->pixelSize(),1,
    //                                      {QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource}));
    //    QRhiTexture::NativeTexture src;
    //    src.layout = 0;
    //    src.object = rhiTexture->nativeTexture().object;
    //    m_rhiTexture->createFrom(src);
    //}



    if (!m_rhiTexture) {
        // No texture available
        delete node;
        QMetaObject::invokeMethod(this,callUpdate,Qt::QueuedConnection);
        return nullptr;
    }



    // Update texture
    QSize textureSize = m_rhiTexture->pixelSize(); // Should get actual texture size from TouchEngine
    node->setTexture(m_rhiTexture, textureSize);
    node->setRect(boundingRect());


    return node;
}

void DsQmlTouchEngineOutputView::releaseResources()
{
    disconnectInstance();
}

void DsQmlTouchEngineOutputView::handleFrameFinished()
{
    if (m_autoUpdate) {
        //qDebug() << "Frame finished, updating output view";
        update();
    }
}

void DsQmlTouchEngineOutputView::handleTextureUpdated(const QString& linkName)
{
    if (linkName == m_outputLink && m_autoUpdate) {
        update();
    }
}

void DsQmlTouchEngineOutputView::connectInstance()
{
    if (!m_instanceId.isEmpty()) {
        m_instance = DsQmlTouchEngineManager::inst()->getInstance(m_instanceId);

        if (m_instance) {
            m_updateTimer.setInterval(1000/120);
            connect(window(),&QQuickWindow::frameSwapped,this,&DsQmlTouchEngineOutputView::handleFrameFinished,Qt::QueuedConnection);
            //connect(&m_updateTimer,&QTimer::timeout,this,&DsQmlTouchEngineOutputView::handleFrameFinished,Qt::DirectConnection);
            m_updateTimer.start();
            //connect(m_instance, &DsQmlTouchEngineInstance::frameFinished,
            //        this, &DsQmlTouchEngineOutputView::handleFrameFinished,Qt::QueuedConnection);
            //connect(m_instance, &DsQmlTouchEngineInstance::textureUpdated,
            //        this, &DsQmlTouchEngineOutputView::handleTextureUpdated,Qt::DirectConnection);
            connect(m_instance, &DsQmlTouchEngineInstance::destroyed,
                    this, &DsQmlTouchEngineOutputView::disconnectInstance,Qt::QueuedConnection);
        }
    }
}

void DsQmlTouchEngineOutputView::disconnectInstance()
{
    if (m_instance) {
        disconnect(m_instance, nullptr, this, nullptr);
        m_instance = nullptr;
    }
}
