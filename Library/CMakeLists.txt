cmake_minimum_required(VERSION 3.29)

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

# ----------------------------------------------------------------------
# Install layout
# ----------------------------------------------------------------------
# Historically DsQt installs into per-config subdirectories (lib/Debug,
# lib/Release, bin/Debug, ...) so that a Ninja Multi-Config build can put
# every configuration into one prefix. That remains the default.
#
# vcpkg works differently: it configures and builds each configuration in a
# separate pass and expects the flat layout <prefix>/lib + <prefix>/debug/lib,
# with the CMake package files under <prefix>/share/<port>. The dsqt portfile
# therefore passes:
#     -DDSQT_INSTALL_CONFIG_SUBDIR=OFF
#     -DDSQT_REGISTER_PACKAGE=OFF
#     -DDSQT_INSTALL_CMAKEDIR=share/dsqt
# DsqtConfig.cmake detects which of the two layouts it is sitting in at
# find_package() time, so both consumption paths use the same config file.
option(DSQT_INSTALL_CONFIG_SUBDIR
    "Install binaries into per-config subdirectories (lib/Debug, lib/Release)" ON)
option(DSQT_REGISTER_PACKAGE
    "Record the install prefix in the user-level CMake package registry" ON)
# The standalone install bundles third-party libraries that DsQt links against
# (currently Spout2's SpoutDX_static) into its own prefix, so consumers get a
# self-contained tree without needing vcpkg. Under vcpkg those libraries are
# real dependencies that already install themselves into the shared prefix, and
# installing them a second time is a hard file-conflict error.
option(DSQT_BUNDLE_DEPENDENCIES
    "Copy third-party dependency libraries into the DsQt install prefix" ON)
set(DSQT_INSTALL_CMAKEDIR "lib/cmake/Dsqt"
    CACHE STRING "Directory (relative to the install prefix) for DsqtConfig.cmake")
# vcpkg's post-build validation rejects unexpected top-level directories in a
# package, so the port installs the QML modules to share/dsqt/qml instead.
# DsqtConfig.cmake looks in both places.
set(DSQT_INSTALL_QMLDIR "qml"
    CACHE STRING "Directory (relative to the install prefix) for the QML modules")

if(DSQT_INSTALL_CONFIG_SUBDIR)
    # Expands to /Debug, /Release, ... at install time.
    set(DSQT_CONFIG_DIR "/$<CONFIG>")
else()
    set(DSQT_CONFIG_DIR "")
endif()

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

# --- Qt kit discovery (Windows) --------------------------------------------------
# Three ways to select a Qt kit, highest precedence first:
#
#   1. Qt6_DIR / CMAKE_PREFIX_PATH        - explicit, nothing is guessed.
#   2. DSQT_QT_VERSION + DSQT_QT_ARCH     - pin an exact kit, e.g. "6.10.2".
#                                           This is what the vcpkg triplets use:
#                                           the requested version is part of the
#                                           triplet's text, and therefore part of
#                                           vcpkg's ABI hash, so each Qt kit gets
#                                           its own binary-cache entry.
#   3. auto-discovery                     - highest kit installed under DSQT_QT_ROOT.
#
# A pinned kit that is not installed is a hard error: silently falling back to a
# different Qt would produce binaries that get cached under the wrong ABI hash.
if(WIN32 AND NOT Qt6_DIR)
    set(DSQT_QT_ROOT "C:/Qt" CACHE PATH "Root of the Qt online-installer tree")
    set(DSQT_QT_ARCH "msvc2022_64" CACHE STRING "Qt kit architecture directory name")
    set(DSQT_QT_VERSION "" CACHE STRING "Exact Qt kit version to build against, e.g. 6.10.2")

    if(DSQT_QT_VERSION)
        set(_qt_pinned "${DSQT_QT_ROOT}/${DSQT_QT_VERSION}/${DSQT_QT_ARCH}")
        if(NOT IS_DIRECTORY "${_qt_pinned}")
            message(FATAL_ERROR
                "DsQt: Qt kit ${DSQT_QT_VERSION} (${DSQT_QT_ARCH}) was requested but "
                "'${_qt_pinned}' does not exist.\n"
                "Install that kit with the Qt Maintenance Tool, or select a triplet "
                "matching a kit you do have.")
        endif()
        message(STATUS "DsQt: Using pinned Qt kit ${_qt_pinned}")
        list(APPEND CMAKE_PREFIX_PATH "${_qt_pinned}")
    else()
        file(GLOB _qt_dirs LIST_DIRECTORIES true "${DSQT_QT_ROOT}/*/${DSQT_QT_ARCH}")
        if(_qt_dirs)
            list(SORT _qt_dirs COMPARE NATURAL ORDER DESCENDING)
            list(GET _qt_dirs 0 _qt_auto)
            message(STATUS "DsQt: Auto-discovered Qt at ${_qt_auto}")
            list(APPEND CMAKE_PREFIX_PATH "${_qt_auto}")
        else()
            message(WARNING "DsQt: No Qt installation found in ${DSQT_QT_ROOT}/. "
                            "Set CMAKE_PREFIX_PATH or Qt6_DIR to your Qt path.")
        endif()
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
        Widgets
)

