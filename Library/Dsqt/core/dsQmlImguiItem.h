#ifndef DSQMLIMGUIITEM_H
#define DSQMLIMGUIITEM_H

#include <qrhiimguiitem.h>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class DsQmlImguiItem : public QRhiImguiItem
{
	Q_OBJECT
    QML_NAMED_ELEMENT(DsImgui)

  public:
    DsQmlImguiItem(QQuickItem* parent=nullptr):QRhiImguiItem(parent){

	}
	QVector<std::function<void()>> callbacks;
	void frame() override {
		for(auto &f : callbacks){
			f();
		}
	}

};

#endif // DSQMLIMGUIITEM_H
