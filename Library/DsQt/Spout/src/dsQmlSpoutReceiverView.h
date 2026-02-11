#pragma once

#include <QQuickItem>
#include <QtQml/qqml.h>
#include <rhi/qrhi.h>

#include "dsSpoutReceiver.h"

class DsSpoutTextureNode;

/**
 * DsQmlSpoutReceiverView - QML item that displays received Spout frames.
 *
 * QML usage:
 *   DsSpoutReceiverView {
 *       anchors.fill: parent
 *       receiver: mySpoutReceiver
 *   }
 */
class DsQmlSpoutReceiverView : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsSpoutReceiverView)

    Q_PROPERTY(DsSpoutReceiver* receiver READ receiver WRITE setReceiver NOTIFY receiverChanged)

public:
    explicit DsQmlSpoutReceiverView(QQuickItem* parent = nullptr);
    ~DsQmlSpoutReceiverView() override;

    DsSpoutReceiver* receiver() const { return m_receiver; }
    void setReceiver(DsSpoutReceiver* receiver);

signals:
    void receiverChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void releaseResources() override;

private slots:
    void onFrameReceived();

private:
    DsSpoutReceiver* m_receiver = nullptr;
    QSharedPointer<QRhiTexture> m_rhiTexture;
    void* m_currentNativeHandle = nullptr;
    bool m_resetNode = false;

    void connectReceiver();
    void disconnectReceiver();
};
