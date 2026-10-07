#include "dsQsys.h"
#include <QtCore/qtsymbolmacros.h>
#include <QtQml/qqmlextensionplugin.h>

QT_DECLARE_EXTERN_RESOURCE(qmake_Dsqt_Qsys)
QT_DECLARE_EXTERN_SYMBOL_VOID(qml_register_types_Dsqt_Qsys)

class Dsqt_QsysPlugin : public QQmlEngineExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)
  public:
    explicit Dsqt_QsysPlugin(QObject* parent = nullptr) : QQmlEngineExtensionPlugin(parent) {
        QT_KEEP_SYMBOL(qml_register_types_Dsqt_Qsys)
        QT_KEEP_RESOURCE(qmake_Dsqt_Qsys)
    }
    void initializeEngine(QQmlEngine*, const char*) override {
        // Importing Dsqt.Qsys starts the configured listener even when the
        // application's first send is inside a button handler.
        dsqt::DsQsys::instance();
    }
};

#include "dsQsysPlugin.moc"
