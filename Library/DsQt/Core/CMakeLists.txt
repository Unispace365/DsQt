cmake_minimum_required(VERSION 3.29)

option(DSQT_WITH_TOML "Enable tomlplusplus support" ON)
option(DSQT_WITH_GLM "Enable GML math library support" OFF)

if(DSQT_WITH_TOML)
#     include(FetchContent)
#     FetchContent_Declare(
#         tomlplusplus
#         GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
#         GIT_TAG v3.4.0
#         GIT_SHALLOW TRUE
#     )
#     FetchContent_MakeAvailable(tomlplusplus)
find_package(tomlplusplus CONFIG REQUIRED)
endif()



if(DSQT_WITH_GLM)
    find_package(glm CONFIG REQUIRED)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/dsVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/core/dsVersion.h
)

find_package(Qt6 6.9
    REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
)

qt_standard_project_setup(REQUIRES 6.9)

add_library(Core)
add_library(Dsqt::Core ALIAS Core)

qt_add_qml_module(Core
    URI Dsqt.Core
    VERSION 1.0
    OUTPUT_TARGETS CoreTargets
    QML_FILES
        #qml/bridge/DsBridgeQueryControls.qml
        qml/DsAppBase.qml
        qml/DsAppMenuBar.qml
        qml/DsAppMenuItem.qml
        qml/DsCrossFadingLoader.qml
        qml/DsFitView.qml
        qml/DsLoader.qml
        qml/DsShortcut.qml
        qml/DsScaleLoader.qml
        qml/DsTextFileViewer.qml
        qml/DsSettingsViewer.qml
        qml/ui/DsAnimateOnOffItem.qml
        qml/ui/DsMediaViewer.qml
        qml/ui/DsPlaylistView.qml
        qml/ui/DsQuickMenu.qml

    SOURCES
        core/dsEnvironment.cpp core/dsEnvironment.h
        core/dsFontManager.h core/dsFontManager.cpp
        core/dsGuiApplication.cpp core/dsGuiApplication.h
        core/dsQmlApplicationEngine.cpp core/dsQmlApplicationEngine.h
        core/dsQmlEditableSource.h
        core/dsQmlEnvironment.h core/dsQmlEnvironment.cpp
        core/dsQmlIdle.h core/dsQmlIdle.cpp
        core/dsQmlIdlePreventer.h core/dsQmlIdlePreventer.cpp
        core/dsQmlObj.h core/dsQmlObj.cpp
        core/dsQmlPathHelper.h
        core/dsQmlTextMeasurer.h
        core/dsReloadUrlInterceptor.h core/dsReloadUrlInterceptor.cpp
        model/dsContentModel.cpp model/dsContentModel.h
        model/dsSettingsTreeModel.cpp model/dsSettingsTreeModel.h
        model/dsQmlTextFileModel.cpp model/dsQmlTextFileModel.h
        model/dsResource.cpp model/dsResource.h
        model/qJsonModel.cpp model/qJsonModel.h
        network/dsNodeWatcher.cpp network/dsNodeWatcher.h
        settings/dsOverloaded.h
        settings/dsQmlSettingsProxy.cpp settings/dsQmlSettingsProxy.h
        settings/dsSettings.cpp settings/dsSettings.h
        settings/dsSettingsBase.cpp
        settings/dsSettingsCollections.cpp
        settings/dsSettingsDateTime.cpp
        settings/dsSettingsGeomCommon.cpp
        settings/dsSettingsGeomGlm.cpp
        settings/dsSettingsGeomQt.cpp
        settings/dsSettingsQColor.cpp
        ui/dsQmlClock.h ui/dsQmlClock.cpp
        ui/dsQmlClusterManager.h ui/dsQmlClusterManager.cpp
        ui/dsQmlClusterView.h ui/dsQmlClusterView.cpp
        ui/dsQmlFpsMonitor.h ui/dsQmlFpsMonitor.cpp
        ui/dsQmlShortcutManager.h ui/dsQmlShortcutManager.cpp
        ui/dsQmlTouchCluster.h ui/dsQmlTouchCluster.cpp
        utility/dsFileMetaData.cpp utility/dsFileMetaData.h
        utility/dsStringUtils.cpp utility/dsStringUtils.h
        utility/dsUrlImageLoader.cpp utility/dsUrlImageLoader.h
        utility/WindowHelper.h

    #DEPENDENCIES
    #     QtQuick
    #     Dsqtplugin
    
    RESOURCES
        res/keyboard.qrc
        res/data.qrc

        QML_FILES 
)

set(CoreTargets ${CoreTargets} PARENT_SCOPE)

target_link_libraries(Core PUBLIC 
    Qt6::Core 
    Qt6::Quick 
    Qt6::Gui 
    Qt6::Qml 
    Coreplugin
    $<$<PLATFORM_ID:Windows>:user32> # For WindowHelper
)

target_include_directories(Core
    PUBLIC
        # During build: use source-tree headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        # Generated headers (dsVersion.h)
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        # Subdirectories needed for Qt's metatype system to find headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/network>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/settings>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ui>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utility>
        # After installation: use installed location
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(DSQT_WITH_TOML)
    # tomlplusplus::tomlplusplus (from find_package(tomlplusplus CONFIG)) already
    # propagates its own include directories and compile definitions via its
    # INTERFACE target — we don't need to add them manually here. The old
    # `${tomlplusplus_SOURCE_DIR}` reference only worked with FetchContent and
    # silently became a no-op after switching to find_package, which meant
    # downstream consumers lost the toml++ wiring entirely.
    target_link_libraries(Core PUBLIC tomlplusplus::tomlplusplus)
    target_compile_definitions(Core PUBLIC DSQT_USE_TOML)
endif()

if(DSQT_WITH_GLM)
    target_link_libraries(Core PUBLIC glm::glm)
    target_compile_definitions(Core PUBLIC DSQT_USE_GLM GLM_ENABLE_EXPERIMENTAL)
endif()

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

# NOTE: We intentionally do NOT install toml++ headers into the Dsqt install
# prefix. With find_package(tomlplusplus CONFIG), tomlplusplus_SOURCE_DIR is
# empty, so the old install() block was silently doing nothing anyway.
# Downstream consumers are instead expected to locate tomlplusplus themselves,
# which DsqtConfig.cmake handles via find_dependency(tomlplusplus). This keeps
# the build and install sides of the toml++ ABI consistent (critical: if the
# vcpkg port is dynamic, its headers mark symbols __declspec(dllimport), and
# both Dsqt and the consumer must see the same config to avoid LNK4217/LNK2019).
