#pragma once

#include <QQuickItem>
#include <QUuid>
#include <QTimer>

class TouchEngineInstance;
class TouchEngineTextureNode;
class QRhiTexture;

/**
 * TouchEngineOutputView - QML item for displaying TouchEngine output
 * Uses Qt Quick Scene Graph for efficient rendering with RHI
 */
class TouchEngineOutputView : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TouchEngineOutputView)
    Q_PROPERTY(QString instanceId READ instanceId WRITE setInstanceId NOTIFY instanceIdChanged)
    Q_PROPERTY(QString outputLink READ outputLink WRITE setOutputLink NOTIFY outputLinkChanged)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY autoUpdateChanged)


public:
    explicit TouchEngineOutputView(QQuickItem* parent = nullptr);
    ~TouchEngineOutputView() override;

    QString instanceId() const { return m_instanceId; }
    void setInstanceId(const QString& id);

    QString outputLink() const { return m_outputLink; }
    void setOutputLink(const QString& link);

    bool autoUpdate() const { return m_autoUpdate; }
    void setAutoUpdate(bool enable);

    Q_INVOKABLE void requestFrame();

signals:
    void instanceIdChanged();
    void outputLinkChanged();
    void autoUpdateChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void releaseResources() override;

private slots:
    void handleFrameFinished();
    void handleTextureUpdated(const QString& linkName);

private:
    QString m_instanceId;
    QString m_outputLink = "output";
    bool m_autoUpdate = true;
    TouchEngineInstance* m_instance = nullptr;
    QSharedPointer<QRhiTexture> m_rhiTexture = nullptr;
    void connectInstance();
    void disconnectInstance();
    QTimer  m_updateTimer;
};