find_package(tomlplusplus CONFIG REQUIRED)

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

add_library(Dsqt INTERFACE)
add_library(Dsqt::Dsqt ALIAS Dsqt)

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
    # Backing targets keep their short CMake names; only the installed file is
    # prefixed (OUTPUT_NAME, set per module). Plugin targets are renamed
    # outright via PLUGIN_TARGET, because Qt writes the plugin target's name
    # into the generated qmldir and forbids OUTPUT_NAME on it.
    install(TARGETS
        Core
        DsqtCoreplugin
        Bridge
        DsqtBridgeplugin
        Waffles
        DsqtWafflesplugin
        TouchEngine
        DsqtTouchEngineplugin
        Touch
        DsqtTouchplugin

        # Where to put the different kinds of files.
        # DSQT_CONFIG_DIR is "/$<CONFIG>" in the default layout (so Debug and
        # Release can coexist in one prefix) and empty under vcpkg, which keeps
        # the two configurations in separate prefixes already.
        LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}   # .so / .dylib
        ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}   # .a / .lib
        RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}${DSQT_CONFIG_DIR}   # .dll / executables
    )

    # Spout module (Windows only)
    if(WIN32 AND TARGET DsSpout)
        set(_spout_install_targets DsSpout DsqtSpoutplugin)

        install(TARGETS ${_spout_install_targets}
            LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}
            ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}
            RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}${DSQT_CONFIG_DIR}
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
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Core"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Bridge/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Bridge/Bridge.qmltypes"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Bridge"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Waffles/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Waffles/Waffles.qmltypes"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Waffles"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/TouchEngine/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/TouchEngine/TouchEngine.qmltypes"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/TouchEngine"
    )
    install(FILES
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Touch/qmldir"
        "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Touch/Touch.qmltypes"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Touch"
    )

    # Spout QML module metadata (Windows only)
    if(WIN32 AND TARGET DsSpout)
        install(FILES
            "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Spout/qmldir"
            "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Spout/DsSpout.qmltypes"
            DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Spout"
        )
    endif()

    # Install QML source files for each module
    # These are needed by qmlimportscanner which looks for files referenced in qmldir
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Core/qml"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Core"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Bridge/qml"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Bridge"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Waffles/qml"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Waffles"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/TouchEngine/qml"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/TouchEngine"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )
    install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Touch/qml"
        DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Touch"
        OPTIONAL
        FILES_MATCHING PATTERN "*.qml"
    )

    # Spout QML files (Windows only)
    if(WIN32 AND TARGET DsSpout)
        install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/Spout/qml"
            DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/Spout"
            OPTIONAL
            FILES_MATCHING PATTERN "*.qml"
        )
    endif()

    # Create the main Config.cmake file from our template
    # Note: We don't use CMake's export() mechanism because Qt's qt_add_qml_module
    # creates internal OBJECT library dependencies that can't be exported.
    # Instead, DsqtConfig.cmake.in creates imported targets manually.
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DsqtConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/DsqtConfig.cmake"
        INSTALL_DESTINATION ${DSQT_INSTALL_CMAKEDIR}
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
        DESTINATION ${DSQT_INSTALL_CMAKEDIR}
    )

    # ----------------------------------------------------------------------
    # Register install location in package registry (cross-platform)
    # Uses install-time prefix via CMAKE_INSTALL_PREFIX set by cmake --install
    # ----------------------------------------------------------------------
    # Never do this inside a vcpkg port: the package would be registered under
    # vcpkg's private packages/ staging directory, which is wiped and rebuilt on
    # every install, leaving dangling registry entries that shadow the real one.
    if(DSQT_REGISTER_PACKAGE)
        if(WIN32)
            # Windows: Write to registry
            install(CODE "set(_dsqt_cmakedir \"${DSQT_INSTALL_CMAKEDIR}\")")
            install(CODE [[
                set(_config_dir "${CMAKE_INSTALL_PREFIX}/${_dsqt_cmakedir}")
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
            install(CODE "set(_dsqt_cmakedir \"${DSQT_INSTALL_CMAKEDIR}\")")
            install(CODE [[
                set(_config_dir "${CMAKE_INSTALL_PREFIX}/${_dsqt_cmakedir}")
                set(_reg_dir "$ENV{HOME}/.cmake/packages/Dsqt")
                file(MAKE_DIRECTORY "${_reg_dir}")
                string(MD5 _hash "${_config_dir}")
                file(WRITE "${_reg_dir}/${_hash}" "${_config_dir}")
                message(STATUS "Registered Dsqt in CMake package registry: ${_config_dir}")
            ]])
        endif()
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
