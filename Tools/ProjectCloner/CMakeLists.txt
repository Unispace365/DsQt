cmake_minimum_required(VERSION 3.16)

set(CMAKE_INSTALL_PREFIX "$ENV{DS_QT_PLATFORM_100}/Tools/deploy/ProjectCloner")
project(ProjectCloner VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_QML_GENERATE_QMLLS_INI ON)

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
        SOURCES cloner.h cloner.cpp
        SOURCES git_ignore_checker.h git_ignore_checker.cpp
        RESOURCES skip.txt
        DEPENDENCIES QtQuick
        RESOURCES images/clone1.gif images/clone2.gif images/clone3.gif images/clone4.gif images/error.gif
)

configure_file("${CMAKE_SOURCE_DIR}/skip.txt" "${CMAKE_BINARY_DIR}/skip.txt" COPYONLY)

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
install(TARGETS appProjectCloner
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET appProjectCloner
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

install(SCRIPT ${deploy_script})
