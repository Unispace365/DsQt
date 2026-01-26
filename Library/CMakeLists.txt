cmake_minimum_required(VERSION 3.29)

include(FetchContent)

project(Dsqt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_FORCE_CMP0156_TO_VALUE NEW)


#set(QML_ROOT ${CMAKE_BINARY_DIR}\\DsQt)

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
        Gui 
        Qml
        Quick
    #REQUIRED COMPONENTS
    #Test
)

#qt_standard_project_setup(REQUIRES 6.9)

add_subdirectory("Dsqt/Core")
add_subdirectory("Dsqt/Bridge")
add_subdirectory("Dsqt/Waffles")
add_subdirectory("Dsqt/TouchEngine")

if(Dsqt::Core::DSQT_USE_GLM)
    message(STATUS "Dsqt: GLM support enabled")
else()
    message(STATUS "Dsqt: GLM support disabled")
endif()

add_library(Dsqt INTERFACE)
add_library(Dsqt::Dsqt ALIAS Dsqt)

# target_compile_definitions(Dsqt PRIVATE GLM_ENABLE_EXPERIMENTAL)

#set_target_properties(Core PROPERTIES FOLDER Core)
#set_target_properties(Bridge PROPERTIES FOLDER Bridge)
#set_target_properties(Waffles PROPERTIES FOLDER Waffles)

target_link_libraries(Dsqt
  INTERFACE
    Dsqt::Core
    Dsqt::Bridge
    Dsqt::Waffles
    Dsqt::TouchEngine
)

# FetchContent_Declare(
#     tomlplusplus
#     GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
#     GIT_TAG        v3.2.0
#     #PREFIX ${CMAKE_DIR_LOCATION}/_deps
# )
# FetchContent_GetProperties(tomlplusplus)
# FetchContent_MakeAvailable(tomlplusplus)

##Fetch GLM from the g-truc glm repo.
##https://glm.g-truc.net/0.9.9/index.html
# FetchContent_Declare(
#     glm
#     GIT_REPOSITORY https://github.com/g-truc/glm.git
#     GIT_TAG        1.0.3
#     CONFIGURE_COMMAND ""
#     BUILD_COMMAND ""
#     #PREFIX ${CMAKE_DIR_LOCATION}/_deps
# )
# FetchContent_GetProperties(glm)
# FetchContent_MakeAvailable(glm)

# ##connect glm
# target_include_directories(DsQt PUBLIC ${glm_SOURCE_DIR})

# ##connect toml++
# target_include_directories(DsQt PUBLIC ${tomlplusplus_SOURCE_DIR}/include)


# set_target_properties(DsQt PROPERTIES
#     MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
#     MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
#     MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
#     MACOSX_BUNDLE TRUE
#     WIN32_EXECUTABLE TRUE
# )

# target_compile_definitions(DsQt
#     PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
# target_link_libraries(DsQt
#     PRIVATE
#         Qt6::Quick
#         Qt6::Core
#         Qt6::Concurrent
#         Qt6::Gui
#         Qt6::GuiPrivate
#         Qt6::ShaderTools
#         Qt6::ShaderToolsPrivate
#         Qt6::Sql
# )

# target_include_directories(DsQt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${imgui_base})

cmake_path(NATIVE_PATH CMAKE_CURRENT_SOURCE_DIR DS_QMLLINT_ROOT)
# set_target_properties(DsQt PROPERTIES FOLDER DsQt-Framework)
# set_target_properties(WafflesUx PROPERTIES FOLDER WafflesUx)
# set_target_properties(TouchEngineQt PROPERTIES FOLDER TouchEngineQt)

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
        # Core
        ${CoreTargets}
        Coreplugin
        # Bridge
        ${BridgeTargets}
        Bridgeplugin
        # Waffles
        ${WafflesTargets}
        Wafflesplugin
        # TouchEngine
        ${TouchEngineTargets}
        TouchEngineplugin
        #Waffles_resources_1
        #Waffles_resources_2
        #Waffles_resources_3

        # Create an export set named DsqtTargets that other projects can import
        EXPORT DsqtTargets

        # Where to put the different kinds of files
        LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}      # .so / .dylib
        ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}      # .a / .lib
        RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}      # .dll / executables
        # Tell consumers where to find the headers
        INCLUDES   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Export targets for build-tree consumption (allows find_package from build dir)
    export(EXPORT DsqtTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/DsqtTargets.cmake"
        NAMESPACE Dsqt::
    )

    # Export the targets so other CMake projects can use find_package(Dsqt) after install
    install(EXPORT DsqtTargets
        FILE DsqtTargets.cmake
        NAMESPACE Dsqt::                # Results in Dsqt::Core, Dsqt::Bridge, Dsqt::Waffles, etc.
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dsqt
    )

    # Create the main Config.cmake file from our template
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DsqtConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/DsqtConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dsqt
    )

    # Generate the version file that find_package() will check
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/DsqtConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion      # Accept 1.x.y but not 2.0.0
        # COMPATIBILITY AnyNewerVersion     # ← more permissive alternative
    )

    # Install the two generated configuration files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/DsqtConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/DsqtConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dsqt
    )

    # ----------------------------------------------------------------------
    # Register install location in package registry (cross-platform)
    # Uses install-time prefix via CMAKE_INSTALL_PREFIX set by cmake --install
    # ----------------------------------------------------------------------
    if(WIN32)
        # Windows: Write to registry
        install(CODE [[
            set(_config_dir "${CMAKE_INSTALL_PREFIX}/lib/cmake/Dsqt")
            string(MD5 _hash "${_config_dir}")
            execute_process(
                COMMAND reg add "HKEY_CURRENT_USER\\Software\\Kitware\\CMake\\Packages\\Dsqt"
                        /v "${_hash}" /t REG_SZ
                        /d "${_config_dir}"
                        /f
                OUTPUT_QUIET ERROR_QUIET
            )
            message(STATUS "Registered Dsqt in CMake package registry: ${_config_dir}")
        ]])
    else()
        # Linux/macOS: Write file to ~/.cmake/packages/Dsqt/
        install(CODE [[
            set(_config_dir "${CMAKE_INSTALL_PREFIX}/lib/cmake/Dsqt")
            set(_reg_dir "$ENV{HOME}/.cmake/packages/Dsqt")
            file(MAKE_DIRECTORY "${_reg_dir}")
            string(MD5 _hash "${_config_dir}")
            file(WRITE "${_reg_dir}/${_hash}" "${_config_dir}")
            message(STATUS "Registered Dsqt in CMake package registry: ${_config_dir}")
        ]])
    endif()
endif()
