cmake_minimum_required(VERSION 3.29)

option(DSQT_WITH_TOML "Enable tomlplusplus support" ON)
option(DSQT_WITH_GLM "Enable GML math library support" OFF)

set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/Library")

if(DSQT_WITH_TOML)
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.2.0
    )
    FetchContent_GetProperties(tomlplusplus)
    FetchContent_MakeAvailable(tomlplusplus)
endif()

if(DSQT_WITH_GLM)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        0.9.9.8
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
    )
    FetchContent_GetProperties(glm)
    FetchContent_MakeAvailable(glm)
endif()

find_package(Qt6 6.9
    REQUIRED COMPONENTS
        # Concurrent
        # Core
        # Gui
        # Qml
        # Quick
)
qt_standard_project_setup(REQUIRES 6.9)

add_library(Core)
# add_library(DsQt::Core ALIAS Core)

qt_add_qml_module(Core
    URI DsQt.Core
    VERSION 1.0
    QML_FILES 
        qml/DsAppBase.qml
        qml/DsAppMenuBar.qml
        qml/DsAppMenuItem.qml
        qml/DsContentBrowser.qml
        qml/DsCrossFadingLoader.qml
        qml/DsFitView.qml
        qml/DsLoader.qml
        qml/DsScaleLoader.qml
        qml/DsTextFileViewer.qml
        qml/ui/DsAnimateOnOffItem.qml
        qml/ui/DsMediaViewer.qml
        qml/ui/DsPlaylistView.qml
        qml/ui/DsQuickMenu.qml

    SOURCES
        core/dsEnvironment.cpp
        core/dsEnvironment.h 
        core/dsGuiApplication.cpp
        core/dsGuiApplication.h
        core/dsQmlApplicationEngine.cpp
        core/dsQmlApplicationEngine.h 
        core/dsQmlEditableSource.h
        core/dsQmlEnvironment.cpp
        core/dsQmlEnvironment.h
        core/dsQmlIdle.cpp
        core/dsQmlIdle.h
        core/dsQmlIdlePreventer.cpp
        core/dsQmlIdlePreventer.h 
        core/dsResource.cpp
        core/dsResource.h
        core/dsQmlObj.cpp
        core/dsQmlObj.h 
        core/dsQmlPathHelper.h
        core/dsQmlTextMeasurer.h
        core/dsReloadUrlInterceptor.cpp
        core/dsReloadUrlInterceptor.h
        network/dsNodeWatcher.cpp
        network/dsNodeWatcher.h
        settings/dsOverloaded.h
        settings/dsQmlSettingsProxy.cpp 
        settings/dsQmlSettingsProxy.h
        settings/dsSettings.cpp
        settings/dsSettings.h
        settings/dsSettingsBase.cpp
        settings/dsSettingsCollections.cpp
        settings/dsSettingsDateTime.cpp
        settings/dsSettingsGeomCommon.cpp
        settings/dsSettingsGeomGlm.cpp
        settings/dsSettingsGeomQt.cpp
        settings/dsSettingsQColor.cpp
        ui/dsQmlClock.cpp
        ui/dsQmlClock.h
        ui/dsQmlClusterManager.cpp
        ui/dsQmlClusterManager.h 
        ui/dsQmlClusterView.cpp
        ui/dsQmlClusterView.h
        ui/dsQmlFpsMonitor.cpp
        ui/dsQmlFpsMonitor.h
        ui/dsQmlTouchCluster.cpp
        ui/dsQmlTouchCluster.h
        utility/dsFileMetaData.cpp
        utility/dsFileMetaData.h
        utility/dsStringUtils.cpp
        utility/dsStringUtils.h
        utility/dsUrlImageLoader.cpp
        utility/dsUrlImageLoader.h
        
        # core/dsQmlImguiItem.h

    RESOURCES
        res/keyboard.qrc
        res/data.qrc
)

target_link_libraries(Core
    PUBLIC
        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

target_include_directories(Core
    PUBLIC
        # During build: use source-tree headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        # Subdirectories needed for Qt's metatype system to find headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ui>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/settings>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/network>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utility>
        # After installation: use installed location
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(DSQT_WITH_TOML)
    target_link_libraries(Core PUBLIC tomlplusplus::tomlplusplus)
    target_compile_definitions(Core PUBLIC DSQT_USE_TOML)
endif()

if(DSQT_WITH_GLM)
    target_include_directories(Core PUBLIC ${glm_SOURCE_DIR})
    target_compile_definitions(Core PUBLIC DSQT_USE_GLM)
endif()

# ----------------------------------------------------------------------
# Installation rules
# ----------------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS Core
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
