cmake_minimum_required(VERSION 3.29)
include(FetchContent)

# Project info
set(DS_APP_NAME TouchFilter)
set(DS_EXE_NAME app${DS_APP_NAME})
set(DS_PROJECT_NAME  "TouchFilter")
set(DS_PROJECT_DESC  "TouchFilter diagnostic visualizer — a DsQt example.")
set(DS_APP_VERSION   "0.0.0")

# Attempt to get version from Git tags
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --is-inside-work-tree
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_REPO_CHECK_RESULT
        OUTPUT_VARIABLE GIT_INSIDE_WORK_TREE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(GIT_FOUND AND GIT_REPO_CHECK_RESULT EQUAL 0 AND GIT_INSIDE_WORK_TREE STREQUAL "true")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_TAG)
        string(REGEX MATCH "[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?" GIT_TAG_VERSION "${GIT_TAG}")
        if(GIT_TAG_VERSION MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
            set(DS_APP_VERSION "${GIT_TAG_VERSION}")
        endif()
    endif()
endif()

string(REGEX REPLACE "^([0-9]+)(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?.*"
                     "\\1\\2\\3\\4"
                     TMP_VERSION "${DS_APP_VERSION}.0.0.0")
string(REGEX REPLACE "\\." "," DS_APP_VERSION_COMMA "${TMP_VERSION}")

project(${DS_APP_NAME} VERSION ${DS_APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/DEPLOY/")
set(QT_FORCE_CMP0156_TO_VALUE NEW)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Quick QuickControls2 Gui)
qt_standard_project_setup(REQUIRES 6.9)

# Find Dsqt from installed package (CMAKE_PREFIX_PATH set via CMakePresets.json)
find_package(Dsqt REQUIRED COMPONENTS Core Touch)

message("DSQT is at ${Dsqt_DIR}")

# Set QML import path for Qt Creator
set(QML_TMP_PATH "${CMAKE_SOURCE_DIR};${CMAKE_SOURCE_DIR}/qml;${QML_IMPORT_PATH}")
list(REMOVE_DUPLICATES QML_TMP_PATH)
set(QML_IMPORT_PATH "${QML_TMP_PATH}" CACHE PATH "QT creator qml import path" FORCE)

# Setup Windows resource file
configure_file("cmake/app.rc.in" "app.rc")

qt_add_executable(${DS_EXE_NAME}
    main.cpp
    app.rc
)

target_link_libraries(${DS_EXE_NAME} PRIVATE Qt6::Quick)
qt_add_resources(${DS_EXE_NAME} "icon" FILES newds.ico)

# Make Fnt.qml a singleton
set_source_files_properties(Fnt.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

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
        qml/PanAndZoom.qml
        qml/TouchVisualizerView.qml

    IMPORT_PATH "${DSQT_QML_IMPORT_PATH}"
    DEPENDENCIES
        Dsqt::Core
        Dsqt::Touch
        QtQuick
)

LIST(APPEND DIR_FOR_INSTALL "./settings/" "./data/")

# Get the settings to show up in the IDE
file(GLOB_RECURSE DS_SETTINGS LIST_DIRECTORIES false ./settings/*)
LIST(APPEND DS_SETTINGS .qmlls.ini)
add_custom_target(ds-settings SOURCES ${DS_SETTINGS})

# Get the data to show up in the IDE and embed into the binary
file(GLOB_RECURSE DS_DATA        LIST_DIRECTORIES false RELATIVE . ./data/*)
file(GLOB_RECURSE DS_DATA_TARGET LIST_DIRECTORIES false            ./data/*)

qt_add_resources(${DS_EXE_NAME} data
    PREFIX "/"
    FILES ${DS_DATA}
)

add_custom_target(ds-data SOURCES ${DS_DATA_TARGET})

set_target_properties(${DS_EXE_NAME} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION       ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(${DS_EXE_NAME} PRIVATE ${Dsqt_INCLUDES})

# Provide DSQT_HAS_<MODULE> compile definitions for conditional plugin imports
foreach(_comp Core Touch)
    if(Dsqt_${_comp}_FOUND)
        target_compile_definitions(${DS_EXE_NAME} PRIVATE DSQT_HAS_${_comp})
    endif()
endforeach()

target_link_libraries(${DS_EXE_NAME}
    PRIVATE
        Dsqt::Core
        Dsqt::Touch
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
)

dsqt_deploy_runtime_dlls(${DS_EXE_NAME})

include(GNUInstallDirs)

install(TARGETS ${DS_EXE_NAME}
    BUNDLE  DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "settings/" DESTINATION ${CMAKE_INSTALL_BINDIR}/settings OPTIONAL)
install(DIRECTORY "data/"     DESTINATION ${CMAKE_INSTALL_BINDIR}/data     OPTIONAL)
