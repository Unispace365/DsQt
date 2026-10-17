#include <QApplication>
#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQuickTest/quicktest.h>

#ifdef DSQT_EFFECTS_STATIC_PLUGIN
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)
#endif

class EffectsTestSetup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // No Q_INIT_RESOURCE here: importing Core must retain its shaders,
        // including when the consumer only links the installed static archives.
        engine->rootContext()->setContextProperty(
            "effectsShaderAvailable",
            QFile::exists(":/qt/qml/Dsqt/Core/shaders/dsMultiEffect.frag.qsb"));
    }
};

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    EffectsTestSetup setup;
    return quick_test_main_with_setup(
        argc, argv, "ds_multieffect", QUICK_TEST_SOURCE_DIR, &setup);
}

#include "quicktest.moc"