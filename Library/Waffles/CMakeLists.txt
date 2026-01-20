cmake_minimum_required(VERSION 3.29)

option(DSQT_WITH_TOML "Enable tomlplusplus support" ON)
option(DSQT_WITH_GML "Enable GML math library support" OFF)

set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/Library")

find_package(Qt6 6.9
    REQUIRED COMPONENTS
        # Concurrent
        # Core
        # Gui
        # Qml
        # Quick
        Multimedia
        Pdf
        WebEngineQuick
)
qt_standard_project_setup(REQUIRES 6.9)

qt_add_qml_module(Waffles
    URI Waffles
    STATIC
    VERSION 1.0
    QML_FILES
        qml/BottomOuterControls.qml
        qml/CloseButton.qml
        qml/FullscreenButton.qml
        qml/IconButton.qml
        qml/LeftOuterControls.qml
        qml/RightOuterControls.qml
        qml/TitledMediaViewer.qml
        qml/TopOuterControls.qml
        qml/waffles/DsViewer.qml
        qml/waffles/DsWaffleStage.qml
        qml/waffles/media_viewer/DsControlSet.qml
        qml/waffles/media_viewer/DsTitledMediaViewer.qml
        qml/waffles/menu_launcher/DsContentLauncher.qml
        qml/waffles/menu_launcher/DsSection.qml
        qml/waffles/menu_launcher/DsTestLauncher.qml

    SOURCES

    RESOURCES
)

add_library(DsQt::Waffles ALIAS Waffles)

target_link_libraries(Waffles
    PUBLIC
        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick

        Qt6::Multimedia
        Qt6::Pdf
        Qt6::WebEngineQuick
)

target_include_directories(Waffles
    PUBLIC
        # During build: use source-tree headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qml>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/waffles>
        # After installation: use installed location
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ----------------------------------------------------------------------
# Installation rules
# ----------------------------------------------------------------------

# Install public header files
install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h" 
        PATTERN "*.hpp"
    # Optional: exclude internal/private headers if needed
    # PATTERN "internal/*" EXCLUDE
)

