if(NOT CMAKE_PROJECT_NAME OR CMAKE_PROJECT_NAME STREQUAL "ProjectCloner")
    cmake_minimum_required(VERSION 3.16)
    project(ProjectCloner VERSION 0.1 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_QML_GENERATE_QMLLS_INI ON)

set(DS_QT_DLLS_SUBDIR ON CACHE BOOL "Set to ON to copy the Qt dlls to a subdirectory of the install directory. Default is ON.")

find_package(Qt6 REQUIRED COMPONENTS Quick Core)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appProjectCloner
    main.cpp
)

qt_add_qml_module(appProjectCloner
    URI ProjectCloner
    VERSION 1.0
    QML_FILES
        Main.qml
        ConsoleOutputView.qml
    SOURCES
        cloner.h cloner.cpp
        git_ignore_checker.h git_ignore_checker.cpp
        consoleoutput.h consoleoutput.cpp
    RESOURCES skip.txt
    DEPENDENCIES QtQuick
    RESOURCES images/clone1.gif images/clone2.gif images/clone3.gif images/clone4.gif images/error.gif
    QML_FILES MainView.qml
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/skip.txt" "${CMAKE_BINARY_DIR}/skip.txt" COPYONLY)
#add_custom_command(TARGET appProjectCloner POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#        "${CMAKE_CURRENT_SOURCE_DIR}/ProjectSource"
#        "$<TARGET_FILE_DIR:appProjectCloner>/ProjectSource"
#)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appProjectCloner PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appProjectCloner
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appProjectCloner
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)

# When built as a subdirectory of DsQt, nest under Tools/ProjectCloner/
if(NOT CMAKE_PROJECT_NAME OR CMAKE_PROJECT_NAME STREQUAL "ProjectCloner")
    set(PC_INSTALL_PREFIX "")
else()
    set(PC_INSTALL_PREFIX "Tools/ProjectCloner/")
endif()

target_compile_definitions(appProjectCloner PRIVATE
    CLONER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../Examples/ClonerSource"
)

install(TARGETS appProjectCloner
    BUNDLE DESTINATION ${PC_INSTALL_PREFIX}
    LIBRARY DESTINATION ${PC_INSTALL_PREFIX}${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${PC_INSTALL_PREFIX}${CMAKE_INSTALL_BINDIR}
)

install(FILES skip.txt DESTINATION ${PC_INSTALL_PREFIX}${CMAKE_INSTALL_BINDIR})

qt_generate_deploy_qml_app_script(
    TARGET appProjectCloner
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

# Override QT_DEPLOY_* variables at install time so the deploy script places
# DLLs, plugins, QML modules and translations under our install prefix.
install(CODE "
    set(QT_DEPLOY_BIN_DIR \"${PC_INSTALL_PREFIX}${CMAKE_INSTALL_BINDIR}\")
    set(QT_DEPLOY_LIB_DIR \"${PC_INSTALL_PREFIX}${CMAKE_INSTALL_LIBDIR}\")
    set(QT_DEPLOY_PLUGINS_DIR \"${PC_INSTALL_PREFIX}plugins\")
    set(QT_DEPLOY_QML_DIR \"${PC_INSTALL_PREFIX}qml\")
    set(QT_DEPLOY_TRANSLATIONS_DIR \"${PC_INSTALL_PREFIX}translations\")
")
install(SCRIPT ${deploy_script})


IF (WIN32 AND DS_QT_DLLS_SUBDIR)
add_definitions(-DDS_QT_DLLS_SUBDIR)
# set the path to the dlls in the install directory
set(nativeDllPath "${CMAKE_INSTALL_PREFIX}/${PC_INSTALL_PREFIX}${CMAKE_INSTALL_BINDIR}")
# convert to a cmake path
cmake_path(CONVERT "${nativeDllPath}" TO_CMAKE_PATH_LIST cmakeDllPath)

# process the dlls

ENDIF()
