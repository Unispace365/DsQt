cmake_minimum_required(VERSION 3.29)

# Read version from VERSION.txt (source of truth)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" DSQT_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${DSQT_VERSION_STRING}" DSQT_VERSION_STRING)

project(Dsqt VERSION ${DSQT_VERSION_STRING} LANGUAGES CXX)

set(CMAKE_AUTORCC ON)

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
          "DsQt"
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

# Module metadata is shared by the build, package publisher and ProjectCloner.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/dsqt-modules.json" DSQT_MODULE_METADATA)
set(DSQT_ALL_MODULES Core Bridge Waffles Qsys Spout Touch TouchEngine)
set(DSQT_ENABLED_MODULES Core)
foreach(_module IN LISTS DSQT_ALL_MODULES)
    if(_module STREQUAL "Core")
        continue()
    endif()
    option(DSQT_BUILD_${_module} "Build the ${_module} module" ON)
    if(DSQT_BUILD_${_module})
        if(NOT WIN32 AND (_module STREQUAL "Spout" OR _module STREQUAL "TouchEngine"))
            continue()
        endif()
        list(APPEND DSQT_ENABLED_MODULES ${_module})
    endif()
endforeach()
# Resolve dependencies in metadata order until the set is stable.
foreach(_pass RANGE 0 6)
    foreach(_module IN LISTS DSQT_ENABLED_MODULES)
        string(JSON _count LENGTH "${DSQT_MODULE_METADATA}" modules ${_module} requires)
        if(_count GREATER 0)
            math(EXPR _last "${_count} - 1")
            foreach(_index RANGE 0 ${_last})
                string(JSON _dependency GET "${DSQT_MODULE_METADATA}" modules ${_module} requires ${_index})
                list(APPEND DSQT_ENABLED_MODULES ${_dependency})
            endforeach()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES DSQT_ENABLED_MODULES)
endforeach()
set(DSQT_REQUIRED_QT_COMPONENTS "")
string(JSON DSQT_MINIMUM_QT GET "${DSQT_MODULE_METADATA}" minimumQt)
foreach(_module IN LISTS DSQT_ENABLED_MODULES)
    string(JSON _minimum GET "${DSQT_MODULE_METADATA}" modules ${_module} minimumQt)
    if(_minimum VERSION_GREATER DSQT_MINIMUM_QT)
        set(DSQT_MINIMUM_QT "${_minimum}")
    endif()
    string(JSON _count LENGTH "${DSQT_MODULE_METADATA}" modules ${_module} qtComponents)
    if(_count GREATER 0)
        math(EXPR _last "${_count} - 1")
        foreach(_index RANGE 0 ${_last})
            string(JSON _qt_component GET "${DSQT_MODULE_METADATA}" modules ${_module} qtComponents ${_index})
            list(APPEND DSQT_REQUIRED_QT_COMPONENTS "${_qt_component}")
        endforeach()
    endif()
endforeach()
list(REMOVE_DUPLICATES DSQT_REQUIRED_QT_COMPONENTS)
message(STATUS "DsQt modules: ${DSQT_ENABLED_MODULES}; minimum Qt: ${DSQT_MINIMUM_QT}")
find_package(Qt6 ${DSQT_MINIMUM_QT} REQUIRED COMPONENTS ${DSQT_REQUIRED_QT_COMPONENTS})
qt_standard_project_setup(REQUIRES 6.12)

if(DSQT_QT_VERSION AND NOT Qt6_VERSION VERSION_EQUAL DSQT_QT_VERSION)
    message(FATAL_ERROR
        "DsQt: triplet pins Qt ${DSQT_QT_VERSION}, but CMake selected Qt ${Qt6_VERSION}. "
        "Set Qt6_DIR or CMAKE_PREFIX_PATH to the pinned kit before building this package.")
endif()

find_package(tomlplusplus CONFIG REQUIRED)

option(DSQT_BUILD_TESTS "Build and run DsQt unit tests" OFF)
if(DSQT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)
endif()

foreach(_module Core Bridge Waffles TouchEngine Spout Touch Qsys)
    if(_module IN_LIST DSQT_ENABLED_MODULES)
        add_subdirectory("DsQt/${_module}")
    endif()
endforeach()
add_library(Dsqt INTERFACE)
add_library(Dsqt::Dsqt ALIAS Dsqt)
foreach(_module IN LISTS DSQT_ENABLED_MODULES)
    target_link_libraries(Dsqt INTERFACE Dsqt::${_module})
endforeach()

