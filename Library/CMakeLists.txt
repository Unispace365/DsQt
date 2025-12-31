cmake_minimum_required(VERSION 3.29)
include(FetchContent)
project(Dsqt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_FORCE_CMP0156_TO_VALUE NEW)

#set(QML_ROOT ${CMAKE_BINARY_DIR}\\Dsqt)
if(PROJECT_IS_TOP_LEVEL)
    find_package(Doxygen COMPONENTS dot)
    option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})
    if(BUILD_DOCUMENTATION)
        set(DOXYGEN_HTML_OUTPUT            ${PROJECT_SOURCE_DIR}/../Documentation/html)
        set(DOXYGEN_JAVADOC_AUTOBRIEF      YES)
        set(DOXYGEN_GENERATE_HTML          YES)
        set(DOXYGEN_HAVE_DOT               YES)
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE doc/main.md)

        doxygen_add_docs(doxy
          "Dsqt"
        )
    endif()
endif()

find_package(Qt6 6.9 COMPONENTS Core Concurrent Quick Gui ShaderTools Sql REQUIRED COMPONENTS Test)
qt_standard_project_setup(REQUIRES 6.9)

add_subdirectory("Dsqt")
add_subdirectory("WafflesUx")
add_subdirectory("TouchEngineQt")

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
PRIVATE Qt6::Quick Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::ShaderTools Qt6::Sql)

target_include_directories(Dsqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${imgui_base})

cmake_path(NATIVE_PATH CMAKE_CURRENT_SOURCE_DIR DS_QMLLINT_ROOT)
set_target_properties(Dsqt PROPERTIES FOLDER DsQt-Framework)
set_target_properties(WafflesUx PROPERTIES FOLDER WafflesUx)
set_target_properties(TouchEngineQt PROPERTIES FOLDER TouchEngineQt)

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(Tests/Settings)
  add_subdirectory(Tests/BridgeTest)
endif()


