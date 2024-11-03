#ifndef DSRADIALMENU_H
#define DSRADIALMENU_H

#include <QQmlComponent>
#include <QQuickItem>
class DSRadialMenu : public QQuickItem {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QQmlListProperty<QObject> data READ data NOTIFY dataChanged FINAL)
	Q_PROPERTY(QQmlComponent *itemDelegate READ itemDelegate WRITE setItemDelegate NOTIFY itemDelegateChanged)
	Q_CLASSINFO("DefaultProperty", "data");

  public:
	DSRadialMenu(QQuickItem *parent = nullptr);

	QQmlListProperty<QObject> data();


	QQmlComponent *itemDelegate() const;
	void		   setItemDelegate(QQmlComponent *newItemDelegate);

  signals:
	void dataChanged();
	void itemDelegateChanged();

  private:
	static void		 appendData(QQmlListProperty<QObject> *list, QObject *item);
	QList<QObject *> m_data;
	QQmlComponent	*m_itemDelegate = nullptr;
};

#endif	// DSRADIALMENU_H
