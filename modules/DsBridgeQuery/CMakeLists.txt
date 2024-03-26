cmake_minimum_required(VERSION 3.16)
include("$ENV{DS_QT_PLATFORM_100}/cmake/dsqt.cmake")
#project(DsBridgeQuery VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR})

##find_package(tomlplusplus)
find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Core Sql)


qt_add_qml_module(DsBridgeQuery
    URI DsBridgeQuery
    STATIC
    VERSION 1.0
    QML_FILES DsBridgeQueryControls.qml
    SOURCES dsBridgeQuery.cpp dsBridgeQuery.h
    SOURCES bridge_utility.h bridge_utility.cpp
    IMPORTS dsqt


)
add_dependencies(DsBridgeQuery dsqt)
qt_import_qml_plugins(DsBridgeQuery)
target_compile_definitions(DsBridgeQuery
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(DsBridgeQuery
    PRIVATE Qt6::Core Qt6::Quick Qt6::Sql dsqt)
target_include_directories(DsBridgeQuery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} dsqt )
