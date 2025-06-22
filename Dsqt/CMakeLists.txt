cmake_minimum_required(VERSION 3.29)
include(FetchContent)
project(Dsqt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(QML_ROOT ${CMAKE_BINARY_DIR}\\Dsqt)

find_package(Doxygen REQUIRED dot)
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})
if(BUILD_DOCUMENTATION)
set(DOXYGEN_HTML_OUTPUT            ${PROJECT_SOURCE_DIR}/../Documentation/html)
set(DOXYGEN_JAVADOC_AUTOBRIEF      YES)
set(DOXYGEN_GENERATE_HTML          YES)
set(DOXYGEN_HAVE_DOT               YES)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE doc/main.md)

doxygen_add_docs(doxy
  "src"
)
endif()

find_package(Qt6 6.9 COMPONENTS Core Quick Gui ShaderTools Sql REQUIRED COMPONENTS Test)
qt_standard_project_setup(REQUIRES 6.9)

add_subdirectory("src")

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

##connect qrhiimgui
set(imgui_base ${qtimgui_SOURCE_DIR}/imgui)
set(imgui_target Dsqt)
#include(${imgui_base}/imgui.cmakeinc)
#include(${imgui_base}/imguiquick.cmakeinc)
target_include_directories(Dsqt PUBLIC ${imgui_base})

##connect glm
target_include_directories(Dsqt PUBLIC ${glm_SOURCE_DIR})

##connect toml++
target_include_directories(Dsqt PUBLIC ${tomlplusplus_SOURCE_DIR}/include)


set_target_properties(Dsqt PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_compile_definitions(Dsqt
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(Dsqt
PRIVATE Qt6::Quick Qt6::Core Qt6::Gui Qt6::ShaderTools Qt6::GuiPrivate Qt6::Sql)

target_include_directories(Dsqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${imgui_base})

cmake_path(NATIVE_PATH CMAKE_CURRENT_SOURCE_DIR DS_QMLLINT_ROOT)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/.qmllint.in.ini ${CMAKE_SOURCE_DIR}/.qmllintx.ini)
#configure_file(${CMAKE_SOURCE_DIR}/cmake/FindDsqt.in.cmake ${CMAKE_SOURCE_DIR}/cmake/FindDsqt.cmake @ONLY)
#export(TARGETS Dsqt Dsqt_resources_1 Dsqt_resources_2 Dsqt_resources_3 Dsqt_resources_4 Dsqtplugin Dsqtplugin_init NAMESPACE Dsqt:: FILE ${CMAKE_SOURCE_DIR}/cmake/DsqtImport.cmake )
set_target_properties(Dsqt PROPERTIES FOLDER DsQt-Framework)
if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(Tests/Settings)
  add_subdirectory(Tests/BridgeTest)
endif()


