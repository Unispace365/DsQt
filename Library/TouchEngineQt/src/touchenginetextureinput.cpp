#include "touchenginetextureinput.h"
#include "touchengineinstance.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGTexture>

#include <TouchEngine/TEInstance.h>
#include <TouchEngine/TouchObject.h>
#include <TouchEngine/TETexture.h>
#include <TouchEngine/TEOpenGL.h>   // for the GL path
#include <TouchEngine/TED3D11.h>    // for the D3D path
#include <TouchEngine/TED3D.h>
#include <TouchEngine/TED3D12.h>
#include <TouchEngine/TEVulkan.h>

#include <TouchEngine/TouchEngine.h>    // for TEInstanceLinkSetTextureValue, TETexture*, etc.


TouchEngineTextureInput::TouchEngineTextureInput(QObject *parent)
    : TouchEngineInputBase(parent)
{
}

void TouchEngineTextureInput::setSourceItem(QQuickItem *item)
{
    if (m_sourceItem == item)
        return;

    // disconnect old window
    detachFromWindow();

    m_sourceItem = item;
    emit sourceItemChanged();

    if (m_sourceItem) {
        // if the item already has a window, hook now
        if (auto *w = m_sourceItem->window())
            attachToWindow(w);

        // in case the item is reparented or its window changes later
        connect(m_sourceItem, &QQuickItem::windowChanged,
                this, &TouchEngineTextureInput::handleWindowChanged);
    }
}

void TouchEngineTextureInput::handleWindowChanged()
{
    if (!m_sourceItem)
        return;

    detachFromWindow();
    if (auto *w = m_sourceItem->window())
        attachToWindow(w);
}

void TouchEngineTextureInput::attachToWindow(QQuickWindow *w)
{
    if (!w)
        return;

    m_window = w;

    // we want the *render-thread* signal, so use DirectConnection
    connect(w, &QQuickWindow::afterRendering,
            this, &TouchEngineTextureInput::handleAfterRendering,
            Qt::DirectConnection);
}

void TouchEngineTextureInput::detachFromWindow()
{
    if (m_window) {
        disconnect(m_window, nullptr, this, nullptr);
        m_window = nullptr;
    }
}

void TouchEngineTextureInput::handleAfterRendering()
{
    // runs every frame
    grabLayerTexture();
    if (autoUpdate() && getInstance()) {
       updateValue();
    }
}

GLuint TouchEngineTextureInput::grabLayerTexture()
{
    if (!m_sourceItem)
        return 0;
    if (!m_window)
        return 0;

    // If the user asked us to, make sure the item actually provides a texture
    if (!m_sourceItem->isTextureProvider())
        return 0;

    //m_sourceItem->setVisible(true);


    QSGTextureProvider *tp = m_sourceItem->textureProvider();

    if (!tp)
        return 0;

    QSGTexture *sgTex = tp->texture();
    if (!sgTex)
        return 0;

    auto id = sgTex->nativeInterface<QNativeInterface::QSGOpenGLTexture>()->nativeTexture();

    auto rhiTex = sgTex->rhiTexture();
    if (!rhiTex) {
        m_lastRhiTexture = nullptr;
        m_lastSize = sgTex->textureSize();
        return id;
    }

    m_lastRhiTexture = rhiTex;
    GLuint lastRenderId = static_cast<GLuint>(rhiTex->nativeTexture().object);
    //qDebug() << "Grabbed texture ID from item:" << lastRenderId;
    m_lastSize = rhiTex->pixelSize();

    // mark as dirty so TouchEngineInputBase will call applyValue()
    //if (autoUpdate() && getInstance()) {
    //    updateValue();
    //}
    m_lastId = id;
    return id;
}

