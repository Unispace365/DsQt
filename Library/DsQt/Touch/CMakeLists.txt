cmake_minimum_required(VERSION 3.29)

# Suppress CMake's "you are using private Qt headers" warning — we know.
set(QT_NO_PRIVATE_MODULE_WARNING ON)

# By default, confirmed touch events are re-injected via QWindowSystemInterface,
# which feeds events through Qt's full input pipeline so TapHandler, DragHandler,
# and PinchHandler work correctly with transient-filtered touches.  Also enables
# event-level lift-resume bridging via QMutableEventPoint::setId().
# Requires Qt private headers (Qt6::GuiPrivate).
# Set DSQT_TOUCH_NO_PRIVATE_REINJECT=ON to fall back to QCoreApplication::sendEvent.
option(DSQT_TOUCH_NO_PRIVATE_REINJECT
    "Disable Qt private-header touch re-injection (fallback to QCoreApplication::sendEvent)"
    OFF)

if(NOT DSQT_TOUCH_NO_PRIVATE_REINJECT)
    find_package(Qt6 6.9 REQUIRED COMPONENTS GuiPrivate)
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.9)

add_library(Touch)
add_library(Dsqt::Touch ALIAS Touch)

qt_add_qml_module(Touch
    PLUGIN_TARGET DsqtTouchplugin
    URI Dsqt.Touch
    VERSION 1.0
    OUTPUT_TARGETS TouchTargets
    SOURCES
        touch/TouchFilter.h
        touch/TouchFilter.cpp
    QML_FILES
        qml/TouchFilterDebugOverlay.qml
        qml/TouchFilterControlsWindow.qml
)

set(TouchTargets ${TouchTargets} PARENT_SCOPE)

target_link_libraries(Touch PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Core
    DsqtTouchplugin
)

if(NOT DSQT_TOUCH_NO_PRIVATE_REINJECT)
    target_link_libraries(Touch PUBLIC Qt6::GuiPrivate)
else()
    target_compile_definitions(Touch PUBLIC DSQT_TOUCH_NO_PRIVATE_REINJECT)
endif()

target_include_directories(Touch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/touch>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


# Prefix the installed library file so it cannot collide with another port in
# vcpkg's shared prefix. The CMake target name and the Dsqt:: alias are
# unchanged, so consumer code is unaffected.
# NOTE: the QML plugin target must NOT use OUTPUT_NAME -- Qt writes the plugin
# target name into the generated qmldir. It is renamed via PLUGIN_TARGET in
# qt_add_qml_module() instead.
set_target_properties(Touch PROPERTIES OUTPUT_NAME DsqtTouch)

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
