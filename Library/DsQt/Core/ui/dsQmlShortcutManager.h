#ifndef DSQMLSHORTCUTMANAGER_H
#define DSQMLSHORTCUTMANAGER_H

#include <QObject>
#include <QQmlEngine>

class DsQmlShortcutManager : public QObject {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(DsShortcutManager)
  public:
    explicit DsQmlShortcutManager(QObject* parent = nullptr);

  signals:
};

#endif // DSQMLSHORTCUTMANAGER_H
