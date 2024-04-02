cmake_minimum_required(VERSION 3.16)

include("$ENV{DS_QT_PLATFORM_100}/cmake/dsqt.cmake")
#Set Install location as a relative location to the source dir -AOF
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/ CACHE PATH "Install Prefix" FORCE)

project(DSSettingsTest LANGUAGES CXX)

enable_testing()


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Quick Test)

#get the settings to show up in the IDE
file(GLOB_RECURSE DS_SETTINGS LIST_DIRECTORIES true "./settings/*")
add_custom_target(ds-settings SOURCES ${DS_SETTINGS})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DSSettingsTest tst_dssettingstest.cpp)

downstream_modules(TARGET DSSettingsTest MODULES dsqt DsBridgeQuery)
##find_package(tomlplusplus)

add_test(NAME DSSettingsTest COMMAND DSSettingsTest)

target_link_libraries(DSSettingsTest PRIVATE Qt${QT_VERSION_MAJOR}::Test)
target_link_libraries(DSSettingsTest
    PRIVATE Qt6::Quick)

