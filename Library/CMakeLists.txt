cmake_minimum_required(VERSION 3.29)

include(FetchContent)

project(DsQt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_FORCE_CMP0156_TO_VALUE NEW)

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

find_package(Qt6 6.9
    COMPONENTS
        Core
        Concurrent
        Qml
        Quick
        Gui
    # REQUIRED COMPONENTS
    #     Test
)
# qt_standard_project_setup(REQUIRES 6.9)

add_subdirectory("Core")
add_subdirectory("Bridge")
add_subdirectory("Waffles")

add_library(DsQt INTERFACE)
add_library(DsQt::DsQt ALIAS DsQt)

set_target_properties(Core PROPERTIES FOLDER Core)
set_target_properties(Bridge PROPERTIES FOLDER Bridge)
set_target_properties(Waffles PROPERTIES FOLDER Waffles)

target_link_libraries(DsQt
  INTERFACE
    DsQt::Core
    DsQt::Bridge
    DsQt::Waffles
)

# FetchContent_Declare(
#     qtimgui
#     GIT_REPOSITORY https://github.com/alpqr/qrhiimgui2.git
#     GIT_TAG        94c99a8f141469169421d17b64007793988c4009
#     #PREFIX ${CMAKE_DIR_LOCATION}/_deps
# )
# FetchContent_GetProperties(qtimgui)
# FetchContent_MakeAvailable(qtimgui)

# ##connect qrhiimgui
# set(imgui_base ${qtimgui_SOURCE_DIR}/imgui)
# set(imgui_target Dsqt)
# #include(${imgui_base}/imgui.cmakeinc)
# #include(${imgui_base}/imguiquick.cmakeinc)
# target_include_directories(Dsqt PUBLIC ${imgui_base})




# set_target_properties(Dsqt 
#     PROPERTIES
#         MACOSX_BUNDLE_GUI_IDENTIFIER dsqt.downstream.com
#         MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
#         MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
#         MACOSX_BUNDLE TRUE
#         WIN32_EXECUTABLE TRUE
# )

# target_compile_definitions(Dsqt
#     PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
# )
# target_link_libraries(Dsqt
#     PRIVATE 
#         Qt6::Quick 
#         Qt6::Core 
#         Qt6::Concurrent 
#         Qt6::Gui 
#         Qt6::ShaderTools 
#         Qt6::GuiPrivate 
#         Qt6::Sql
# )

# target_include_directories(Dsqt 
#     PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${imgui_base})

cmake_path(NATIVE_PATH CMAKE_CURRENT_SOURCE_DIR DS_QMLLINT_ROOT)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/.qmllint.in.ini ${CMAKE_SOURCE_DIR}/.qmllintx.ini)
#configure_file(${CMAKE_SOURCE_DIR}/cmake/FindDsqt.in.cmake ${CMAKE_SOURCE_DIR}/cmake/FindDsqt.cmake @ONLY)
#export(TARGETS Dsqt Dsqt_resources_1 Dsqt_resources_2 Dsqt_resources_3 Dsqt_resources_4 Dsqtplugin Dsqtplugin_init NAMESPACE Dsqt:: FILE ${CMAKE_SOURCE_DIR}/cmake/DsqtImport.cmake )

# if(PROJECT_IS_TOP_LEVEL)
#   add_subdirectory(Tests/Settings)
#   add_subdirectory(Tests/BridgeTest)
# endif()

# ----------------------------------------------------------------------
# Installation rules
# ----------------------------------------------------------------------

if(PROJECT_IS_TOP_LEVEL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install the built library targets and create an export set
    install(TARGETS
        tomlplusplus_tomlplusplus
        # Core
        Core_resources_1
        Core_resources_2
        Core_resources_3
        Core_resources_4
        # Bridge
        Bridge_resources_1
        Bridge_resources_2
        Bridge_resources_3
        # Waffles
        Waffles_resources_1
        Waffles_resources_2
        Waffles_resources_3

        # Create an export set named DsQtTargets that other projects can import
        EXPORT DsQtTargets

        # Where to put the different kinds of files
        LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}      # .so / .dylib
        ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}      # .a / .lib
        RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}      # .dll / executables
        # Tell consumers where to find the headers
        INCLUDES   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Export the targets so other CMake projects can use find_package(DsQt)
    install(EXPORT DsQtTargets
        FILE DsQtTargets.cmake
        NAMESPACE DsQt::                # Results in DsQt::Core, DsQt::Bridge, DsQt::Waffles, etc.
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DsQt
    )

    # Create the main Config.cmake file from our template
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DsQtConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/DsQtConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DsQt
    )

    # Generate the version file that find_package() will check
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/DsQtConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion      # Accept 1.x.y but not 2.0.0
        # COMPATIBILITY AnyNewerVersion     # ← more permissive alternative
    )

    # Install the two generated configuration files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/DsQtConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/DsQtConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DsQt
    )
endif()
