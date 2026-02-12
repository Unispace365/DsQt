
cmake_minimum_required(VERSION 3.29)

find_package(Qt6 6.9
    REQUIRED COMPONENTS 
        Concurrent
        Core
        Qml
        Sql
)

if (NOT TARGET Qt6::Sql)
  message(STATUS "Bridge disabled: Qt Sql not available in this Qt build")
  return()
endif()

qt_standard_project_setup(REQUIRES 6.9)

add_library(Bridge)
add_library(Dsqt::Bridge ALIAS Bridge)

qt_add_qml_module(Bridge
    URI Dsqt.Bridge
    OUTPUT_TARGETS BridgeTargets
    VERSION 1.0
    QML_FILES
        qml/DsContentBrowser.qml

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
# Installation rules (headers only - target install is in main CMakeLists.txt)
# ----------------------------------

include(GNUInstallDirs)

# Install public header files
install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h" 
        PATTERN "*.hpp"
    # Optional: exclude internal/private headers if needed
    # PATTERN "internal/*" EXCLUDE
)
