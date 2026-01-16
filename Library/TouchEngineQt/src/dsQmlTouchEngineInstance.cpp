#include "dsQmlTouchEngineInstance.h"
//#include "DX11Renderer.h"
#include "D3D12Renderer.h"
#include "OpenGLRenderer.h"
#include <QtCore>
#include "VulkanRenderer.h"

DsQmlTouchEngineInstance::DsQmlTouchEngineInstance(QObject *parent,QQuickWindow* window)
    : QObject{parent}
{
    m_frameTimer.start();
    m_window = window;

}

void
DsQmlTouchEngineInstance::linkLayoutDidChange()
{
    QMutexLocker locker(&myMutex);
    myPendingLayoutChange = true;
}

void DsQmlTouchEngineInstance::didConfigure(TEResult result)
{
    if(result != TEResultCancelled) {
        QMutexLocker locker(&myMutex);
        myConfigureRenderer = true;
        myConfigureResult = result;
    }
}

void
DsQmlTouchEngineInstance::eventCallback(TEInstance * instance,
                              TEEvent event,
                              TEResult result,
                              int64_t start_time_value,
                              int32_t start_time_scale,
                              int64_t end_time_value,
                              int32_t end_time_scale,
                              void * info)
{
    DsQmlTouchEngineInstance *inst = static_cast<DsQmlTouchEngineInstance *>(info);

    switch (event)
    {
    case TEEventInstanceReady:
        inst->didConfigure(result);
        break;
    case TEEventInstanceDidLoad:
        inst->didLoad();
        break;
    case TEEventInstanceDidUnload:
        break;
    case TEEventFrameDidFinish:
        inst->endFrame(start_time_value, start_time_scale, result);
        break;
    case TEEventGeneral:
        break;
    default:
        break;
    }
}

void
DsQmlTouchEngineInstance::linkEventCallback(TEInstance * instance, TELinkEvent event, const char *identifier, void * info)
{
    DsQmlTouchEngineInstance* inst = static_cast<DsQmlTouchEngineInstance*>(info);
    switch (event)
    {
    case TELinkEventAdded:
        inst->linkLayoutDidChange();
        break;
    case TELinkEventValueChange:
        inst->linkValueChange(identifier);
        break;
    default:
        break;
    }
}

void
DsQmlTouchEngineInstance::linkValueChange(const char* identifier)
{
    TouchObject<TELinkInfo> link;
    TEResult result = TEInstanceLinkGetInfo(myInstance, identifier, link.take());
    if (result == TEResultSuccess && link->scope == TEScopeOutput)
    {
        QString linkId = QString::fromUtf8(identifier);

        switch (link->type)
        {
        case TELinkTypeTexture:
        {
            // Stash the state, we don't do any actual renderer work from this thread
            QMutexLocker locker(&myMutex);
            myPendingOutputTextures.push_back(linkId);
            break;
        }
        case TELinkTypeFloatBuffer:
        case TELinkTypeStringData:
        case TELinkTypeDouble:
        case TELinkTypeInt:
        case TELinkTypeBoolean:
        {
            // Emit signal for output components to read the value
            emit outputLinkValueChanged(linkId);
            break;
        }
        default:
            break;
        }
    }
}

void
DsQmlTouchEngineInstance::endFrame(int64_t time_value, int32_t time_scale, TEResult result)
{
    setInFrame(false);
}

void
DsQmlTouchEngineInstance::getState(bool& configured, bool& ready, bool& loaded, bool& linksChanged, bool& inFrame)
{
    QMutexLocker locker(&myMutex);
    configured = myConfigureRenderer;
    myConfigureRenderer = false;
    loaded = myDidLoad;
    ready = myIsReady;
    if (myDidLoad)
    {
        // For this example, we are only interested in links after load has completed
        linksChanged = myPendingLayoutChange;
        inFrame = myInFrame;
        myPendingLayoutChange = false;
    }
    else
    {
        linksChanged = false;
        inFrame = false;
    }
}

void
DsQmlTouchEngineInstance::setInFrame(bool inFrame)
{
    QMutexLocker locker(&myMutex);
    myInFrame = inFrame;
}

