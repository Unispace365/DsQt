cmake_minimum_required(VERSION 3.29)

find_package(Qt6 6.12
    REQUIRED COMPONENTS
        Core
        Gui
        Multimedia
        Qml
        Quick
        CanvasPainter
)

# WebEngine and Pdf can have different versions from the base Qt SDK.
# Record the built feature set for installed consumers, which must link the same modules.
foreach(_feature WebEngineQuick PdfQuick VirtualKeyboard)
    find_package(Qt6${_feature} 6.9 QUIET HINTS "${Qt6_DIR}/../Qt6${_feature}")
    if(TARGET Qt6::${_feature})
        set(DSQT_WAFFLES_HAS_${_feature} TRUE PARENT_SCOPE)
    else()
        set(DSQT_WAFFLES_HAS_${_feature} FALSE PARENT_SCOPE)
    endif()
endforeach()
qt_standard_project_setup(REQUIRES 6.12)

add_library(Waffles)
add_library(Dsqt::Waffles ALIAS Waffles)

# Design-token singleton.
set_source_files_properties(qml/waffles/DsTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Waffles
    PLUGIN_TARGET DsqtWafflesplugin
    URI Dsqt.Waffles
    VERSION 1.0
    OUTPUT_TARGETS WafflesTargets
    DEPENDENCIES TARGET Core
    SOURCES
        capture/dsCaptureSource.h
        capture/dsCaptureSource.cpp
        capture/dsCaptureSession.h
        capture/dsCaptureSession.cpp
        whiteboard/dsWhiteboardCanvas.h
        whiteboard/dsWhiteboardCanvas.cpp
    QML_FILES
        qml/waffles/DsWaffleQuickMenu.qml
        qml/waffles/DsQuickMenuActions.qml

        qml/ux/BottomOuterControls.qml
        qml/ux/CloseButton.qml
        qml/ux/DsCtrlIcon.qml
        qml/ux/DsFloatingKeyboard.qml
        qml/ux/DsMediaControls.qml
        qml/ux/DsVirtualKeyboard.qml
        qml/ux/FullscreenButton.qml
        qml/ux/FullscreenController.qml
        qml/ux/IconButton.qml
        qml/ux/LeftOuterControls.qml
        qml/ux/RightOuterControls.qml
        qml/ux/TitledMediaViewer.qml
        qml/ux/Whiteboard.qml
        qml/ux/WhiteboardControls.qml
        qml/waffles/whiteboard/DsWhiteboard.qml
        qml/ux/TopOuterControls.qml
        qml/waffles/DsController.qml
        qml/waffles/DsContentState.qml
        qml/waffles/DsGlassBackground.qml
        qml/waffles/DsTheme.qml
        qml/waffles/DsViewer.qml
        qml/waffles/DsWaffleStage.qml
        qml/waffles/media_viewer/DsControlSet.qml
        qml/waffles/media_viewer/DsTitledMediaViewer.qml
        qml/waffles/playlist/DsMediaTemplate.qml
        qml/waffles/playlist/DsCustomLayoutTemplate.qml
        qml/waffles/playlist/DsCustomLayoutMedia.qml
        qml/waffles/playlist/DsPlaylistViewer.qml
        qml/waffles/playlist/PresentationController.qml
        qml/waffles/menu_launcher/DsContentLauncher.qml
        qml/waffles/menu_launcher/DsSection.qml
        qml/waffles/menu_launcher/DsTestLauncher.qml

        qml/ui/DsVideoViewer.qml
        qml/ui/DsCaptureViewer.qml
        qml/ui/DsWebViewer.qml
        qml/ui/DsPdfViewer.qml

    RESOURCES
        data/images/waffles/quick_menu/content.svg
        data/images/waffles/quick_menu/content_disabled.svg
        data/images/waffles/quick_menu/whiteboard.svg
        data/images/waffles/quick_menu/forward.svg
        data/images/waffles/quick_menu/forward_disabled.svg
        data/images/waffles/quick_menu/arrange.svg
        data/images/waffles/quick_menu/arrange_disabled.svg
        data/images/waffles/quick_menu/close_assets.svg
        data/images/waffles/quick_menu/close_assets_disabled.svg
        data/images/waffles/quick_menu/gather.svg
        data/images/waffles/quick_menu/gather_disabled.svg
        data/images/waffles/quick_menu/back.svg
        data/images/waffles/quick_menu/back_disabled.svg
        data/images/waffles/quick_menu/asset_browsing.svg
        data/images/waffles/quick_menu/asset_browsing_disabled.svg
        data/images/waffles/quick_menu/ambient.svg
        data/images/waffles/quick_menu/ambient_disabled.svg
        data/images/waffles/missing_content.svg
        # Window controls are bundled for every consuming application.
        data/images/waffles/window/close.svg
        data/images/waffles/window/close_pressed.svg
        data/images/waffles/window/collapse.svg
        data/images/waffles/window/collapse_pressed.svg
        data/images/waffles/window/controller_collapse.svg
        data/images/waffles/window/controller_collapse_pressed.svg
        data/images/waffles/window/controller_expand.svg
        data/images/waffles/window/controller_expand_pressed.svg
        data/images/waffles/window/fullscreen.svg
        data/images/waffles/window/fullscreen_pressed.svg
        data/images/waffles/window/lock.svg
        data/images/waffles/window/lock_pressed.svg
        data/images/waffles/window/locked.svg
        data/images/waffles/window/locked_pressed.svg

        data/images/waffles/whiteboard/pen.svg
        data/images/waffles/whiteboard/highlighter.svg
        data/images/waffles/whiteboard/eraser.svg
        data/images/waffles/whiteboard/background.svg
        data/images/waffles/whiteboard/undo.svg
        data/images/waffles/whiteboard/redo.svg
        data/images/waffles/whiteboard/save.svg
        data/images/waffles/whiteboard/close.svg

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
    Dsqt::Core
    Qt6::Core
    Qt6::Quick
    Qt6::Gui
    Qt6::Qml
    Qt6::Multimedia
    Qt6::CanvasPainter
    $<TARGET_NAME_IF_EXISTS:Qt6::WebEngineQuick>
    $<TARGET_NAME_IF_EXISTS:Qt6::PdfQuick>
    $<TARGET_NAME_IF_EXISTS:Qt6::VirtualKeyboard>
    DsqtWafflesplugin
)

target_include_directories(Waffles
    PUBLIC
        # During build: use source-tree headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        # Subdirectories needed for Qt's metatype system to find headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/whiteboard>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/capture>
        # After installation: use installed location
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Prefix the installed library file so it cannot collide with another port in
# vcpkg's shared prefix. The CMake target name and the Dsqt:: alias are
# unchanged, so consumer code is unaffected.
# NOTE: the QML plugin target must NOT use OUTPUT_NAME -- Qt writes the plugin
# target name into the generated qmldir. It is renamed via PLUGIN_TARGET in
# qt_add_qml_module() instead.
set_target_properties(Waffles PROPERTIES OUTPUT_NAME DsqtWaffles)

# ----------------------------------------------------------------------
# Installation rules (headers only - target install is in main CMakeLists.txt)
# ----------------------------------------------------------------------

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
