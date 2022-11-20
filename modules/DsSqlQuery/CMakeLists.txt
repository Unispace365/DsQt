cmake_minimum_required(VERSION 3.16)
include("$ENV{DS_QT_PLATFORM_100}/cmake/dsqt.cmake")
#project(DsSqlQuery VERSION 0.1 LANGUAGES CXX)

#set(CMAKE_AUTOMOC ON)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(QT_QML_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR})

##find_package(tomlplusplus)
add_library(DsSqlQuery STATIC)


qt_add_qml_module(DsSqlQuery
    URI DsSqlQuery
    VERSION 1.0
    QML_FILES DsSqlQueryControls.qml
    SOURCES dssqlquery.cpp dssqlquery.h
    IMPORTS dsqml
)


target_compile_definitions(DsSqlQuery
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(DsSqlQuery
    PRIVATE Qt6::Quick dsqml)
target_include_directories(DsSqlQuery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