bool
DsQmlTouchEngineInstance::applyOutputTextureChange()
{
    // Only hold the lock briefly
    std::vector<QString> changes;
    {
        QMutexLocker locker(&myMutex);
        std::swap(myPendingOutputTextures, changes);
    }

    //if we are going to updateTheOutputImage from the updatePaintNode of the
    //view just return true here to indicate there are changes.
    if(m_useHeavyTextureGet){
        return !changes.empty();
    }

    for (const auto & identifier : changes)
    {
        size_t imageIndex = myOutputLinkTextureMap[identifier];
        std::string idStr = std::string(identifier.toUtf8().constData());
        myRenderer->updateOutputImage(myInstance, imageIndex, idStr);
    }

    return !changes.empty();
}

bool DsQmlTouchEngineInstance::hasInputLink(QString identifier)
{
    return lastInputLinks.contains(identifier);
}

void DsQmlTouchEngineInstance::applyLayoutChange()
{
    myRenderer->beginImageLayout();

    myRenderer->clearInputImages();
    myRenderer->clearOutputImages();
    myOutputLinkTextureMap.clear();
    lastInputLinks.clear();
    for (auto scope : { TEScopeInput, TEScopeOutput })
    {
        TouchObject<TEStringArray> groups;
        TEResult result = TEInstanceGetLinkGroups(myInstance, scope, groups.take());

        if (result == TEResultSuccess)
        {
            for (int32_t i = 0; i < groups->count; i++)
            {
                TouchObject<TELinkInfo> group;
                result = TEInstanceLinkGetInfo(myInstance, groups->strings[i], group.take());
                if (result == TEResultSuccess)
                {
                    // Use group info here
                }
                TouchObject<TEStringArray> children;
                if (result == TEResultSuccess)
                {
                    result = TEInstanceLinkGetChildren(myInstance, groups->strings[i], children.take());
                }
                if (result == TEResultSuccess)
                {
                    for (int32_t j = 0; j < children->count; j++)
                    {
                        TouchObject<TELinkInfo> info;

                        result = TEInstanceLinkGetInfo(myInstance, children->strings[j], info.take());

                        if (result == TEResultSuccess)
                        {
                            qDebug() << "Found"
                                     << (scope == TEScopeInput ? "input" : "output")
                                     << "link:"
                                     << QString::fromUtf8(info->identifier)
                                     << "type:"
                                     << info->type;
                            if(scope == TEScopeInput){
                                lastInputLinks.append(QString::fromUtf8(info->identifier));
                            }
                            if (result == TEResultSuccess && info->type == TELinkTypeTexture)
                            {
                                if(scope == TEScopeOutput)
                                {
                                    myRenderer->addOutputImage();
                                    myOutputLinkTextureMap[info->identifier] = myRenderer->getRightSideImageCount() - 1;
                                }
                            }
                        }
                    }
                }
            }
            emit linksChanged();
        }
    }

    myRenderer->endImageLayout();
}


bool DsQmlTouchEngineInstance::initialize(QRhi* rhi, TEGraphicsAPI apiType)
{
    switch (apiType){
        case TEGraphicsAPI_OpenGL:
            myMode = TEGraphicsAPI_OpenGL;
            myRenderer = std::make_unique<OpenGLRenderer>();
            break;
#ifdef _WIN32
        case TEGraphicsAPI_D3D12:
            myMode = TEGraphicsAPI_D3D12;
            myRenderer = std::make_unique<D3D12Renderer>();
            break;
        case TEGraphicsAPI_Vulkan:
            myMode = TEGraphicsAPI_Vulkan;
            myRenderer = std::make_unique<VulkanRenderer>();
            break;
#endif
        default:
            return false;
    }

    connect(m_window,&QQuickWindow::afterFrameEnd,this,&DsQmlTouchEngineInstance::update,Qt::DirectConnection);

    // Connect to sceneGraphInvalidated to release textures before device is destroyed
    connect(m_window,&QQuickWindow::sceneGraphInvalidated,this,&DsQmlTouchEngineInstance::onSceneGraphInvalidated,Qt::DirectConnection);

    if (!myRenderer->setup(m_window))
    {
        return false;
    }

    TEResult teresult = TEInstanceCreate(eventCallback, linkEventCallback, this, myInstance.take());

    if (teresult == TEResultSuccess)
    {
        teresult = TEInstanceAssociateGraphicsContext(myInstance, myRenderer->getTEContext());
    }
    if (teresult == TEResultSuccess)
    {
        teresult = TEInstanceConfigure(myInstance, nullptr, TETimeExternal);
    }

    if (teresult != TEResultSuccess)
    {
        return false;
    }

    setIsReady(true);
    return true;
}

