
cmake_minimum_required(VERSION 3.29)

option(DSQT_WITH_TOML "Enable tomlplusplus support" ON)
option(DSQT_WITH_GLM "Enable GML math library support" OFF)

set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/Library")
# set(QML_IMPORT_PATH

#      "${QML_ROOT};${QML_ROOT}/qml;${QML_ROOT}/qml/waffles;${QML_ROOT}/qml/bridge;${QML_BIN_ROOT};${QML_BIN_ROOT}/qml;${QML_BIN_ROOT}/qml/waffles;${QML_BIN_ROOT}/qml/bridge"
#      CACHE PATH "QT creator qml import path" FORCE
#  )

find_package(Qt6 6.9
    REQUIRED COMPONENTS 
        Concurrent
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



#find_package(Qt6 REQUIRED COMPONENTS Core Multimedia Quick Pdf WebEngineQuick)
qt_standard_project_setup(REQUIRES 6.9)

add_library(Bridge)

qt_add_qml_module(Bridge
    URI Dsqt.Bridge
    OUTPUT_TARGETS BridgeTargets
    VERSION 1.0
    QML_FILES 
        #currently no qml files

    SOURCES
        bridge/dsQmlBridge.h bridge/dsQmlBridge.cpp
        bridge/dsBridgeDatabase.h
        bridge/dsBridgeQuery.cpp bridge/dsBridgeQuery.h
        bridge/dsBridgeWatcher.cpp bridge/dsBridgeWatcher.h
        bridge/dsQmlPlaylistControl.cpp bridge/dsQmlPlaylistControl.h
        
        model/dsContentModelItemModel.cpp model/dsContentModelItemModel.h
        model/dsQmlEventSchedule.cpp model/dsQmlEventSchedule.h
        model/dsQmlPlaylist.cpp model/dsQmlPlaylist.h
        
)

set(BridgeTargets ${BridgeTargets} PARENT_SCOPE)

target_link_libraries(Bridge 
    PUBLIC 
        
        Qt6::Concurrent
        Qt6::Core 
        Qt6::Qml
        Qt6::Sql

        
        Core 
        Bridgeplugin
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
# ----------------------------------

include(GNUInstallDirs)

install(TARGETS Bridge
  EXPORT DsqtTargets
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
