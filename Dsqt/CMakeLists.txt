cmake_minimum_required(VERSION 3.29)
include(FetchContent)
project(Dsqt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_AUTOUIC ON)


set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(QT_QML_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/Dsqt)
#set(QML_ROOT ${CMAKE_BINARY_DIR}\\Dsqt)
set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_IMPORT_PATH

     "${QML_ROOT};${QML_ROOT}\\qml;${QML_ROOT}\\qml\\waffles;${QML_ROOT}\\qml\\bridge"
     CACHE PATH "QT creator qml import path" FORCE
 )

find_package(Qt6 6.9 COMPONENTS Core Quick Gui ShaderTools Sql REQUIRED COMPONENTS Test)
qt_standard_project_setup(REQUIRES 6.9)

qt_add_qml_module(Dsqt
    URI Dsqt
    STATIC
    VERSION 1.0
    QML_FILES DsqtBaseControls.qml
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
    QML_FILES qml/waffles/menu_launcher/ContentLauncher.qml

    SOURCES
    dsqtbase.cpp dsqtbase.h
    core/dsenvironment.cpp core/dsenvironment.h
    core/dsguiapplication.cpp core/dsguiapplication.h
    core/dsimgui_item.h
    core/dsqmlapplicationengine.cpp core/dsqmlapplicationengine.h
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
    SOURCES core/dsenvironmentqml.h core/dsenvironmentqml.cpp
    SOURCES core/ds_qml_obj.h core/ds_qml_obj.cpp
    SOURCES model/icontent_helper.h model/icontent_helper.cpp
    SOURCES model/content_helper.h model/content_helper.cpp
    SOURCES model/property_map_diff.h model/property_map_diff.cpp
    SOURCES model/reference_map.h
    SOURCES core/reloadurlinterceptor.h core/reloadurlinterceptor.cpp
    DEPENDENCIES
         QtQuick
         Dsqtplugin

)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.2.0
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_GetProperties(tomlplusplus)
FetchContent_MakeAvailable(tomlplusplus)

##Fetch GLM from the g-truc glm repo.
##https://glm.g-truc.net/0.9.9/index.html
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_GetProperties(glm)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    qtimgui
    GIT_REPOSITORY https://github.com/alpqr/qrhiimgui2.git
    GIT_TAG        94c99a8f141469169421d17b64007793988c4009
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_GetProperties(qtimgui)
FetchContent_MakeAvailable(qtimgui)

##connect qrhiimgui
set(imgui_base ${qtimgui_SOURCE_DIR}/imgui)
set(imgui_target Dsqt)
#include(${imgui_base}/imgui.cmakeinc)
#include(${imgui_base}/imguiquick.cmakeinc)
target_include_directories(Dsqt PUBLIC ${imgui_base})

##connect glm
target_include_directories(Dsqt PUBLIC ${glm_SOURCE_DIR})

##connect toml++
target_include_directories(Dsqt PUBLIC ${tomlplusplus_SOURCE_DIR}/include)


set_target_properties(Dsqt PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_compile_definitions(Dsqt
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(Dsqt
PRIVATE Qt6::Quick Qt6::Core Qt6::Gui Qt6::ShaderTools Qt6::GuiPrivate Qt6::Sql)

target_include_directories(Dsqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "core" "model" "network" "settings" "ui" "utility" "bridge" ${imgui_base})

cmake_path(NATIVE_PATH CMAKE_CURRENT_SOURCE_DIR DS_QMLLINT_ROOT)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/.qmllint.in.ini ${CMAKE_SOURCE_DIR}/.qmllintx.ini)
#configure_file(${CMAKE_SOURCE_DIR}/cmake/FindDsqt.in.cmake ${CMAKE_SOURCE_DIR}/cmake/FindDsqt.cmake @ONLY)
#export(TARGETS Dsqt Dsqt_resources_1 Dsqt_resources_2 Dsqt_resources_3 Dsqt_resources_4 Dsqtplugin Dsqtplugin_init NAMESPACE Dsqt:: FILE ${CMAKE_SOURCE_DIR}/cmake/DsqtImport.cmake )
set_target_properties(Dsqt PROPERTIES FOLDER DsQt-Framework)
if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(Tests/Settings)
  add_subdirectory(Tests/BridgeTest)
endif()

