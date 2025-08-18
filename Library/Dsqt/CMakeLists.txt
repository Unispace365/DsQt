set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/Library")
set(QML_IMPORT_PATH

     "${QML_ROOT};${QML_ROOT}/qml;${QML_ROOT}/qml/waffles;${QML_ROOT}/qml/bridge;${QML_BIN_ROOT};${QML_BIN_ROOT}/qml;${QML_BIN_ROOT}/qml/waffles;${QML_BIN_ROOT}/qml/bridge"
     CACHE PATH "QT creator qml import path" FORCE
 )

find_package(Qt6 REQUIRED COMPONENTS Core Multimedia Quick)

qt_add_qml_module(Dsqt
    URI Dsqt
    STATIC
    VERSION 1.0
    QML_FILES 
        qml/bridge/DsBridgeQueryControls.qml
        qml/DsAppBase.qml
        qml/DsScaleLoader.qml
        qml/ui/DsPlaylistView.qml
        qml/ui/DsQuickMenu.qml
        qml/waffles/DsViewer.qml
        qml/waffles/DsWaffleStage.qml
        qml/waffles/media_viewer/DsControlSet.qml
        qml/waffles/media_viewer/DsTitledMediaViewer.qml
        qml/waffles/menu_launcher/DsContentLauncher.qml
        qml/waffles/menu_launcher/DsSection.qml
        qml/waffles/menu_launcher/DsTestLauncher.qml
        

    SOURCES
        bridge/dsBridgeQuery.cpp bridge/dsBridgeQuery.h
        bridge/dsBridgeWatcher.cpp bridge/dsBridgeWatcher.h
        bridge/dsQmlBridgeLog.cpp bridge/dsQmlBridgeLog.h
        core/dsEnvironment.cpp core/dsEnvironment.h
        core/dsGuiApplication.cpp core/dsGuiApplication.h
        core/dsQmlApplicationEngine.cpp core/dsQmlApplicationEngine.h
        core/dsQmlEnvironment.h core/dsQmlEnvironment.cpp
        core/dsQmlEnvironment.h core/dsQmlEnvironment.cpp
        core/dsQmlIdle.h core/dsQmlIdle.cpp
        core/dsQmlIdlePreventer.h core/dsQmlIdlePreventer.cpp
        core/dsQmlImguiItem.h
        core/dsQmlObj.h core/dsQmlObj.cpp
        core/dsQmlObj.h core/dsQmlObj.cpp
        core/dsReloadUrlInterceptor.h core/dsReloadUrlInterceptor.cpp
        core/dsReloadUrlInterceptor.h core/dsReloadUrlInterceptor.cpp
        model/dsContentModel.cpp model/dsContentModel.h
        model/dsContentModel.cpp model/dsContentModel.h
        model/dsIContentHelper.cpp model/dsIContentHelper.h
        model/dsIContentHelper.cpp model/dsIContentHelper.h
        model/dsPropertyMapDiff.cpp model/dsPropertyMapDiff.h
        model/dsPropertyMapDiff.cpp model/dsPropertyMapDiff.h
        model/dsQmlContentHelper.cpp model/dsQmlContentHelper.h
        model/dsQmlContentHelper.cpp model/dsQmlContentHelper.h
        model/dsQmlContentModel.cpp model/dsQmlContentModel.h
        model/dsQmlContentModel.cpp model/dsQmlContentModel.h
        model/dsQmlEventSchedule.cpp model/dsQmlEventSchedule.h
        model/dsReferenceMap.h model/dsResource.cpp model/dsResource.h
        model/dsReferenceMap.h model/dsResource.cpp model/dsResource.h
        model/qjsonmodel.cpp model/qjsonmodel.h
        model/qJsonModel.cpp model/qJsonModel.h
        network/dsNodeWatcher.cpp network/dsNodeWatcher.h
        settings/dsOverloaded.h
        settings/DsOverloaded.h
        settings/dsQmlSettingsProxy.cpp settings/dsQmlSettingsProxy.h
        settings/dsQmlSettingsProxy.cpp settings/dsQmlSettingsProxy.h
        settings/dsSettings.cpp settings/dsSettings.h
        settings/dsSettingsBase.cpp
        settings/dsSettingsCollections.cpp
        settings/dsSettingsDateTime.cpp
        settings/dsSettingsGeomCommon.cpp
        settings/dsSettingsGeomGlm.cpp
        settings/dsSettingsGeomQt.cpp
        settings/dsSettingsQColor.cpp
        ui/dsQmlClock.h ui/dsQmlClock.cpp
        ui/dsQmlClusterManager.h ui/dsQmlClusterManager.cpp
        ui/dsQmlClusterView.h ui/dsQmlClusterView.cpp
        ui/dsQmlFpsMonitor.h ui/dsQmlFpsMonitor.cpp
        ui/dsQmlPlaylistControl.h ui/dsQmlPlaylistControl.cpp
        ui/dsQmlTouchCluster.h ui/dsQmlTouchCluster.cpp
        utility/dsFileMetaData.cpp utility/dsFileMetaData.h
        utility/dsStringUtils.cpp utility/dsStringUtils.h
        utility/dsUrlImageLoader.cpp utility/dsUrlImageLoader.h
        waffles.qrc

    DEPENDENCIES
         QtQuick
         Dsqtplugin

)

target_link_libraries(Dsqtplugin PRIVATE Qt6::Core Qt6::Multimedia Qt6::Quick)

target_include_directories(Dsqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} bridge core model network qml resources settings ui utility)
