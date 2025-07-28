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
    QML_FILES qml/DSAppBase.qml
    QML_FILES qml/DSScaleLoader.qml
    QML_FILES qml/waffles/DSRadialMenu.qml
    QML_FILES qml/waffles/DSRadialMenuIconItem.qml
    QML_FILES qml/waffles/menu_launcher/Section.qml
    QML_FILES qml/waffles/WaffleStage.qml
    QML_FILES qml/bridge/DsBridgeQueryControls.qml
    QML_FILES qml/waffles/menu_launcher/TestLauncher.qml
    QML_FILES qml/waffles/media_viewer/TitledMediaViewer.qml
    QML_FILES qml/waffles/media_viewer/ControlSet.qml
    QML_FILES qml/waffles/Viewer.qml
    QML_FILES qml/waffles/QuickMenu.qml
    QML_FILES qml/waffles/menu_launcher/ContentLauncher.qml

    SOURCES
    #dsqtbase.cpp dsqtbase.h
    core/dsEnvironment.cpp core/dsEnvironment.h
    core/dsGuiApplication.cpp core/dsGuiApplication.h
    core/dsQmlImguiItem.h
    core/dsQmlApplicationEngine.cpp core/dsQmlApplicationEngine.h
    model/content_model.cpp model/content_model.h
    model/qmlcontentmodel.h model/qmlcontentmodel.cpp
    model/qjsonmodel.cpp model/qjsonmodel.h
    model/dsresource.cpp model/dsresource.h
    network/dsnodewatcher.cpp network/dsnodewatcher.h
    settings/dssettings.cpp settings/dssettings.h
    settings/dssettings_base.cpp
    settings/dssettings_collections.cpp
    settings/dssettings_datetime.cpp
    settings/dssettings_geom_common.cpp
    settings/dssettings_geom_glm.cpp
    settings/dssettings_geom_qt.cpp
    settings/dssettings_qcolor.cpp
    settings/dssettings_proxy.cpp settings/dssettings_proxy.h
    settings/overload.h
    ui/touchcluster.h ui/touchcluster.cpp
    utility/file_meta_data.cpp utility/file_meta_data.h
    utility/string_util.cpp utility/string_util.h
    utility/url_image_loader.cpp utility/url_image_loader.h
    bridge/dsBridgeQuery.cpp bridge/dsBridgeQuery.h
    waffles.qrc
    SOURCES ui/clustermanager.h ui/clustermanager.cpp
    SOURCES ui/clusterview.h ui/clusterview.cpp
    SOURCES core/dsQmlEnvironment.h core/dsQmlEnvironment.cpp
    SOURCES core/dsQmlObj.h core/dsQmlObj.cpp
    SOURCES model/icontent_helper.h model/icontent_helper.cpp
    SOURCES model/content_helper.h model/content_helper.cpp
    SOURCES model/property_map_diff.h model/property_map_diff.cpp
    SOURCES model/reference_map.h
    SOURCES core/dsReloadUrlInterceptor.h core/dsReloadUrlInterceptor.cpp
    DEPENDENCIES
         QtQuick
         Dsqtplugin
         SOURCES core/dsQmlIdle.h core/dsQmlIdle.cpp
         SOURCES core/dsQmlIdlePreventer.h core/dsQmlIdlePreventer.cpp

)

target_link_libraries(Dsqtplugin PRIVATE Qt6::Core)
target_link_libraries(Dsqtplugin PRIVATE Qt6::Quick)

target_include_directories(Dsqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} bridge core model network qml resources settings ui utility)