bool DsQmlTouchEngineInstance::loadComponent()
{
    std::string utf8 = std::string(m_componentPath.toUtf8().constData());

    HRESULT result = S_OK;
    if (utf8.empty())
    {
        return false;
    }

    TEResult teresult = TEInstanceConfigure(myInstance, utf8.c_str(), TETimeExternal);
    if(teresult == TEResultSuccess){
        teresult = TEInstanceSetFrameRate(myInstance, m_frameRate, 1);
    }
    if (teresult == TEResultSuccess)
    {
        teresult = TEInstanceLoad(myInstance);
    }
    if (teresult == TEResultSuccess)
    {
        teresult = TEInstanceResume(myInstance);
    }
    assert(teresult == TEResultSuccess);
    myDidLoad = true;
    emit isLoadedChanged();
    //const auto interval = static_cast<UINT>(std::ceil(1000. / m_frameRate / 2.));

    //disconnect(&m_updateTimer,&QTimer::timeout,this,&TouchEngineInstance2::update);

    //m_updateTimer.setInterval(interval);

    //connect(&m_updateTimer,&QTimer::timeout,this,&TouchEngineInstance2::update);

    //m_updateTimer.start();

    //SetTimer(myWindow, UpdateTimerID, interval, nullptr);
    return true;
}

void DsQmlTouchEngineInstance::unloadComponent()
{
    if (myInstance.get()) {
        TEInstanceSuspend(myInstance.get());

        // Unlock all OpenGL textures before unloading
        //for (auto it = m_lockedGLTextures.begin(); it != m_lockedGLTextures.end(); ++it) {
          //  if (it.value().get()) {
           //     TEOpenGLTexture* glTex = reinterpret_cast<TEOpenGLTexture*>(it.value().get());
           //     TEOpenGLTextureUnlock(glTex);
           // }
        //}
        //m_lockedGLTextures.clear();

        TEInstanceUnload(myInstance.get());
    }

    myRenderer->stop();
    myRenderer.reset();
    myOutputLinkTextureMap.clear();
    myPendingOutputTextures.clear();
    myDidLoad = false;
    myIsReady = false;

    emit isLoadedChanged();
    emit isReadyChanged();
}

void DsQmlTouchEngineInstance::startNewFrame() {


    if (myOtherDidLoad && !myOtherInFrame) {

        emit frameFinished();
        setInFrame(true);

        int64_t time = getRenderTime();

        qint64 currentTime = m_frameTimer.nsecsElapsed();

        // Convert nanoseconds to TouchEngine time format
        int64_t timeValue = currentTime;
        int32_t timeScale = 1000000000; // nanoseconds
        myLastResult = TEInstanceStartFrameAtTime(myInstance, time, TimeRate, false);

        if (myLastResult == TEResultSuccess) {
            myLastFloatValue += 1.0 / (60.0 * 8.0);
        } else {
            //qDebug() << "Error: " << std::string(TEResultGetDescription(myLastResult));
            setInFrame(false);
        }
    }
}

void DsQmlTouchEngineInstance::update() {
    bool configured, ready, loaded, linksChanged, inFrame;
    getState(configured, ready, loaded, linksChanged, inFrame);
    myOtherDidLoad = loaded;
    myOtherInFrame = inFrame;
    if (configured) {
        QString message;
        if (TEResultGetSeverity(myConfigureResult) == TESeverityError) {
            const char *description = TEResultGetDescription(myConfigureResult);

            message = "There was an error configuring TouchEngine: ";
            if (description) {
                message += description;
            } else {
                message += std::to_string(myConfigureResult);
            }

            myConfigureError = true;
        } else {
            myConfigureError = !myRenderer->configure(myInstance, message);
        }
        if (myConfigureError) {
            m_errorString = message;
            emit errorStringChanged();
        }
    }

    if (myConfigureError) {
        return;
    }

    bool changed = linksChanged;

    // Make any pending renderer state updates
    if (linksChanged) {
        applyLayoutChange();
    }

    if (loaded && !inFrame) {
        changed = changed || applyOutputTextureChange();
        emit canUpdateLinks(myInstance);

        startNewFrame();
    }
    if (changed) {
        // render(loaded);
    }
}

