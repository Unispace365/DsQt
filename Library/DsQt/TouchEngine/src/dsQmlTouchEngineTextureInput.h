#ifndef DSQMLTOUCHENGINETEXTUREINPUT_H
#define DSQMLTOUCHENGINETEXTUREINPUT_H

#include "dsQmlTouchEngineInputBase.h"
#include <QPointer>
#include <QQuickItem>
#include <rhi/qrhi.h>
#include <QSGTextureProvider>
#include <TouchEngine/TouchObject.h>
#include <TouchEngine/TEOpenGL.h>

/**
 * DsQmlTouchEngineTextureInput - QML component for feeding a QQuickItem's layer texture
 * into a TouchEngine texture link.
 *
 * Usage in QML:
 * DsTouchEngineTextureInput {
 *     instanceId: teInstance.instanceIdString
 *     linkName: "video_in"
 *     sourceItem: myItem   // must have layer.enabled: true
 * }
 */

class DsQmlTouchEngineTextureInput;
struct TextureKeeper
{
    TextureKeeper() {}
    ~TextureKeeper() {}
    TEOpenGLTexture* rawTexture;
    TouchObject<TEOpenGLTexture> teTexture;
    DsQmlTouchEngineTextureInput* owner;
    GLuint texId;
};

class DsQmlTouchEngineTextureInput : public DsQmlTouchEngineInputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineTextureInput)

    Q_PROPERTY(QQuickItem* sourceItem READ sourceItem WRITE setSourceItem NOTIFY sourceItemChanged)


public:
    explicit DsQmlTouchEngineTextureInput(QObject *parent = nullptr);
    ~DsQmlTouchEngineTextureInput() override = default;

    QQuickItem *sourceItem() const { return m_sourceItem; }
    void setSourceItem(QQuickItem *item);

signals:
    void sourceItemChanged();
    void requireLayerEnabledChanged();

protected:
    void applyValue(TEInstance *teInstance) override;
    static void	textureReleaseCallback(GLuint texture, TEObjectEvent event, void *info);

private slots:
    void handleWindowChanged();
    void handleAfterRendering();

private:
    void attachToWindow(QQuickWindow *w);
    void detachFromWindow();
    GLuint grabLayerTexture();

    QPointer<QQuickItem> m_sourceItem;
    QPointer<QQuickWindow> m_window;

    // we just cache the latest QRhiTexture we got from the item
    QRhiTexture *m_lastRhiTexture = nullptr;
    QSize m_lastSize;
    GLuint m_lastId=0;
    TouchObject<TEOpenGLTexture> m_teTexture;
    TouchObject<TED3DSharedTexture> m_sharedTexture;
    static QOpenGLFunctions* m_funcs;
    QMutex m_mutex;
    QList<TextureKeeper*> m_textureKeepersDeletes;

};

#endif // DSQMLTOUCHENGINETEXTUREINPUT_H
