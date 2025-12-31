#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include "touchenginemanager.h"
#include "touchengineinstance.h"
#include "touchengineoutputview.h"

int main(int argc, char *argv[])
{
    qInfo() << "Starting TouchEngineQt Application";
    QGuiApplication app(argc, argv);
    
    app.setOrganizationName("TouchDesigner");
    app.setOrganizationDomain("derivative.ca");
    app.setApplicationName("TouchEngineQt");


    //lets try forcing D3D12 or Vulkan
    //QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    //QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
    //Currently only works with OpenGL.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/qt/qml/TouchEngineQt/qml/Main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                qInfo() << "Failed to load QML file:" << url;
                QCoreApplication::exit(-1);
            }
        }, Qt::QueuedConnection);

    auto list = engine.importPathList();
    for(auto path:list) {
        qDebug() << "Import path:" << path;
    }


    engine.loadFromModule("TouchEngineQt", "SimpleMain");
    
    if (engine.rootObjects().isEmpty()) {
        qInfo() << "No root objects found after loading QML.";
        return -1;
    }
    
    // Initialize graphics after window is created
    QObject *rootObject = engine.rootObjects().first();


    if (auto* window = qobject_cast<QQuickWindow*>(rootObject)) {
        auto setApi = [window]() {
            QSGRendererInterface* rif = window->rendererInterface();
            QRhi* rhi = window->rhi();
            rhi->nativeHandles();
            if (rif) {
                // Get native graphics device based on backend
                void* device = nullptr;
                TouchEngineInstance::TEGraphicsAPI apiType = TouchEngineInstance::TEGraphicsAPI_Unknown;

                switch (rif->graphicsApi()) {
                case QSGRendererInterface::Direct3D11: {
                    device = rif->getResource(window, QSGRendererInterface::DeviceResource);
                    apiType = TouchEngineInstance::TEGraphicsAPI_D3D11;
                    qDebug() << "Using Direct3D 11";
                    break;
                }
                case QSGRendererInterface::Direct3D12: {
                    device = rif->getResource(window, QSGRendererInterface::DeviceResource);
                    apiType = TouchEngineInstance::TEGraphicsAPI_D3D12;
                    qDebug() << "Using Direct3D 12";
                    break;
                }
                case QSGRendererInterface::Vulkan: {
                    device = rif->getResource(window, QSGRendererInterface::DeviceResource);
                    apiType = TouchEngineInstance::TEGraphicsAPI_Vulkan;
                    qDebug() << "Using Vulkan";
                    break;
                }
                case QSGRendererInterface::OpenGL: {
                    // OpenGL context should be current
                    apiType = TouchEngineInstance::TEGraphicsAPI_OpenGL;
                    qDebug() << "Using OpenGL";
                    break;
                }
                case QSGRendererInterface::Metal: {
                    device = rif->getResource(window, QSGRendererInterface::DeviceResource);
                    apiType = TouchEngineInstance::TEGraphicsAPI_Metal;
                    qDebug() << "Using Metal";
                    break;
                }
                default:
                    qWarning() << "Unsupported graphics API";
                    break;
                }

                if (apiType != TouchEngineInstance::TEGraphicsAPI_Unknown) {
                    // Initialize TouchEngineManager with the detected API
                    TouchEngineManager* manager = TouchEngineManager::inst();

                    // Store API type in manager for future instances
                    manager->setGraphicsAPI(apiType);
                    manager->setWindow(window);
                    if (device || apiType == TouchEngineInstance::TEGraphicsAPI_OpenGL) {
                        manager->initializeGraphics(rhi,device);
                    }
                }
            }
        };
        if(window->isSceneGraphInitialized()) {
            setApi();
        } else {
            QObject::connect(window, &QQuickWindow::sceneGraphInitialized, rootObject, setApi, Qt::DirectConnection);
        }

    }
    
    return app.exec();
}
