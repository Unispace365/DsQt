cmake_minimum_required(VERSION 3.29)
include(FetchContent)

#project info
set(DS_APP_NAME DsQtTests)
set(DS_EXE_NAME app${DS_APP_NAME})
set(DS_APP_VERSION "1.0.0.0")
set(DS_PROJECT_NAME "DsQtTests")
set(DS_PROJECT_DESC "DsQt Tests")

#project(${DS_APP_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/DEPLOY/")
set(QT_QML_GENERATE_QMLLS_INI ON)
set(DS_QT_DLLS_SUBDIR ON CACHE BOOL "Set to ON to copy the Qt dlls to a subdirectory of the install directory. Default is ON.")
find_package(Qt6 6.9 REQUIRED COMPONENTS Quick Gui ShaderTools Core Sql Test)
qt_standard_project_setup(REQUIRES 6.9)




enable_testing()


qt_add_executable(${DS_EXE_NAME}
    tst_dssettingstest.cpp
)
add_test(NAME ${DS_EXE_NAME} COMMAND ${DS_EXE_NAME})

#get the settings to show up in the IDE
message("adding test settings target")
file(GLOB_RECURSE TEST_SETTINGS LIST_DIRECTORIES false ${CMAKE_CURRENT_SOURCE_DIR}/settings/*)
#LIST(APPEND DS_SETTINGS qt.conf)
#LIST(APPEND DS_SETTINGS .qmlls.ini)
add_custom_target(test-settings SOURCES ${TEST_SETTINGS})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/settings)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/settings/test_settings.toml ${CMAKE_BINARY_DIR}/settings COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/settings/engine.toml ${CMAKE_BINARY_DIR}/settings COPYONLY)

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

target_include_directories(${DS_EXE_NAME} PUBLIC ${tomlplusplus_SOURCE_DIR}/include)

target_link_libraries(${DS_EXE_NAME}
    PRIVATE Qt6::Quick Dsqt Dsqtplugin Dsqtplugin_init Dsqt_resources_1 Dsqt_resources_2 Dsqt_resources_3 glm Qt6::Sql Qt6::GuiPrivate Qt6::Gui Qt6::ShaderTools Qt6::ShaderToolsPrivate Qt6::Test
)

target_include_directories(${DS_EXE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "../." ${imgui_base})

include(GNUInstallDirs)




