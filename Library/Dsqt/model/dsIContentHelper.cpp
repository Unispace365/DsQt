#include "model/dsIContentHelper.h"

namespace dsqt::model {
const QString IContentHelper::DEFAULTCATEGORY	  = "default!";
const QString IContentHelper::WAFFLESCATEGORY	  = "waffles!";
const QString IContentHelper::PRESENTATIONCATEGORY = "presentation";
const QString IContentHelper::AMBIENTCATEGORY	  = "ambient";

IContentHelper::IContentHelper(QObject *parent) : QObject(parent)
{}

}
