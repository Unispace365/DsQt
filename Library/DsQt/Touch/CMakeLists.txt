cmake_minimum_required(VERSION 3.29)

# Suppress CMake's "you are using private Qt headers" warning — we know.
set(QT_NO_PRIVATE_MODULE_WARNING ON)

# When ON, re-inject confirmed touch events via QWindowSystemInterface rather
# than QCoreApplication::sendEvent.  This feeds events through Qt's full input
# pipeline so that TapHandler, DragHandler and PinchHandler work correctly with
# transient-filtered touches.  Also enables event-level lift-resume bridging via
# QMutableEventPoint::setId().  Requires Qt private headers (Qt6::GuiPrivate).
option(DSQT_TOUCH_PRIVATE_REINJECT
    "Use Qt private headers for proper touch re-injection (enables TapHandler compatibility)"
    OFF)

if(DSQT_TOUCH_PRIVATE_REINJECT)
    find_package(Qt6 6.9 REQUIRED COMPONENTS GuiPrivate)
endif()

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

if(DSQT_TOUCH_PRIVATE_REINJECT)
    target_link_libraries(Touch PUBLIC Qt6::GuiPrivate)
    target_compile_definitions(Touch PUBLIC DSQT_TOUCH_PRIVATE_REINJECT)
endif()

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
