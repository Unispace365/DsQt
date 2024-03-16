## find the tomlplusplus package
## this needs to be in the projects conanfile.txt
## find_package(tomlplusplus)

find_package(Qt6 REQUIRED COMPONENTS Gui Core Quick ShaderTools)

#create the modules
## this adds the qml and c++ files to the module
qt_add_qml_module(dsqml
    STATIC
    URI dsqml
    VERSION 1.0
    SOURCES
    model/dscontentmodel.h model/dscontentmodel.cpp
    model/content_model.h model/content_model.cpp
    model/dscontexthash.h model/dscontexthash.cpp
    model/dsresource.h model/dsresource.cpp
    model/dscontentitemmodel.h model/dscontentitemmodel.cpp
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
    QML_FILES Test.qml qml/DSAppBase.qml
    SOURCES core/dsimgui_item.h
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

target_link_libraries(dsqml PUBLIC tomlplusplus::tomlplusplus Qt6::GuiPrivate Qt::QuickPrivate)
target_include_directories(dsqml PUBLIC tomlplusplus::tomlplusplus settings model core network Qt6::Gui Qt6::GuiPrivate Qt::QuickPrivate ${imgui_base})
target_include_directories(dsqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} glm)
target_link_libraries(dsqml PUBLIC glm Qt6::Gui)
set(DS_LIB_FILES dsqml.qmltypes dsqmlplugin.lib qmldir)
list(TRANSFORM DS_LIB_FILES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ )
message("${DS_LIB_FILES}")
set(DSQML_QML_LIB_FILES ${DS_LIB_FILES} PARENT_SCOPE)
