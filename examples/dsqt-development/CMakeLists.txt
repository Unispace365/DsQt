cmake_minimum_required(VERSION 3.5)
include("$ENV{DS_QT_PLATFORM_100}/cmake/dsqt.cmake")
#Set Install location as a relative location to the source dir -AOF
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/ CACHE PATH "Install Prefix" FORCE)

project(dsqt-development VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(QT_QML_DEBUG 1)
set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/dsqt-development.rc")
find_package(Qt6 6.2 COMPONENTS Quick Gui REQUIRED Sql)


qt_add_executable(appdsqt-development
    base.qrc
    main.cpp
    ${app_icon_resource_windows}
)

qt_add_qml_module(appdsqt-development
    URI dsqt-development
    VERSION 1.0
    QML_FILES qml/main.qml qml/Sizer.qml
)


downstream_modules(TARGET appdsqt-development MODULES  DsBridgeQuery dsqml)
##find_package(tomlplusplus)

#get the settings to show up in the IDE
file(GLOB_RECURSE DS_SETTINGS LIST_DIRECTORIES true "./settings/*")
LIST(APPEND DS_SETTINGS qt.conf)

add_custom_target(ds-settings SOURCES ${DS_SETTINGS})
#get the settings to show up in the IDE
file(GLOB_RECURSE DS_DATA LIST_DIRECTORIES true "./data/*")
add_custom_target(ds-data SOURCES ${DS_DATA})



set_target_properties(appdsqt-development PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appdsqt-development
    PRIVATE Qt6::Quick Qt6::Gui Qt6::Sql)

#install(TARGETS appdsqt-development
#    BUNDLE DESTINATION .
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(TARGETS appdsqt-development
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET appdsqt-development
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})

