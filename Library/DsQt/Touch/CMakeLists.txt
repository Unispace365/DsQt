cmake_minimum_required(VERSION 3.29)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Quick)
qt_standard_project_setup(REQUIRES 6.9)

add_library(Touch)
add_library(Dsqt::Touch ALIAS Touch)

qt_add_qml_module(Touch
    URI Dsqt.Touch
    VERSION 1.0
    OUTPUT_TARGETS TouchTargets
    SOURCES
        touch/TouchFilter.h
        touch/TouchFilter.cpp
)

set(TouchTargets ${TouchTargets} PARENT_SCOPE)

target_link_libraries(Touch PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Touchplugin
)

target_include_directories(Touch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/touch>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ----------------------------------------------------------------------
# Installation rules (headers only — target install is in main CMakeLists.txt)
# ----------------------------------------------------------------------

include(GNUInstallDirs)

install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)
