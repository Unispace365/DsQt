set(QML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/Library")
set(QML_IMPORT_PATH

     "${QML_ROOT};${QML_ROOT}/qml;${QML_ROOT}/qml/waffles;${QML_ROOT}/qml/bridge;${QML_BIN_ROOT};${QML_BIN_ROOT}/qml;${QML_BIN_ROOT}/qml/waffles;${QML_BIN_ROOT}/qml/bridge"
     CACHE PATH "QT creator qml import path" FORCE
 )

find_package(Qt6 REQUIRED COMPONENTS Core Multimedia Quick Pdf WebEngineQuick)

qt_add_qml_module(WafflesUx
    URI WafflesUx
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
    SOURCES
        
        
    DEPENDENCIES
        Dsqt
        WafflesUxPlugin
        QtQuick

    RESOURCES


)

target_link_libraries(WafflesUx PRIVATE Qt6::Core Qt6::Multimedia Qt6::Quick Qt6::Pdf PUBLIC Qt6::WebEngineQuick)

target_include_directories(WafflesUx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} bridge core model network qml resources settings ui utility)
