#include "dsradialmenu.h"
#include "dsmenuitem.h"
#include "qqmlengine.h"
using namespace Qt::Literals::StringLiterals;
namespace dsqt::ui {
DSRadialMenu::DSRadialMenu(QQuickItem *parent) : QQuickItem(parent) {}

QQmlListProperty<QObject> DSRadialMenu::data() {
	return QQmlListProperty<QObject>(this, nullptr, &DSRadialMenu::appendData, nullptr, nullptr, nullptr);
}

void DSRadialMenu::appendData(QQmlListProperty<QObject> *list, QObject *item) {
	DSRadialMenu *menu	   = qobject_cast<DSRadialMenu *>(list->object);
	DSMenuItem	 *menuItem = qobject_cast<DSMenuItem *>(item);
	if (menu) {
		if (menuItem) {
			item->setParent(menu);
			menu->m_data.append(menuItem);
			emit menu->dataChanged();
		} else {

			qmlEngine(menu)->throwError(QJSValue::TypeError, u"Illegal element %1 "_s.arg(item->metaObject()->className()));
		}
	}
}


QQmlComponent *DSRadialMenu::itemDelegate() const {
	return m_itemDelegate;
}

void DSRadialMenu::setItemDelegate(QQmlComponent *newItemDelegate) {
	if (m_itemDelegate == newItemDelegate) return;
	m_itemDelegate = newItemDelegate;
	emit itemDelegateChanged();
}
}
