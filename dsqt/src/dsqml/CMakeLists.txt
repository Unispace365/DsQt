
set (DSQML_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/dscontentmodel.h
    PARENT_SCOPE
)
add_library(dsqml STATIC)

qt6_add_qml_module(dsqml
    URI dsqml
    VERSION 1.0
    #OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../${CMAKE_BUILD_TYPE}/lib/"
    SOURCES
        dscontentmodel.cpp
        dscontentmodel.h
    QML_FILES
        test.qml

)

set(DS_LIB_FILES dsqml.qmltypes dsqmlplugin$<$<CONFIG:Debug>:d>.lib qmldir)
list(TRANSFORM DS_LIB_FILES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ )
message("${DS_LIB_FILES}")
set(DSQML_QML_LIB_FILES ${DS_LIB_FILES} PARENT_SCOPE)
