#include "dsQmlTouchEngineStringOutput.h"
#include "dsQmlTouchEngineInstance.h"
#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TouchObject.h>
#include <QDebug>

DsQmlTouchEngineStringOutput::DsQmlTouchEngineStringOutput(QObject *parent)
    : DsQmlTouchEngineOutputBase(parent)
{
}

DsQmlTouchEngineStringOutput::~DsQmlTouchEngineStringOutput()
{
}

void DsQmlTouchEngineStringOutput::readValue(TEInstance* teInstance)
{
    if (!teInstance || linkName().isEmpty()) {
        return;
    }

    QByteArray linkNameUtf8 = linkName().toUtf8();

    // Get the string value from the output link
    TEString* teString = nullptr;
    TEResult result = TEInstanceLinkGetStringValue(teInstance,
                                                    linkNameUtf8.constData(),
                                                    TELinkValueCurrent,
                                                    &teString);

    if (result == TEResultSuccess && teString) {
        const char* str = teString->string;
        QString newValue = QString::fromUtf8(str ? str : "");
        TERelease(&teString);

        if (m_value != newValue) {
            m_value = newValue;
            emit valueChanged();
            emit valueUpdated();
        }
    } else if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to get string value from link '%1': %2")
                          .arg(linkName())
                          .arg(TEResultGetDescription(result));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
    }
}
