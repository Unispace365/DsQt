cmake_minimum_required(VERSION 3.29)
include(FetchContent)

#project info
set(DS_APP_NAME ClonerSource)
set(DS_EXE_NAME app${DS_APP_NAME})
set(DS_PROJECT_NAME "PROJECT_NAME_")
set(DS_PROJECT_DESC "PROJECT_DESC_")

# Read version from VERSION.txt (source of truth)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" DS_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${DS_VERSION_STRING}" DS_VERSION_STRING)
set(DS_APP_VERSION "${DS_VERSION_STRING}.0")

#define the project
project(${DS_APP_NAME} VERSION ${DS_VERSION_STRING} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/DEPLOY/")
set(QT_FORCE_CMP0156_TO_VALUE NEW)
set(QT_QML_GENERATE_QMLLS_INI ON)
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Quick QuickControls2 Multimedia Gui GuiPrivate ShaderTools Concurrent Sql Pdf WebEngineQuick)
qt_standard_project_setup(REQUIRES 6.9)

# Find Dsqt from installed package (CMAKE_PREFIX_PATH set via CMakePresets.json)
find_package(Dsqt REQUIRED COMPONENTS Core Bridge Waffles TouchEngine)

message("DSQT is at ${Dsqt_DIR}")

#set qml import path. This is used by Qt Creator to find the QML files.
set(QML_TMP_PATH
     "${CMAKE_SOURCE_DIR};${CMAKE_SOURCE_DIR}/qml;${QML_IMPORT_PATH}"
)
list(REMOVE_DUPLICATES QML_TMP_PATH)
set(QML_IMPORT_PATH
     "${QML_TMP_PATH}"
     CACHE PATH "QT creator qml import path" FORCE
)

#setup windows resource file
configure_file("cmake/app.rc.in" "app.rc")

qt_add_executable(${DS_EXE_NAME}
    main.cpp
    app.rc
)

target_link_libraries(${DS_EXE_NAME} PRIVATE Qt6::Quick)
qt_add_resources(${DS_EXE_NAME} "icon" FILES newds.ico)

#make Fnt.qml a singleton
set_source_files_properties(Fnt.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Get Dsqt paths from the installed package (set by DsqtConfig.cmake)
message(STATUS "DSQT_INSTALL_PREFIX: ${DSQT_INSTALL_PREFIX}")
message(STATUS "DSQT_QML_IMPORT_PATH: ${DSQT_QML_IMPORT_PATH}")

qt_add_qml_module(${DS_EXE_NAME}
    URI ${DS_APP_NAME}
    VERSION 1.0
    QML_FILES
        Main.qml
        Fnt.qml
        qml/MainView.qml
        qml/CustomClock.qml
        qml/TouchDesigner.qml

    IMPORT_PATH "${DSQT_QML_IMPORT_PATH}"
    DEPENDENCIES
        Dsqt::Core
        Dsqt::Bridge
        Dsqt::TouchEngine
        Dsqt::Waffles
        QtQuick
        #RESOURCES cmake/app.rc.in
)



LIST(APPEND DIR_FOR_INSTALL "./settings/" "./data/")

#get the settings to show up in the IDE
file(GLOB_RECURSE DS_SETTINGS LIST_DIRECTORIES false ./settings/*)
#LIST(APPEND DS_SETTINGS qt.conf)
LIST(APPEND DS_SETTINGS .qmlls.ini)
add_custom_target(ds-settings SOURCES ${DS_SETTINGS})

#get the data to show up in the IDE
file(GLOB_RECURSE DS_DATA LIST_DIRECTORIES false RELATIVE . ./data/*)
file(GLOB_RECURSE DS_DATA_TARGET LIST_DIRECTORIES false ./data/*)

qt_add_resources(${DS_EXE_NAME} data
    PREFIX "/"
    FILES ${DS_DATA}
)

add_custom_target(ds-data SOURCES ${DS_DATA_TARGET})
add_custom_target(aux SOURCES Readme.md)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(${DS_EXE_NAME} PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${DS_EXE_NAME}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)
find_package(glm CONFIG REQUIRED)

target_include_directories(${DS_EXE_NAME} PRIVATE ${Dsqt_INCLUDES} ${TouchEngineQt_INCLUDES})


target_link_libraries(${DS_EXE_NAME}
    PRIVATE
        Dsqt::Core
        Dsqt::Bridge
        Dsqt::TouchEngine
        Dsqt::Waffles

        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::ShaderTools
        Qt6::GuiPrivate
        Qt6::Sql

        PkgConfig::tomlplusplus
        glm::glm
)

message("HEADERS ${Dsqt_INCLUDES}")
include(GNUInstallDirs)

# TouchEngine SDK DLL location (from installed package, config-specific subdirectory)
set(TOUCHENGINE_DLL "${DSQT_BIN_DIR}/$<CONFIG>/TouchEngine.dll")

# Copy TouchEngine.dll next to the executable during build
if(WIN32)
    add_custom_command(TARGET ${DS_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TOUCHENGINE_DLL}"
            $<TARGET_FILE_DIR:${DS_EXE_NAME}>
        COMMENT "Copying TouchEngine.dll to output directory"
    )
endif()

install(TARGETS ${DS_EXE_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "settings/" DESTINATION ${CMAKE_INSTALL_BINDIR}/settings OPTIONAL)
install(DIRECTORY "data/" DESTINATION ${CMAKE_INSTALL_BINDIR}/data OPTIONAL)

# Install TouchEngine.dll to the deploy directory
if(WIN32)
    install(FILES "${DSQT_BIN_DIR}/$<CONFIG>/TouchEngine.dll" DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
endif()

# Install vcpkg runtime dependencies (tomlplusplus, etc.)
# vcpkg in manifest mode installs DLLs to vcpkg_installed/x64-windows/bin
if(WIN32)
    # Install all DLLs from vcpkg manifest installation
    file(GLOB VCPKG_RUNTIME_DLLS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/*.dll")
    if(VCPKG_RUNTIME_DLLS)
        install(FILES ${VCPKG_RUNTIME_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

qt_generate_deploy_qml_app_script(
    TARGET ${DS_EXE_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

install(SCRIPT ${deploy_script})
