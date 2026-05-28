
cmake_minimum_required(VERSION 3.29)

find_package(Qt6 6.9
    REQUIRED COMPONENTS
        Core
        Gui
        Multimedia
        Qml
        Quick
    OPTIONAL_COMPONENTS
        WebEngineQuick
        Pdf
)
qt_standard_project_setup(REQUIRES 6.9)

add_library(Waffles)
add_library(Dsqt::Waffles ALIAS Waffles)

# Design-token singleton.
set_source_files_properties(qml/waffles/DsTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Waffles
    URI Dsqt.Waffles
    VERSION 1.0
    OUTPUT_TARGETS WafflesTargets
    QML_FILES

        qml/ux/BottomOuterControls.qml
        qml/ux/CloseButton.qml
        qml/ux/DsCtrlIcon.qml
        qml/ux/DsMediaControls.qml
        qml/ux/FullscreenButton.qml
        qml/ux/FullscreenController.qml
        qml/ux/IconButton.qml
        qml/ux/LeftOuterControls.qml
        qml/ux/RightOuterControls.qml
        qml/ux/TitledMediaViewer.qml
        qml/ux/TopOuterControls.qml
        qml/waffles/DsController.qml
        qml/waffles/DsGlassBackground.qml
        qml/waffles/DsTheme.qml
        qml/waffles/DsViewer.qml
        qml/waffles/DsWaffleStage.qml
        qml/waffles/media_viewer/DsControlSet.qml
        qml/waffles/media_viewer/DsTitledMediaViewer.qml
        qml/waffles/menu_launcher/DsContentLauncher.qml
        qml/waffles/menu_launcher/DsSection.qml
        qml/waffles/menu_launcher/DsTestLauncher.qml

        qml/ui/DsVideoViewer.qml
        qml/ui/DsWebViewer.qml
        qml/ui/DsPdfViewer.qml

    RESOURCES
        # Bundled Roboto variable font (SIL OFL 1.1 — see data/fonts/Roboto/OFL.txt). Loaded by
        # DsTheme via a FontLoader so any app using Dsqt.Waffles automatically gets "Roboto"
        # available in QFontDatabase, regardless of whether the app bundles its own fonts.
        data/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf
        data/fonts/Roboto/OFL.txt

        # Bundled media-control icon set. Same rationale as Roboto: every consumer of
        # Dsqt.Waffles gets the standard play/pause/loop/volume/scrub/keyboard/back/forward
        # /reload glyphs without having to ship them per-app. Referenced from DsMediaControls
        # via the qrc:/qt/qml/Dsqt/Waffles/data/... path.
        data/images/waffles/media/arrow_left.svg
        data/images/waffles/media/arrow_right.svg
        data/images/waffles/media/keyboard.svg
        data/images/waffles/media/loop.svg
        data/images/waffles/media/pause.svg
        data/images/waffles/media/play.svg
        data/images/waffles/media/reload.svg
        data/images/waffles/media/volume.svg
)

set(WafflesTargets ${WafflesTargets} PARENT_SCOPE)

target_link_libraries(Waffles PUBLIC
    Qt6::Core 
    Qt6::Quick 
    Qt6::Gui 
    Qt6::Qml 
    Qt6::Multimedia
    $<TARGET_NAME_IF_EXISTS:Qt6::WebEngineQuick>
    $<TARGET_NAME_IF_EXISTS:Qt6::Pdf>
    Wafflesplugin
)

target_include_directories(Waffles
    PUBLIC
        # During build: use source-tree headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        # Subdirectories needed for Qt's metatype system to find headers
        #$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
        
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