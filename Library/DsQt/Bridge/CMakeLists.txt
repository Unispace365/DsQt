cmake_minimum_required(VERSION 3.29)

set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/Library")

find_package(Qt6 6.9
    REQUIRED COMPONENTS 
        # Concurrent
        # Core
        # Gui
        # Qml
        # Quick
        Sql
)

if (NOT TARGET Qt6::Sql)
  message(STATUS "Bridge disabled: Qt Sql not available in this Qt build")
  return()
endif()

qt_standard_project_setup(REQUIRES 6.9)

add_library(Bridge)
# add_library(DsQt::Bridge ALIAS Bridge)

qt_add_qml_module(Bridge
    URI DsQt.Bridge
    VERSION 1.0
    QML_FILES 
        qml/bridge/DsBridgeQueryControls.qml

    SOURCES
        bridge/dsBridge.h
        bridge/dsBridge.cpp
        bridge/dsBridgeDatabase.h
        bridge/dsBridgeQuery.cpp 
        bridge/dsBridgeQuery.h
        bridge/dsBridgeWatcher.cpp 
        bridge/dsBridgeWatcher.h
        bridge/dsQmlPlaylistControl.cpp
        bridge/dsQmlPlaylistControl.h
        model/dsContentModel.cpp
        model/dsContentModel.h
        model/dsContentModelItemModel.cpp
        model/dsContentModelItemModel.h
        model/dsQmlEventSchedule.cpp
        model/dsQmlEventSchedule.h
        model/dsQmlPlaylist.cpp
        model/dsQmlPlaylist.h
        model/dsQmlTextFileModel.cpp
        model/dsQmlTextFileModel.h
        model/qJsonModel.cpp
        model/qJsonModel.h

    RESOURCES
)

add_library(DsQt::Bridge ALIAS Bridge)

target_link_libraries(Bridge
    PUBLIC
        Core

        Qt6::Core
        Qt6::Qml
        Qt6::Sql
)

target_include_directories(Bridge
    PUBLIC
        # During build: use source-tree headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        # Subdirectories needed for Qt's metatype system to find headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bridge>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model>
        # After installation: use installed location
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ----------------------------------------------------------------------
# Installation rules
# ----------------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS Bridge
  EXPORT DsQtTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install public header files
install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h" 
        PATTERN "*.hpp"
    # Optional: exclude internal/private headers if needed
    # PATTERN "internal/*" EXCLUDE
)

