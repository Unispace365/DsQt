#ifndef TOUCHENGINEINSTANCE_H
#define TOUCHENGINEINSTANCE_H

#include <QObject>
#include <QQmlEngine>
#include <QMutex>
#include <QtCore>
#include <TouchEngine/TouchEngine.h>
#include "Renderer.h"
#include <QQuickWindow>
#include <QUuid>
#include <qtimer.h>
#include <QOpenGLFunctions>
#include <rhi/qrhi.h>

Q_DECLARE_OPAQUE_POINTER(TEInstance*)

class TouchEngineInstance : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TouchEngineInstance)
    QML_UNCREATABLE("TouchEngineInstance2 cannot be created from QML");
    Q_PROPERTY(QString componentPath READ componentPath WRITE setComponentPath NOTIFY componentPathChanged)
    Q_PROPERTY(bool isLoaded READ isLoaded NOTIFY isLoadedChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY isReadyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(qreal frameRate READ frameRate WRITE setFrameRate NOTIFY frameRateChanged)
    Q_PROPERTY(QString instanceIdString READ instanceIdString CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged FINAL);
public:

    // Graphics API enumeration matching TouchEngine
    enum TEGraphicsAPI {
        TEGraphicsAPI_Unknown = 0,
        TEGraphicsAPI_D3D11 = 1,
        TEGraphicsAPI_D3D12 = 2,
        TEGraphicsAPI_Vulkan = 3,
        TEGraphicsAPI_OpenGL = 4,
        TEGraphicsAPI_Metal = 5
    };

    explicit TouchEngineInstance(QObject *parent = nullptr,QQuickWindow* window=nullptr);

    // C++ internal ID
    QUuid instanceId() const { return m_instanceId; }

    TEGraphicsAPI getAPI() const {
        return myMode;
    }

    void didConfigure(TEResult result);
    void didLoad() {
        QMutexLocker locker(&myMutex);
        myDidLoad = true;
    }

    bool initialize(QRhi *rhi = nullptr, TEGraphicsAPI apiType = TEGraphicsAPI_D3D11);

    // Configure and load component
    Q_INVOKABLE bool loadComponent();
    Q_INVOKABLE void unloadComponent();

    QString componentPath() const;
    void setComponentPath(const QString &newComponentPath);

    bool isLoaded() const;

    bool isReady() const;

    QString errorString() const;

    qreal frameRate() const;
    void setFrameRate(qreal newFrameRate);

    QString instanceIdString() const;

    QString name() const;
    void setName(const QString &newName);
    QSharedPointer<QRhiTexture> getRhiTexture(QString& linkName);
    //  getGLTexture(QString& linkName);


    void setIsReady(bool ready);
    TEGraphicsContext *graphicsContext() const;
    const QOpenGLFunctions* getGLFunctions() const;
    bool    hasInputLink(QString identifier);
    bool    doesInputTextureTransfer() { return myRenderer->doesInputTextureTransfer(); }
signals:

    void componentPathChanged();

    void isLoadedChanged();

    void isReadyChanged();

    void errorStringChanged();

    void frameRateChanged();

    void nameChanged();

    void frameFinished();
    void canUpdateLinks(TEInstance* teInstance);
    void linksChanged();

private:
    static void eventCallback(TEInstance* instance, TEEvent event,
                              TEResult result, int64_t start_time_value,
                              int32_t start_time_scale, int64_t end_time_value,
                              int32_t end_time_scale, void* info);

    static void linkEventCallback(TEInstance* instance, TELinkEvent event,
                                 const char* identifier, void* info);

    void	linkValueChange(const char* identifier);
    void    linkLayoutDidChange();
    void	endFrame(int64_t time_value, int32_t time_scale, TEResult result);
    void	getState(bool& configured, bool &ready, bool& loaded, bool& linksChanged, bool& inFrame);
    void	setInFrame(bool inFrame);
    void	applyLayoutChange();
    bool	applyOutputTextureChange();

    int64_t	getRenderTime();
    void startNewFrame();
    void update();

private:
    //start temp data
    static constexpr size_t ImageWidth{ 256 };
    static constexpr size_t ImageHeight{ 256 };
    struct Color {
        int red;
        int green;
        int blue;
    };
    struct Gradient {
        Color start;
        Color end;
    };
    double			myLastFloatValue{ 0.0 };
    TEResult		myLastResult{ TEResultSuccess };
    //end

    QString myPath;
    TEGraphicsAPI myMode;
    TouchObject<TEInstance> myInstance;

    mutable QMutex myMutex;
    bool myDidLoad = false;
    bool myOtherDidLoad = false;
    bool myOtherInFrame = false;
    bool myIsReady = false;
    bool myInFrame = false;
    bool myConfigureRenderer = false;
    bool myConfigureError = false;

    // TE link identifier to renderer index
    QMap<QString, size_t>	myOutputLinkTextureMap;
    QList<QString>		lastInputLinks;
    std::vector<QString>		myPendingOutputTextures;
    bool							myPendingLayoutChange{ false };
    TEResult						myConfigureResult{ TEResultSuccess };
    std::unique_ptr<Renderer>       myRenderer;

    QElapsedTimer m_frameTimer;
    QString m_componentPath;
    QString m_errorString;
    qreal m_frameRate=60;
    QString m_instanceIdString;
    QString m_name;
    QUuid m_instanceId = QUuid::createUuid();
    LARGE_INTEGER	myStartTime{ 0 };
    LARGE_INTEGER	myPerformanceCounterFrequency{ 1 };
    int TimeRate = 6000;
    QTimer  m_updateTimer;
    QQuickWindow *m_window;
    bool m_useHeavyTextureGet = true;
    QTemporaryFile *m_tempFile;
};

#endif // TOUCHENGINEINSTANCE_H
