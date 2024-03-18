#ifndef DSIMGUI_ITEM_H
#define DSIMGUI_ITEM_H

#include <qrhiimguiitem.h>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class DsImguiItem : public QRhiImguiItem
{
	Q_OBJECT
	QML_NAMED_ELEMENT(Imgui)

  public:
	DsImguiItem(QQuickItem* parent=nullptr):QRhiImguiItem(parent){

	}
	QVector<std::function<void()>> callbacks;
	void frame() override {
		for(auto &f : callbacks){
			f();
		}
	}

};

#endif // DSIMGUI_ITEM_H
