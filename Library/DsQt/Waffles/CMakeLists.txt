
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

qt_add_qml_module(Waffles
    PLUGIN_TARGET DsqtWafflesplugin
    URI Dsqt.Waffles
    VERSION 1.0
    OUTPUT_TARGETS WafflesTargets
    QML_FILES

        qml/ux/BottomOuterControls.qml
        qml/ux/CloseButton.qml
        qml/ux/FullscreenButton.qml
        qml/ux/IconButton.qml
        qml/ux/LeftOuterControls.qml
        qml/ux/RightOuterControls.qml
        qml/ux/TitledMediaViewer.qml
        qml/ux/TopOuterControls.qml
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
    DsqtWafflesplugin
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


# Prefix the installed library file so it cannot collide with another port in
# vcpkg's shared prefix. The CMake target name and the Dsqt:: alias are
# unchanged, so consumer code is unaffected.
# NOTE: the QML plugin target must NOT use OUTPUT_NAME -- Qt writes the plugin
# target name into the generated qmldir. It is renamed via PLUGIN_TARGET in
# qt_add_qml_module() instead.
set_target_properties(Waffles PROPERTIES OUTPUT_NAME DsqtWaffles)

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