QString DsQmlTouchEngineInstance::componentPath() const
{
    QMutexLocker locker(&myMutex);
    return m_componentPath;
}

void DsQmlTouchEngineInstance::setComponentPath(const QString &newComponentPath)
{
    if (m_componentPath == newComponentPath)
        return;
    m_componentPath = newComponentPath;
    emit componentPathChanged();
}

bool DsQmlTouchEngineInstance::isLoaded() const
{
    QMutexLocker locker(&myMutex);
    return myDidLoad;
}

bool DsQmlTouchEngineInstance::isReady() const
{
    QMutexLocker locker(&myMutex);
    return myIsReady;
}

void DsQmlTouchEngineInstance::setIsReady(bool ready) {

    QMutexLocker locker(&myMutex);
    if(myIsReady == ready) return;
    myIsReady = ready;
    emit isReadyChanged();
}

QString DsQmlTouchEngineInstance::errorString() const
{
    return m_errorString;
}

qreal DsQmlTouchEngineInstance::frameRate() const
{
    return m_frameRate;
}

void DsQmlTouchEngineInstance::setFrameRate(qreal newFrameRate)
{
    if (qFuzzyCompare(m_frameRate, newFrameRate))
        return;
    m_frameRate = newFrameRate;
    emit frameRateChanged();
}

QString DsQmlTouchEngineInstance::instanceIdString() const
{
    return m_instanceIdString;
}

QString DsQmlTouchEngineInstance::name() const
{
    return m_name;
}

void DsQmlTouchEngineInstance::setName(const QString &newName)
{
    if (m_name == newName)
        return;
    m_name = newName;
    emit nameChanged();
}

QSharedPointer<QRhiTexture> DsQmlTouchEngineInstance::getRhiTexture(QString &linkName)
{
    if (myOutputLinkTextureMap.contains(linkName) )
    {
        if(m_useHeavyTextureGet){
            size_t imageIndex = myOutputLinkTextureMap[linkName];
            std::string idStr = std::string(linkName.toUtf8().constData());
            myRenderer->updateOutputImage(myInstance,imageIndex,idStr);
        }
        return myRenderer->getOutputRhiTexture(myOutputLinkTextureMap[linkName]);
    } else {
        return nullptr;
    }
    return nullptr;
}

// GLint DsQmlTouchEngineInstance::getGLTexture(QString &linkName)
// {
//     if (myOutputLinkTextureMap.contains(linkName))
//     {
//         return TEOpen myRenderer->getOutputImage(myOutputLinkTextureMap[linkName]);
//     }
//     return 0;
// }

int64_t
DsQmlTouchEngineInstance::getRenderTime()
{
    LARGE_INTEGER now{ 0 };

    if (myStartTime.QuadPart == 0)
    {
        QueryPerformanceFrequency(&myPerformanceCounterFrequency);
        QueryPerformanceCounter(&myStartTime);
        now.QuadPart = 0;
    }
    else
    {
        QueryPerformanceCounter(&now);
        now.QuadPart -= myStartTime.QuadPart;
    }

    now.QuadPart *= TimeRate;
    now.QuadPart /= myPerformanceCounterFrequency.QuadPart;

    return now.QuadPart;
}

TEGraphicsContext *DsQmlTouchEngineInstance::graphicsContext() const
{
    if (!myRenderer)
        return nullptr;
    return myRenderer->getTEContext();
}

const QOpenGLFunctions *DsQmlTouchEngineInstance::getGLFunctions() const
{
    OpenGLRenderer* oglRender = dynamic_cast<OpenGLRenderer*>(myRenderer.get());
    if(oglRender) {
        return oglRender->getGLFunctions();
    }
    return nullptr;
}

void DsQmlTouchEngineInstance::onSceneGraphInvalidated()
{
    // Release Vulkan/D3D12 textures before the graphics device is destroyed
    if (myRenderer) {
        myRenderer->releaseTextures();
    }
}
