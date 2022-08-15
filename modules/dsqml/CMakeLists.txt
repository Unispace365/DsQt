set (INCLUDES_LIST )
set (CPP_LIST )
#set (DSQML_LOCAL_INCLUDES ${INCLUDES_LIST})
#list (TRANSFORM DSQML_LOCAL_INCLUDES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ )

#add_library(dsqml STATIC)
find_package(tomlplusplus)
qt_add_qml_module(dsqml
    STATIC
    URI dsqml
    VERSION 1.0
    SOURCES
    dscontentmodel.h dscontexthash.h dssettings.h dsqmlapplicationengine.h
    dscontentmodel.cpp dscontexthash.cpp dssettings.cpp dsqmlapplicationengine.cpp
    QML_FILES Test.qml
)

target_link_libraries(dsqml PRIVATE tomlplusplus::tomlplusplus)
target_include_directories(dsqml PUBLIC tomlplusplus::tomlplusplus)
target_include_directories(dsqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set(DS_LIB_FILES dsqml.qmltypes dsqmlplugin.lib qmldir)
list(TRANSFORM DS_LIB_FILES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ )
message("${DS_LIB_FILES}")
set(DSQML_QML_LIB_FILES ${DS_LIB_FILES} PARENT_SCOPE)
