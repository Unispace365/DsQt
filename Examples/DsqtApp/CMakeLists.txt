cmake_minimum_required(VERSION 3.29)
include(FetchContent)

project(DsqtApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick Gui ShaderTools Core Sql)
qt_standard_project_setup(REQUIRES 6.8)


list(APPEND CMAKE_MODULE_PATH
    "$ENV{DS_QT_PLATFORM_100}/../Dsqt/cmake/")

add_subdirectory("$ENV{DS_QT_PLATFORM_100}/../Dsqt" "${CMAKE_BINARY_DIR}/Dsqt")

qt_add_executable(appDsqtApp
    main.cpp
)

qt_add_qml_module(appDsqtApp
    URI DsqtApp
    VERSION 1.0
    QML_FILES
        Main.qml
        qml/Sizer.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appDsqtApp PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appDsqtApp
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.2.0
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_GetProperties(tomlplusplus)
FetchContent_MakeAvailable(tomlplusplus)

##Fetch GLM from the g-truc glm repo.
##https://glm.g-truc.net/0.9.9/index.html
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
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

target_include_directories(appDsqtApp PRIVATE ${Dsqt_INCLUDES} Qt6::ShaderToolsPrivate)

##connect qrhiimgui
set(imgui_base ${qtimgui_SOURCE_DIR}/imgui)
set(imgui_target appDsqtApp)
include(${imgui_base}/imgui.cmakeinc)
include(${imgui_base}/imguiquick.cmakeinc)

##connect glm
target_include_directories(appDsqtApp PUBLIC ${glm_SOURCE_DIR})

##connect toml++
target_include_directories(appDsqtApp PUBLIC ${tomlplusplus_SOURCE_DIR}/include)


target_link_libraries(appDsqtApp
    PRIVATE Qt6::Quick Dsqt Dsqtplugin Dsqtplugin_init Dsqt_resources_1 Dsqt_resources_2 Dsqt_resources_3 glm Qt6::Sql Qt6::GuiPrivate Qt6::Gui Qt6::ShaderTools Qt6::ShaderToolsPrivate
)




message("HEADERS ${Dsqt_INCLUDES}")
include(GNUInstallDirs)
install(TARGETS appDsqtApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