if(DSQT_BUILD_TESTS)
    add_subdirectory(Tests/Settings)
    add_subdirectory(Tests/Logging)
    if("Bridge" IN_LIST DSQT_ENABLED_MODULES)
        add_subdirectory(Tests/BridgeTest)
    endif()
    if("Waffles" IN_LIST DSQT_ENABLED_MODULES)
        add_subdirectory(Tests/Whiteboard)
        add_subdirectory(Tests/Playlist)
        add_subdirectory(Tests/ContentState)
        add_subdirectory(Tests/Capture)
    endif()
    if("Qsys" IN_LIST DSQT_ENABLED_MODULES)
        add_subdirectory(Tests/Qsys)
    endif()
    add_subdirectory(Tests/Effects)
    if("TouchEngine" IN_LIST DSQT_ENABLED_MODULES)
        add_subdirectory(Tests/TouchEngine)
    endif()

    # Make tests independent of the calling shell: Qt DLLs are not deployed next
    # to the test executables, and an inherited QT_QUICK_BACKEND=software (etc.)
    # breaks the pixel checks of the Effects and Whiteboard tests.
    set(_dsqt_test_isolated_vars
        QT_QPA_PLATFORM QT_QUICK_BACKEND QSG_RHI_BACKEND QT_IM_MODULE QT_QUICK_CONTROLS_STYLE)
    function(_dsqt_tests_isolate_environment _dir)
        if(_dir MATCHES "/Tests/")
            get_property(_tests DIRECTORY "${_dir}" PROPERTY TESTS)
            foreach(_test IN LISTS _tests)
                get_property(_env TEST "${_test}" DIRECTORY "${_dir}" PROPERTY ENVIRONMENT)
                set(_mods "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
                foreach(_var IN LISTS _dsqt_test_isolated_vars)
                    # Applied after ENVIRONMENT, so keep what a test sets itself.
                    if(NOT _env MATCHES "(^|;)${_var}=")
                        list(APPEND _mods "${_var}=unset:")
                    endif()
                endforeach()
                set_property(TEST "${_test}" DIRECTORY "${_dir}" APPEND PROPERTY
                    ENVIRONMENT_MODIFICATION ${_mods})
            endforeach()
        endif()
        get_property(_subdirs DIRECTORY "${_dir}" PROPERTY SUBDIRECTORIES)
        foreach(_subdir IN LISTS _subdirs)
            _dsqt_tests_isolate_environment("${_subdir}")
        endforeach()
    endfunction()
    _dsqt_tests_isolate_environment("${CMAKE_CURRENT_SOURCE_DIR}")
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
    # Most modules use manually imported archives. TouchEngine also exports
    # its Qt-generated resource objects and static plugin initializer.
    # Backing targets keep their short CMake names; only the installed file is
    # prefixed (OUTPUT_NAME, set per module). Plugin targets are renamed
    # outright via PLUGIN_TARGET, because Qt writes the plugin target's name
    # into the generated qmldir and forbids OUTPUT_NAME on it.
    foreach(_module IN LISTS DSQT_ENABLED_MODULES)
        set(_target "${_module}")
        set(_qmltypes "${_module}")
        if(_module STREQUAL "Spout")
            set(_target DsSpout)
            set(_qmltypes DsSpout)
        endif()
        if(_module STREQUAL "TouchEngine")
            install(TARGETS TouchEngine DsqtTouchEngineplugin ${TouchEngineTargets}
                EXPORT DsqtTouchEngineTargets
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}${DSQT_CONFIG_DIR}
                OBJECTS DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}/objects)
            install(EXPORT DsqtTouchEngineTargets
                FILE DsqtTouchEngineTargets.cmake
                NAMESPACE Dsqt::
                DESTINATION ${DSQT_INSTALL_CMAKEDIR})
        else()
            install(TARGETS ${_target} Dsqt${_module}plugin
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}${DSQT_CONFIG_DIR}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}${DSQT_CONFIG_DIR})
        endif()
        install(FILES
            "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/${_module}/qmldir"
            "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/${_module}/${_qmltypes}.qmltypes"
            DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/${_module}")
        install(DIRECTORY "${QT_QML_OUTPUT_DIRECTORY}/Dsqt/${_module}/qml"
            DESTINATION "${DSQT_INSTALL_QMLDIR}/Dsqt/${_module}"
            OPTIONAL FILES_MATCHING PATTERN "*.qml")
    endforeach()
    string(JSON _installed_metadata SET "${DSQT_MODULE_METADATA}" builtModules "[]")
    set(_index 0)
    foreach(_module IN LISTS DSQT_ENABLED_MODULES)
        string(JSON _installed_metadata SET "${_installed_metadata}" builtModules ${_index} "\"${_module}\"")
        math(EXPR _index "${_index} + 1")
    endforeach()
    string(JSON _installed_metadata SET "${_installed_metadata}" builtQtVersion "\"${Qt6_VERSION}\"")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dsqt-modules.json" "${_installed_metadata}\n")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dsqt-modules.json" DESTINATION ${DSQT_INSTALL_CMAKEDIR})

    # Create the main Config.cmake file from our template
    # The config creates the archive imports and loads TouchEngine's exported
    # resource graph only when that module is requested.
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DsQtConfig.cmake.in"
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
