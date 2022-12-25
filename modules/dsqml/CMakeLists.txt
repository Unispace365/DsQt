## find the tomlplusplus package
## this needs to be in the projects conanfile.txt
## find_package(tomlplusplus)

find_package(Qt6 REQUIRED COMPONENTS Gui)

#create the modules
## this adds the qml and c++ files to the module
qt_add_qml_module(dsqml
    STATIC
    URI dsqml
    VERSION 1.0
    SOURCES
    dscontentmodel.h dscontexthash.h settings/settings.h dsqmlapplicationengine.h
    dscontentmodel.cpp dscontexthash.cpp settings/settings.cpp dsqmlapplicationengine.cpp
    dsenvironment.h dsenvironment.cpp
    settings/settings_qcolor.cpp
    settings/settings_base.cpp
    settings/settings_collections.cpp
    settings/settings_datetime.cpp
    settings/settings_geom_qt.cpp
    settings/settings_geom_glm.cpp
    settings/settings_geom_common.cpp
    settings/setting_proxy.h settings/setting_proxy.cpp
    overload.h
    QML_FILES Test.qml
)

#link to the libaries that we are using
target_link_libraries(dsqml PUBLIC tomlplusplus::tomlplusplus)
target_include_directories(dsqml PUBLIC tomlplusplus::tomlplusplus settings)
target_include_directories(dsqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} glm)
target_link_libraries(dsqml PUBLIC glm Qt6::Gui)
set(DS_LIB_FILES dsqml.qmltypes dsqmlplugin.lib qmldir)
list(TRANSFORM DS_LIB_FILES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ )
message("${DS_LIB_FILES}")
set(DSQML_QML_LIB_FILES ${DS_LIB_FILES} PARENT_SCOPE)
