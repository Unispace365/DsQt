cmake_minimum_required(VERSION 3.29)
include(FetchContent)

#project info
set(DS_APP_NAME ClonerSource)
set(DS_EXE_NAME app${DS_APP_NAME})
set(DS_APP_VERSION "1.0.0.0")
set(DS_PROJECT_NAME "PROJECT_NAME_")
set(DS_PROJECT_DESC "PROJECT_DESC_")

project(${DS_APP_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/DEPLOY/")
set(QT_QML_GENERATE_QMLLS_INI ON)
set(DS_QT_DLLS_SUBDIR ON CACHE BOOL "Set to ON to copy the Qt dlls to a subdirectory of the install directory. Default is ON.")
find_package(Qt6 6.9 REQUIRED COMPONENTS Quick Gui ShaderTools Core Sql)
qt_standard_project_setup(REQUIRES 6.9)



list(APPEND CMAKE_MODULE_PATH
    "$ENV{DS_QT_PLATFORM_100}/Library/cmake/")

add_subdirectory("$ENV{DS_QT_PLATFORM_100}/Library" "${CMAKE_BINARY_DIR}/Library")

#set qml import path. This is used by Qt Creator to find the QML files.
set(QML_TMP_PATH
     "${CMAKE_SOURCE_DIR};${CMAKE_SOURCE_DIR}\\qml;${QML_IMPORT_PATH}"
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

qt_add_resources(${DS_EXE_NAME} "icon" FILES newds.ico)

qt_add_qml_module(${DS_EXE_NAME}
    URI ${DS_APP_NAME}
    VERSION 1.0
    QML_FILES
        Main.qml
        qml/Sizer.qml
        qml/Gallery.qml
        qml/Downstream.qml
    DEPENDENCIES
        Dsqt
        QtQuick
        QML_FILES qml/MediaViewerProxy.qml
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

##Fetch toml++
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_GetProperties(tomlplusplus)
FetchContent_MakeAvailable(tomlplusplus)

##Fetch GLM from the g-truc glm repo.
##https://glm.g-truc.net/0.9.9/index.html
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_GetProperties(glm)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    qtimgui
    GIT_REPOSITORY https://github.com/alpqr/qrhiimgui2.git
    GIT_TAG        94c99a8f141469169421d17b64007793988c4009
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_GetProperties(qtimgui)
FetchContent_MakeAvailable(qtimgui)

target_include_directories(${DS_EXE_NAME} PRIVATE ${Dsqt_INCLUDES} Qt6::ShaderToolsPrivate)

##connect qrhiimgui
set(imgui_base ${qtimgui_SOURCE_DIR}/imgui)
set(imgui_target ${DS_EXE_NAME})
include(${imgui_base}/imgui.cmakeinc)
include(${imgui_base}/imguiquick.cmakeinc)

##connect glm
target_include_directories(${DS_EXE_NAME} PUBLIC ${glm_SOURCE_DIR})

##connect toml++
target_include_directories(${DS_EXE_NAME} PUBLIC ${tomlplusplus_SOURCE_DIR}/include)


target_link_libraries(${DS_EXE_NAME}
    PRIVATE Qt6::Quick Dsqt Dsqtplugin Dsqtplugin_init Dsqt_resources_1 Dsqt_resources_2 Dsqt_resources_3 glm Qt6::Sql Qt6::GuiPrivate Qt6::Gui Qt6::ShaderTools Qt6::ShaderToolsPrivate
)

message("HEADERS ${Dsqt_INCLUDES}")
include(GNUInstallDirs)

install(TARGETS ${DS_EXE_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "settings/" DESTINATION ${CMAKE_INSTALL_BINDIR}/settings OPTIONAL)
install(DIRECTORY "data/" DESTINATION ${CMAKE_INSTALL_BINDIR}/data OPTIONAL)

qt_generate_deploy_qml_app_script(
    TARGET ${DS_EXE_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

install(SCRIPT ${deploy_script})

IF (WIN32 AND DS_QT_DLLS_SUBDIR)

add_definitions(-DDS_QT_DLLS_SUBDIR)
# set the path to the dlls in the install directory
set(nativeDllPath "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
# convert to a cmake path
cmake_path(CONVERT "${nativeDllPath}" TO_CMAKE_PATH_LIST cmakeDllPath)
file(MAKE_DIRECTORY "${cmakeDllPath}/qt")

#make an empty qt manifest for running the app inside Qt Creator 
configure_file("${CMAKE_SOURCE_DIR}/cmake/qt.manifest.in" "${CMAKE_BINARY_DIR}/qt/qt.manifest")

# process the dlls
install(CODE "
# grab all the dlls in the install directory
file(GLOB xdlls \"${cmakeDllPath}/*.dll\")\n

#loop through the dlls\n
foreach(dll \${xdlls})\n

    #get the name of the dll sans the path\n
    file(RELATIVE_PATH rDll \"${cmakeDllPath}\" \"\${dll}\" )
    
    #add it to the list of <file> elements to add to the manifest\n
    set(DLL_LIST \"\${DLL_LIST}\n<file name=\\\"\${rDll}\\\"/>\")\n
    
    #move the dll to the qt directory\n
    file(RENAME \"\${dll}\" \"${cmakeDllPath}/qt/\${rDll}\")\n

endforeach()\n

#use the list to create the manifest\n
configure_file(\"${CMAKE_SOURCE_DIR}/cmake/qt.manifest.in\" \"${cmakeDllPath}/qt/qt.manifest\")
")

ENDIF()
