
cmake_minimum_required(VERSION 3.29)



set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/Library")
# set(QML_IMPORT_PATH

#      "${QML_ROOT};${QML_ROOT}/qml;${QML_ROOT}/qml/waffles;${QML_ROOT}/qml/bridge;${QML_BIN_ROOT};${QML_BIN_ROOT}/qml;${QML_BIN_ROOT}/qml/waffles;${QML_BIN_ROOT}/qml/bridge"
#      CACHE PATH "QT creator qml import path" FORCE
#  )

#find_package(Qt6 REQUIRED COMPONENTS Core Multimedia Quick Pdf WebEngineQuick)
find_package(Qt6 6.9
    REQUIRED COMPONENTS
        # Concurrent
        # Core
        # Gui
        # Qml
        # Quick
)
qt_standard_project_setup(REQUIRES 6.9)

add_library(Waffles)

qt_add_qml_module(Waffles
    URI DsQt.Waffles
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

)

set(WafflesTargets ${WafflesTargets} PARENT_SCOPE)

target_link_libraries(Waffles PUBLIC Qt6::Core Qt6::Quick Qt6::Gui Qt6::Qml Wafflesplugin)
#target_include_directories(Waffles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} bridge core model network qml resources settings ui utility)

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
# Installation rules
# ----------------------------------

include(GNUInstallDirs)

install(TARGETS Waffles
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