#ifndef DSREFERENCEMAP_H
#define DSREFERENCEMAP_H

#include <QMap>
#include <QString>

namespace dsqt::model {
class QmlContentModel;
// a reference map in support of QmlContentModel is basically a QMap. isTemp can be marked as true to
// aid in debugging
class ReferenceMap : public QMap<QString, QmlContentModel*> {
  public:
    ReferenceMap()
        : QMap<QString, QmlContentModel*>() {};
    bool isTemp = false;
};
} // namespace dsqt::model

#endif // REFERENCE_MAP_H
