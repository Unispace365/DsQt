cmake_minimum_required(VERSION 3.29)

include(FetchContent)

# Read version from VERSION.txt (source of truth)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" DSQT_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${DSQT_VERSION_STRING}" DSQT_VERSION_STRING)

project(Dsqt VERSION ${DSQT_VERSION_STRING} LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: enable C++ exceptions (required by toml++ and Qt).
# The Visual Studio generator adds /EHsc by default but Ninja does not.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    string(APPEND CMAKE_CXX_FLAGS " /EHsc")
endif()

set(QT_FORCE_CMP0156_TO_VALUE NEW)

# Consolidate all QML module output into a single directory structure
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Tell Qt Creator (and other IDEs) where to find the built QML modules.
set(QML_TMP_PATH "${QT_QML_OUTPUT_DIRECTORY};${QML_IMPORT_PATH}")
list(REMOVE_DUPLICATES QML_TMP_PATH)
set(QML_IMPORT_PATH "${QML_TMP_PATH}" CACHE PATH "Qt Creator QML import path" FORCE)

# Regenerate .qmlls.ini files in each module's source directory so qmlls
# points at this build's output rather than any previously cached build.
set(QT_QML_GENERATE_QMLLS_INI ON)

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

# --- Qt auto-discovery (Windows) -------------------------------------------------
# Scans C:/Qt/*/msvc2022_64 and selects the highest installed version.
# Override by setting CMAKE_PREFIX_PATH or Qt6_DIR in your CMakeUserPresets.json.
if(WIN32 AND NOT Qt6_DIR)
    file(GLOB _qt_dirs LIST_DIRECTORIES true "C:/Qt/*/msvc2022_64")
    if(_qt_dirs)
        list(SORT _qt_dirs ORDER DESCENDING)
        list(GET _qt_dirs 0 _qt_auto)
        message(STATUS "DsQt: Auto-discovered Qt at ${_qt_auto}")
        list(APPEND CMAKE_PREFIX_PATH "${_qt_auto}")
    else()
        message(WARNING "DsQt: No Qt installation found in C:/Qt/. "
                        "Set CMAKE_PREFIX_PATH or Qt6_DIR to your Qt path.")
    endif()
endif()
# ---------------------------------------------------------------------------------

find_package(Qt6 6.9
    REQUIRED COMPONENTS
        Core
        Concurrent
        Gui
        Qml
        Quick
)

option(DSQT_BUILD_TESTS "Build and run DsQt unit tests" OFF)
if(DSQT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

#qt_standard_project_setup(REQUIRES 6.9)

add_subdirectory("Dsqt/Core")
add_subdirectory("Dsqt/Bridge")
add_subdirectory("Dsqt/Waffles")
add_subdirectory("Dsqt/TouchEngine")
add_subdirectory("Dsqt/Spout")
add_subdirectory("Dsqt/Touch")

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
    Dsqt::Touch
)

if(WIN32 AND TARGET Dsqt::Spout)
    target_link_libraries(Dsqt INTERFACE Dsqt::Spout)
endif()

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

if(DSQT_BUILD_TESTS)
    add_subdirectory(Tests/Settings)
    add_subdirectory(Tests/Logging)
    # add_subdirectory(Tests/BridgeTest)  # Stubbed — enable when tests are implemented
endif()

# ----------------------------------------------------------------------
# Installation rules
# ----------------------------------------------------------------------



if(PROJECT_IS_TOP_LEVEL)
    option(DSQT_BUILD_TOOLS "Build and install DsQt tools (ProjectCloner)" OFF)

    if(DSQT_BUILD_TOOLS)
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/../Tools/ProjectCloner"
            "${CMAKE_BINARY_DIR}/Tools/ProjectCloner"
        )
    endif()

    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install the built library targets
    # Note: Qt's qt_add_qml_module creates internal OBJECT library targets that
    # cannot be exported. We install the libraries without export and create
    # imported targets manually in DsqtConfig.cmake.
    install(TARGETS
        Core
        Coreplugin
        Bridge
        Bridgeplugin
        Waffles
        Wafflesplugin
        TouchEngine
        TouchEngineplugin
        Touch
        Touchplugin

        # Where to put the different kinds of files
        # Install into config-specific subdirectories so Debug and Release
        # libraries can coexist in the same install prefix.
        LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>      # .so / .dylib
        ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>      # .a / .lib
        RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>      # .dll / executables
    )

    # Spout module (Windows only)
    if(WIN32 AND TARGET DsSpout)
        set(_spout_install_targets DsSpout DsSpoutplugin)

        #if(TARGET SpoutDX_static)
            # FetchContent build — install the in-tree #static lib too
            #list(APPEND _spout_install_targets #SpoutDX_static)
        #endif()
        install(TARGETS ${_spout_install_targets}
            LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
            ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
            RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
        )
    endif()

    # ----------------------------------------------------------------------
    # Install QML modules (qmldir, qmltypes, plugins)
    # Standard Qt location: ${prefix}/qml/Module/Name/
    # ----------------------------------------------------------------------

    # Install QML module metadata files (qmldir and qmltypes) for each module
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Core/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Core/Core.qmltypes"
        DESTINATION "qml/Dsqt/Core"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Bridge/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Bridge/Bridge.qmltypes"
        DESTINATION "qml/Dsqt/Bridge"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Waffles/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Waffles/Waffles.qmltypes"
        DESTINATION "qml/Dsqt/Waffles"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/TouchEngine/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/TouchEngine/TouchEngine.qmltypes"
        DESTINATION "qml/Dsqt/TouchEngine"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Touch/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Touch/Touch.qmltypes"
        DESTINATION "qml/Dsqt/Touch"
    )

    # Spout QML module metadata (Windows only)
    if(WIN32 AND TARGET DsSpout)
        install(FILES
            "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Spout/qmldir"
            "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Spout/DsSpout.qmltypes"
            DESTINATION "qml/Dsqt/Spout"
        )
    endif()

    # Install QML source files for each module
    # These are needed by qmlimportscanner which looks for files referenced in qmldir
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Core/qml"
        DESTINATION "qml/Dsqt/Core"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Bridge/qml"
        DESTINATION "qml/Dsqt/Bridge"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Waffles/qml"
        DESTINATION "qml/Dsqt/Waffles"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/TouchEngine/qml"
        DESTINATION "qml/Dsqt/TouchEngine"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Touch/qml"
        DESTINATION "qml/Dsqt/Touch"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )

    # Spout QML files (Windows only)
    if(WIN32 AND TARGET DsSpout)
        install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Spout/qml"
            DESTINATION "qml/Dsqt/Spout"
            OPTIONAL
            FILES_MATCHING PATTERN "*.qml"
        )
    endif()

    # ── Bundle tomlplusplus alongside DsQt ──────────────────────────────────────
    # Copies vcpkg's tomlplusplus headers, import libs, and DLLs into the install
    # prefix and installs a self-contained cmake config.  This makes DsQt fully
    # self-contained: consuming projects no longer need vcpkg or any extra
    # CMAKE_PREFIX_PATH entry to satisfy find_package(Dsqt).
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_dsqt_toml_vcpkg "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

        # Headers (toml++ puts everything under include/toml++/)
        if(IS_DIRECTORY "${_dsqt_toml_vcpkg}/include/toml++")
            install(DIRECTORY "${_dsqt_toml_vcpkg}/include/toml++"
                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            )
        endif()

        # Release import library
        if(EXISTS "${_dsqt_toml_vcpkg}/lib/tomlplusplus.lib")
            install(FILES "${_dsqt_toml_vcpkg}/lib/tomlplusplus.lib"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/Release"
            )
        endif()

        # Debug import library
        if(EXISTS "${_dsqt_toml_vcpkg}/debug/lib/tomlplusplus.lib")
            install(FILES "${_dsqt_toml_vcpkg}/debug/lib/tomlplusplus.lib"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/Debug"
            )
        endif()

        # Release DLL(s) — glob so the versioned name (tomlplusplus-3.dll) can vary
        file(GLOB _dsqt_toml_rel_dlls "${_dsqt_toml_vcpkg}/bin/tomlplusplus*.dll")
        if(_dsqt_toml_rel_dlls)
            install(FILES ${_dsqt_toml_rel_dlls}
                DESTINATION "${CMAKE_INSTALL_BINDIR}/Release"
            )
        endif()

        # Debug DLL(s) + PDB
        file(GLOB _dsqt_toml_dbg_files
            "${_dsqt_toml_vcpkg}/debug/bin/tomlplusplus*.dll"
            "${_dsqt_toml_vcpkg}/debug/bin/tomlplusplus*.pdb"
        )
        if(_dsqt_toml_dbg_files)
            install(FILES ${_dsqt_toml_dbg_files}
                DESTINATION "${CMAKE_INSTALL_BINDIR}/Debug"
                OPTIONAL
            )
        endif()

        # Self-contained cmake config — lets DsqtConfig.cmake find tomlplusplus
        # without requiring vcpkg in the consuming project.
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundledTomlplusplus.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tomlplusplus"
            RENAME "tomlplusplusConfig.cmake"
        )

        unset(_dsqt_toml_vcpkg)
        unset(_dsqt_toml_rel_dlls)
        unset(_dsqt_toml_dbg_files)
    endif()
    # ── End tomlplusplus bundle ─────────────────────────────────────────────────

    # Create the main Config.cmake file from our template
    # Note: We don't use CMake's export() mechanism because Qt's qt_add_qml_module
    # creates internal OBJECT library dependencies that can't be exported.
    # Instead, DsqtConfig.cmake.in creates imported targets manually.
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

    # ----------------------------------------------------------------------
    # Install ClonerSource example template
    # ----------------------------------------------------------------------
    if(DSQT_BUILD_TOOLS)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Examples/ClonerSource/"
            DESTINATION "Examples/ClonerSource"
            PATTERN "build" EXCLUDE
            PATTERN ".qtcreator" EXCLUDE
            PATTERN "CMakeLists.txt.user" EXCLUDE
        )
    endif()
endif()
