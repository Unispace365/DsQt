#include "dscontentmodel.h"

namespace dsqt::model {
DSContentModel::DSContentModel(QObject* parent):QQmlPropertyMap(parent){}

QVariant DSContentModel::updateValue(const QString &key, const QVariant &input)
{
    qWarning()<<"Attempting to set value for '"<<key<<"' but DSContentModel is immutable";
    return this->value(key);
}




}
