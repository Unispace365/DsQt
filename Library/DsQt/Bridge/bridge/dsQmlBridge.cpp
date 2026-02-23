#include "bridge/dsQmlBridge.h"

#include <QCoreApplication.h>

namespace dsqt::bridge {

DsQmlBridge::DsQmlBridge()
    : QObject(nullptr)
{
    // HACK: We need to tell the QML Engine that this object will be destructed via C++, or the program will crash upon exit
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
    m_content = model::ContentModel::createNamed("Bridge");
}

model::ContentModel* DsQmlBridge::content() const {
    // Should only be called from main thread! Use database() to obtain thread-safe data model.
    bool isMainThread = QThread::currentThread() == QCoreApplication::instance()->thread();
    Q_ASSERT(isMainThread);

    return m_content;
}

void DsQmlBridge::setContent(model::ContentModel* newContent) {
    if (m_content == newContent || !newContent) return;
    m_content = newContent;
    emit contentChanged();
}

} // namespace dsqt::bridge
