cmake_minimum_required(VERSION 3.16)
include("$ENV{DS_QT_PLATFORM_100}/cmake/dsqt.cmake")

project(dsqt-development VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 COMPONENTS Quick REQUIRED)

qt_add_executable(appdsqt-development
    main.cpp
)

qt_add_qml_module(appdsqt-development
    URI dsqt-development
    VERSION 1.0
    QML_FILES main.qml 
)


downstream_modules(TARGET appdsqt-development MODULES dsqml)

set_target_properties(appdsqt-development PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appdsqt-development
    PRIVATE Qt6::Quick)

install(TARGETS appdsqt-development
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})