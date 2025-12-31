#include "touchengineoutputview.h"
#include "touchenginetexturenode.h"
#include "touchenginemanager.h"
#include "touchengineinstance.h"
#include <QQuickWindow>

TouchEngineOutputView::TouchEngineOutputView(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);


}

TouchEngineOutputView::~TouchEngineOutputView()
{
    disconnectInstance();
}

void TouchEngineOutputView::setInstanceId(const QString& id)
{
    if (m_instanceId != id) {
        disconnectInstance();
        m_instanceId = id;
        connectInstance();
        emit instanceIdChanged();
        //update();
    }
}

void TouchEngineOutputView::setOutputLink(const QString& link)
{
    if (m_outputLink != link) {
        m_outputLink = link;
        emit outputLinkChanged();
        //update();
    }
}

void TouchEngineOutputView::setAutoUpdate(bool enable)
{
    if (m_autoUpdate != enable) {
        m_autoUpdate = enable;
        emit autoUpdateChanged();
    }
}

void TouchEngineOutputView::requestFrame()
{
    if (m_instance) {
        //m_instance->startFrame();
    }
}

QSGNode* TouchEngineOutputView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{


    auto callUpdate = [this]() {
        update();
    };

    //update();
    auto* node = static_cast<TouchEngineTextureNode*>(oldNode);
    //return node;

    // Create or update node
    if (!node) {
        node = new TouchEngineTextureNode(window(),m_instanceId,m_outputLink);
    }

    auto rhi = window()->rhi();
    if (!m_instance || !m_instance->isLoaded() || !rhi) {
        delete node;
        QMetaObject::invokeMethod(this,callUpdate,Qt::QueuedConnection);
        return nullptr;
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

void TouchEngineOutputView::releaseResources()
{
    disconnectInstance();
}

void TouchEngineOutputView::handleFrameFinished()
{
    if (m_autoUpdate) {
        //qDebug() << "Frame finished, updating output view";
        update();
    }
}

void TouchEngineOutputView::handleTextureUpdated(const QString& linkName)
{
    if (linkName == m_outputLink && m_autoUpdate) {
        update();
    }
}

void TouchEngineOutputView::connectInstance()
{
    if (!m_instanceId.isEmpty()) {
        m_instance = TouchEngineManager::inst()->getInstance(m_instanceId);

        if (m_instance) {
            m_updateTimer.setInterval(1000/120);
            connect(window(),&QQuickWindow::frameSwapped,this,&TouchEngineOutputView::handleFrameFinished,Qt::QueuedConnection);
            //connect(&m_updateTimer,&QTimer::timeout,this,&TouchEngineOutputView::handleFrameFinished,Qt::DirectConnection);
            m_updateTimer.start();
            //connect(m_instance, &TouchEngineInstance2::frameFinished,
            //        this, &TouchEngineOutputView::handleFrameFinished,Qt::QueuedConnection);
            //connect(m_instance, &TouchEngineInstance2::textureUpdated,
            //        this, &TouchEngineOutputView::handleTextureUpdated,Qt::DirectConnection);
            connect(m_instance, &TouchEngineInstance::destroyed,
                    this, &TouchEngineOutputView::disconnectInstance,Qt::QueuedConnection);
        }
    }
}

void TouchEngineOutputView::disconnectInstance()
{
    if (m_instance) {
        disconnect(m_instance, nullptr, this, nullptr);
        m_instance = nullptr;
    }
}
