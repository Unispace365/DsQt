#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QApplication>
#include <QtQml/qqmlextensionplugin.h>
Q_IMPORT_QML_PLUGIN(Dsqt_WafflesPlugin)
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)
#include <QDir>
#include <QImage>
#include <QUrl>

class WhiteboardTestSetup : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QUrl outputFile(const QString &name) const {
        return QUrl::fromLocalFile(QDir::current().absoluteFilePath(name));
    }
    Q_INVOKABLE QSize imageSize(const QUrl &file) const {
        return QImage(file.toLocalFile()).size();
    }
    Q_INVOKABLE QColor pixel(const QUrl &file, int x, int y) const {
        const QImage image(file.toLocalFile());
        return image.valid(x, y) ? image.pixelColor(x, y) : QColor();
    }
public slots:
    void applicationAvailable() {
        QCoreApplication::setOrganizationName("Downstream");
        QCoreApplication::setApplicationName("DsQtWhiteboardTests");
    }
    void qmlEngineAvailable(QQmlEngine *engine) {
        engine->rootContext()->setContextProperty("testFiles", this);
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    WhiteboardTestSetup setup;
    return quick_test_main_with_setup(argc, argv, "ds_whiteboard", QUICK_TEST_SOURCE_DIR, &setup);
}
#include "quicktest.moc"
