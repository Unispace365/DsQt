cmake_minimum_required(VERSION 3.16)
## find the tomlplusplus package
## this needs to be in the projects conanfile.txt
## find_package(tomlplusplus)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Core Gui ShaderTools)

#tell cmake what to call the javascript files in the qml module
set_source_files_properties(qml/js/moment-with-locals.js PROPERTIES  QT_QML_SOURCE_TYPENAME "Moment")


#create the modules
## this adds the qml and c++ files to the module
qt_add_qml_module(dsqt
    STATIC
    URI dsqt
    VERSION 1.0
    SOURCES

    model/content_model.h model/content_model.cpp
    model/resource.h model/resource.cpp
    settings/dssettings.h settings/dssettings.cpp
    core/dsqmlapplicationengine.h core/dsqmlapplicationengine.cpp
    core/dsenvironment.h core/dsenvironment.cpp
    core/dsguiapplication.h core/dsguiapplication.cpp
    settings/dssettings_qcolor.cpp
    settings/dssettings_base.cpp
    settings/dssettings_collections.cpp
    settings/dssettings_datetime.cpp
    settings/dssettings_geom_qt.cpp
    settings/dssettings_geom_glm.cpp
    settings/dssettings_geom_common.cpp
    settings/dssettings_proxy.h settings/dssettings_proxy.cpp
    network/dsnodewatcher.h network/dsnodewatcher.cpp
    utility/string_util.h utility/string_util.cpp
    utility/file_meta_data.h utility/file_meta_data.cpp
    utility/url_image_loader.h utility/url_image_loader.cpp
    model/qjsonmodel.cpp model/qjsonmodel.h
    core/dsimgui_item.h
    QML_FILES
    Test.qml qml/DSAppBase.qml
    qml/js/moment-with-locals.js
    qml/DSTransition.qml
)


#docs
include("$ENV{DS_QT_PLATFORM_100}/cmake/docs.cmake")
set(DOCS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/DsQt")
downstream_docs(IN_DIR ${DOCS_DIR} OUT_DIR ${DOCS_DIR} DOCNAME "DsQt" DESC "Qt for Experience Projects")


#link to the libaries that we are using
#message("****************************************************** ${qtimgui_SOURCE_DIR}")
set(imgui_base ${qtimgui_SOURCE_DIR}/imgui)
#set(imgui_target dsqml)
#include(${imgui_base}/imgui.cmakeinc)
#include(${imgui_base}/imguiquick.cmakeinc)

target_link_libraries(dsqt PUBLIC tomlplusplus::tomlplusplus  Qt6::GuiPrivate Qt::QuickPrivate)
target_include_directories(dsqt PUBLIC tomlplusplus::tomlplusplus settings model core network Qt::Quick Qt6::Gui Qt6::GuiPrivate Qt::QuickPrivate ${imgui_base})
target_include_directories(dsqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} glm)
target_link_libraries(dsqt PUBLIC glm Qt6::Gui)
set(DS_LIB_FILES dsqt.qmltypes dsqtplugin.lib qmldir)
list(TRANSFORM DS_LIB_FILES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ )
message("${DS_LIB_FILES}")
set(DSQT_QML_LIB_FILES ${DS_LIB_FILES} PARENT_SCOPE)
