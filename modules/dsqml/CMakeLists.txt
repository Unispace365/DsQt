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
    model/dscontentmodel.h model/dscontexthash.h settings/dssettings.h core/dsqmlapplicationengine.h
    model/dscontentmodel.cpp model/dscontexthash.cpp settings/dssettings.cpp core/dsqmlapplicationengine.cpp
    core/dsenvironment.h core/dsenvironment.cpp
    settings/dssettings_qcolor.cpp
    settings/dssettings_base.cpp
    settings/dssettings_collections.cpp
    settings/dssettings_datetime.cpp
    settings/dssettings_geom_qt.cpp
    settings/dssettings_geom_glm.cpp
    settings/dssettings_geom_common.cpp
    settings/dssettings_proxy.h settings/dssettings_proxy.cpp

    QML_FILES Test.qml
)

#link to the libaries that we are using
target_link_libraries(dsqml PUBLIC tomlplusplus::tomlplusplus)
target_include_directories(dsqml PUBLIC tomlplusplus::tomlplusplus settings model core)
target_include_directories(dsqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} glm)
target_link_libraries(dsqml PUBLIC glm Qt6::Gui)
set(DS_LIB_FILES dsqml.qmltypes dsqmlplugin.lib qmldir)
list(TRANSFORM DS_LIB_FILES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ )
message("${DS_LIB_FILES}")
set(DSQML_QML_LIB_FILES ${DS_LIB_FILES} PARENT_SCOPE)
