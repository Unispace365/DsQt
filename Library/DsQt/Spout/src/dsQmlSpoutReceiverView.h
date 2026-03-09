#pragma once

#include <QQuickItem>
#include <QPointer>
#include <QtQml/qqml.h>
#include <rhi/qrhi.h>

#include <memory>

#include "dsSpoutReceiver.h"

class DsSpoutTextureNode;
class DsSpoutTextureImporter;

/**
 * DsQmlSpoutReceiverView - QML item that displays received Spout frames.
 *
 * Automatically detects the RHI backend (D3D11, D3D12, Vulkan, OpenGL) and
 * creates the appropriate texture importer for zero-copy display.
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
    QPointer<DsSpoutReceiver> m_receiver;
    std::unique_ptr<DsSpoutTextureImporter> m_importer;
    bool m_resetNode = false;

    void connectReceiver();
    void disconnectReceiver();
};
