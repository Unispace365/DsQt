set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}\\Library")
set(QML_IMPORT_PATH

     "${QML_ROOT};${QML_ROOT}\\qml;${QML_ROOT}\\qml\\waffles;${QML_ROOT}\\qml\\bridge;${QML_BIN_ROOT};${QML_BIN_ROOT}\\qml;${QML_BIN_ROOT}\\qml\\waffles;${QML_BIN_ROOT}\\qml\\bridge"
     CACHE PATH "QT creator qml import path" FORCE
 )

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_add_qml_module(Dsqt
    URI Dsqt
    STATIC
    VERSION 1.0
    #QML_FILES DsqtBaseControls.qml
    QML_FILES qml/DsAppBase.qml
    QML_FILES qml/DsScaleLoader.qml
    QML_FILES
    QML_FILES
    QML_FILES qml/waffles/menu_launcher/DsSection.qml
    QML_FILES qml/waffles/DsWaffleStage.qml
    QML_FILES qml/bridge/DsBridgeQueryControls.qml
    QML_FILES qml/waffles/menu_launcher/DsTestLauncher.qml
    QML_FILES qml/waffles/media_viewer/DsTitledMediaViewer.qml
    QML_FILES qml/waffles/media_viewer/DsControlSet.qml
    QML_FILES qml/waffles/DsViewer.qml
    QML_FILES qml/waffles/DsQuickMenu.qml
    QML_FILES qml/waffles/menu_launcher/DsContentLauncher.qml

    SOURCES
    #dsqtbase.cpp dsqtbase.h
    core/dsEnvironment.cpp core/dsEnvironment.h
    core/dsGuiApplication.cpp core/dsGuiApplication.h
    core/dsQmlImguiItem.h
    core/dsQmlApplicationEngine.cpp core/dsQmlApplicationEngine.h
    # model/content_model.cpp model/content_model.h
    # model/qmlcontentmodel.h model/qmlcontentmodel.cpp
    # model/qjsonmodel.cpp model/qjsonmodel.h
    # model/dsresource.cpp model/dsresource.h
    network/dsNodeWatcher.cpp network/dsNodeWatcher.h
    settings/dsSettings.cpp settings/dsSettings.h
    settings/dsSettingsBase.cpp
    settings/dsSettingsCollections.cpp
    settings/dsSettingsDatetime.cpp
    settings/dsSettingsGeomCommon.cpp
    settings/dsSettingsGeomGlm.cpp
    settings/dsSettingsGeomQt.cpp
    settings/dsSettingsQColor.cpp
    settings/dsQmlSettingsProxy.cpp settings/dsQmlSettingsProxy.h
    settings/dsOverloaded.h
    ui/dsQmlTouchCluster.h ui/dsQmlTouchCluster.cpp
    utility/dsFileMetaData.cpp utility/dsFileMetaData.h
    utility/dsStringUtils.cpp utility/dsStringUtils.h
    utility/dsUrlImageLoader.cpp utility/dsUrlImageLoader.h
    bridge/dsBridgeQuery.cpp bridge/dsBridgeQuery.h
    waffles.qrc
    SOURCES ui/dsQmlClustermanager.h ui/dsQmlClustermanager.cpp
    SOURCES ui/dsQmlClusterView.h ui/dsQmlClusterView.cpp
    SOURCES core/dsQmlEnvironment.h core/dsQmlEnvironment.cpp
    SOURCES core/dsQmlObj.h core/dsQmlObj.cpp
    # SOURCES model/icontent_helper.h model/icontent_helper.cpp
    # SOURCES model/dsContentHelper.h model/dsContentHelper.cpp
    # SOURCES model/property_map_diff.h model/property_map_diff.cpp
    # SOURCES model/reference_map.h
    SOURCES core/dsReloadUrlInterceptor.h core/dsReloadUrlInterceptor.cpp

    SOURCES model/dsContentModel.cpp model/dsContentModel.h
    SOURCES model/dsIContentHelper.cpp model/dsIContentHelper.h
    SOURCES model/dsPropertyMapDiff.cpp model/dsPropertyMapDiff.h
    SOURCES model/dsQmlContentHelper.cpp model/dsQmlContentHelper.h
    SOURCES model/dsQmlContentModel.cpp model/dsQmlContentModel.h
    SOURCES model/dsReferenceMap.h model/dsResource.cpp model/dsResource.h
    SOURCES model/qJsonModel.cpp model/qJsonModel.h
    DEPENDENCIES
         QtQuick
         Dsqtplugin
         SOURCES core/dsQmlIdle.h core/dsQmlIdle.cpp
         SOURCES core/dsQmlIdlePreventer.h core/dsQmlIdlePreventer.cpp

)

target_link_libraries(Dsqtplugin PRIVATE Qt6::Core)
target_link_libraries(Dsqtplugin PRIVATE Qt6::Quick)

target_include_directories(Dsqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} bridge core model network qml resources settings ui utility)
