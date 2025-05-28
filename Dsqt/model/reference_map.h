#ifndef REFERENCE_MAP_H
#define REFERENCE_MAP_H


#include <QString>
#include <QMap>

namespace dsqt::model {
class QmlContentModel;
//a reference map is basically a QMap. isTemp can be marked as true to
//aid in debugging
class ReferenceMap : public QMap<QString, QmlContentModel *>
{
public:
    ReferenceMap():QMap<QString,QmlContentModel*>(){};
    bool isTemp = false;
};
}

#endif // REFERENCE_MAP_H
