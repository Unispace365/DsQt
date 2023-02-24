cmake_minimum_required(VERSION 3.16)
include("$ENV{DS_QT_PLATFORM_100}/cmake/dsqt.cmake")
#project(DsSqlQuery VERSION 0.1 LANGUAGES CXX)

#set(CMAKE_AUTOMOC ON)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(QT_QML_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR})

##find_package(tomlplusplus)
find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Core Sql)


qt_add_qml_module(DsSqlQuery
    URI DsSqlQuery
    STATIC
    VERSION 1.0
    QML_FILES DsSqlQueryControls.qml
    SOURCES dssqlquery.cpp dssqlquery.h nw_sql_queries.h
    IMPORTS dsqml
)

target_compile_definitions(DsSqlQuery
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(DsSqlQuery
    PRIVATE Qt6::Core Qt6::Quick Qt6::Sql dsqml)
target_include_directories(DsSqlQuery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} dsqml )