void TouchEngineTextureInput::applyValue(TEInstance *teInstance)
{
    if (!teInstance)
        return;



    // we need an instance object so we can ask which graphics API we are using
    TouchEngineInstance *inst = getInstance();
    if (!inst)
        return;


    // we also need to know which link to set
    const QString id = linkName();
    if (id.isEmpty())
        return;

    if(!inst->hasInputLink(id)){
        return;
    }

    // this should have been written by your afterRendering() grab:
    //  - m_glTextureId for GL
    //  - m_size for width/height
    //if (!m_lastRhiTexture)
    //    return;

    const auto api = inst->getAPI();

    //TouchObject<TETexture> teTex;   // RAII wrapper from the TE headers
    TEResult r = TEResultSuccess;
    bool reuse = false;
    TextureKeeper* tk =nullptr;
    //TouchObject<TEOpenGLTexture> texture;
    switch (api)
    {
    case TouchEngineInstance::TEGraphicsAPI_OpenGL:
    {

        // we got this from QSGTexture::textureId()
        //if (!m_lastRhiTexture)
        //    return;


        //get the native id from the rhitexture
        //GLuint glTexId =     grabLayerTexture();
        GLuint glTexId =     m_lastId;
        if (glTexId == 0)
            return;

        //if(1 || !m_sharedTexture){
            TEOpenGLTexture* out = TEOpenGLTextureCreate(glTexId,
                GL_TEXTURE_2D,
                GL_RGBA8,
                m_lastSize.width(),
                m_lastSize.height(),
                TETextureOriginTopLeft,
                kTETextureComponentMapIdentity,
                TouchEngineTextureInput::textureReleaseCallback,
                tk);


                m_teTexture.take(out);

        //}
            //}
            //TERetain(out);
            //tk->rawTexture = out;
            //tk->texId = glTexId;
            break;

    }

    case TouchEngineInstance::TEGraphicsAPI_D3D11:
    {
        // You’ll need to store an ID3D11Texture2D* from your render hook
        // (i.e. when you manage to grab a shareable D3D11 texture from Qt).
        // Then this branch becomes:
        //
        // r = TED3D11TextureCreate(
        //        m_d3d11Texture.Get(),       // or raw pointer
        //        TETextureOriginTopLeft,
        //        &kTETextureComponentMapIdentity,
        //        nullptr, nullptr,
        //        teTex.take());
        //
        // For now, bail out so we don’t call SetTextureValue with nothing.
        return;
    }

    case TouchEngineInstance::TEGraphicsAPI_D3D12:
    {
        // Same idea as D3D11 but with TED3D12TextureCreate(...)
        return;
    }

    case TouchEngineInstance::TEGraphicsAPI_Vulkan:
    {
        // For Vulkan you need the VkImage + semaphore info that Qt gives you,
        // then use the functions in TEVulkan.h to wrap it.
        return;
    }

    case TouchEngineInstance::TEGraphicsAPI_Metal:
    {
        // On Windows you probably won’t hit this, but if you do:
        // TEMetalTextureCreate(mtlTex, TETextureOriginTopLeft, ...);
        return;
    }

    default:
        return;
    }

    // finally push it into the instance
    std::string utf8Id = std::string(id.toUtf8().constData());
    // context can be nullptr because your TouchEngineInstance already
    // called TEInstanceAssociateGraphicsContext(...)
    TouchObject<TESemaphore> semaphore;
    uint64_t waitValue = 0;

    TEOpenGLContext* ctx = static_cast<TEOpenGLContext*>(inst->graphicsContext());
    if(!ctx) return;

    //if(!reuse){

    r = TEInstanceLinkSetTextureValue(
         teInstance,
         utf8Id.c_str(),
         m_teTexture,
         ctx);
    //}


    //texture.reset(); // release our reference, TE now has it
    //if(m_funcs == nullptr){
    //    m_funcs = QOpenGLContext::currentContext()->functions();
    //}

    // {
    //     QMutexLocker locker(&m_mutex);
    //     for(auto& keeper : m_textureKeepersDeletes){
    //         // delete the GL texture in the render thread
    //         if(m_funcs){
    //             m_funcs->glDeleteTextures(1, &keeper->texId);
    //         }
    //         delete keeper;
    //     }
    // }

    if (r != TEResultSuccess) {
        optional: qWarning() << "failed to set texture on link" << id << r;
    }
}
QOpenGLFunctions* TouchEngineTextureInput::m_funcs = nullptr;

void
TouchEngineTextureInput::textureReleaseCallback(GLuint texture, TEObjectEvent event, void *info)
{

    // // TODO: might come from another thread
    // // let's try doing nothing
    TextureKeeper* tk = static_cast<TextureKeeper*>(info);
    // //if(texId == 1) return;
    // if (event == TEObjectEventRelease)
    // {
    //     QMutexLocker locker(&tk->owner->m_mutex);
    //     tk->owner->m_textureKeepersDeletes.append(tk);

    // }

}
