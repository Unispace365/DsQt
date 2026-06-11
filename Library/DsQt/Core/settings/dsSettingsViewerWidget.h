#pragma once

#include <QWidget>

class QTabWidget;

namespace dsqt {

class Settings;
class SettingsFile;

// A Qt Widgets window that shows every registered SettingsFile as a tab,
// each containing a QTreeView backed by a SettingsTreeModel.
// The tabs rebuild automatically when Settings::instancesChanged fires.
class SettingsViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsViewerWidget(Settings *settings, QWidget *parent = nullptr);

private:
    void rebuild();
    SettingsFile *currentSettingsFile() const;
    void onSave();
    void onRestore();

    Settings   *m_settings;
    QTabWidget *m_tabs;
};

}
