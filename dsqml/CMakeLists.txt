set (INCLUDES_LIST dscontentmodel.h dscontexthash.h dssettings.h)
set (CPP_LIST dscontentmodel.cpp dscontexthash.cpp dssettings.cpp)
#set (DSQML_LOCAL_INCLUDES ${INCLUDES_LIST})
#list (TRANSFORM DSQML_LOCAL_INCLUDES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ )

#add_library(dsqml STATIC)
find_package(tomlplusplus)
qt_add_qml_module(dsqml
    STATIC
    URI dsqml
    VERSION 1.0
    SOURCES
    ${INCLUDES_LIST}
    ${CPP_LIST}
    QML_FILES Test.qml
)

target_link_libraries(dsqml PRIVATE tomlplusplus::tomlplusplus)
target_include_directories(dsqml PRIVATE tomlplusplus::tomlplusplus)
target_include_directories(dsqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set(DS_LIB_FILES dsqml.qmltypes dsqmlplugin.lib qmldir)
list(TRANSFORM DS_LIB_FILES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ )
message("${DS_LIB_FILES}")
set(DSQML_QML_LIB_FILES ${DS_LIB_FILES} PARENT_SCOPE)
