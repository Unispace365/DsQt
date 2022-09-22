cmake_minimum_required(VERSION 3.16)
include("$ENV{DS_QT_PLATFORM_100}/cmake/dsqt.cmake")
#Set Install location as a relative location to the source dir -AOF
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/ CACHE PATH "Install Prefix" FORCE)

project(dsqt-development VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 COMPONENTS Quick REQUIRED)
find_package(tomlplusplus)

qt_add_executable(appdsqt-development
    main.cpp
)

qt_add_qml_module(appdsqt-development
    URI dsqt-development
    VERSION 1.0
    QML_FILES qml/main.qml
)


downstream_modules(TARGET appdsqt-development MODULES dsqml DsSqlQuery)

#get the settings to show up in the IDE
file(GLOB_RECURSE DS_SETTINGS LIST_DIRECTORIES true "./settings/*")
add_custom_target(ds-settings SOURCES ${DS_SETTINGS})

set_target_properties(appdsqt-development PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appdsqt-development
    PRIVATE Qt6::Quick)
target_include_directories(appdsqt-development PUBLIC tomlplusplus::tomlplusplus)
#install(TARGETS appdsqt-development
#    BUNDLE DESTINATION .
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(TARGETS appdsqt-development
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET appdsqt-development
    FILENAME_VARIABLE deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})

