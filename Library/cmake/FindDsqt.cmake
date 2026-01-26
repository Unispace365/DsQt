# FindDsqt.cmake
# ────────────────────────────────────────────────────────────────
# Find module for Dsqt library with component support.
#
# Supports components: Core, Bridge, Waffles, TouchEngine
#
# Usage:
#   find_package(Dsqt REQUIRED COMPONENTS Core Bridge TouchEngine)
#   target_link_libraries(myapp PRIVATE Dsqt::Core Dsqt::Bridge Dsqt::TouchEngine)
#
# This module uses the DS_QT_PLATFORM_100 environment variable to locate the library.
# ────────────────────────────────────────────────────────────────

include(FindPackageHandleStandardArgs)

# Get the platform directory from environment
file(TO_CMAKE_PATH "$ENV{DS_QT_PLATFORM_100}" _Dsqt_PLATFORM_DIR)

if(NOT _Dsqt_PLATFORM_DIR)
    message(FATAL_ERROR "DS_QT_PLATFORM_100 environment variable not set")
endif()

# Define base paths
set(_Dsqt_LIBRARY_DIR "${_Dsqt_PLATFORM_DIR}/Library")
set(_Dsqt_SOURCE_DIR "${_Dsqt_LIBRARY_DIR}/Dsqt")

# Possible build directories (Qt Creator and CLI builds)
# Glob for Qt Creator build directories
file(GLOB _Dsqt_QTC_BUILD_DIRS "${_Dsqt_LIBRARY_DIR}/build/Desktop_Qt_*")

set(_Dsqt_BUILD_DIRS
    "${_Dsqt_LIBRARY_DIR}/build/debug"
    "${_Dsqt_LIBRARY_DIR}/build/release"
    "${_Dsqt_LIBRARY_DIR}/build-cli/debug"
    "${_Dsqt_LIBRARY_DIR}/build-cli/release"
    "${_Dsqt_LIBRARY_DIR}/build"
    ${_Dsqt_QTC_BUILD_DIRS}
)

# Define known components and their properties
set(_Dsqt_KNOWN_COMPONENTS Core Bridge Waffles TouchEngine)

# Function to find and create an imported target for a component
function(_dsqt_find_component COMPONENT)
    set(_comp_lower "${COMPONENT}")
    string(TOLOWER "${COMPONENT}" _comp_lower_name)

    # Source directory for headers
    set(_header_dir "${_Dsqt_SOURCE_DIR}/${COMPONENT}")

    # Build subdirectory path
    set(_build_subdir "Dsqt/${COMPONENT}")

    # Build search paths for this component
    set(_lib_search_paths "")
    set(_plugin_search_paths "")
    foreach(_build_dir IN LISTS _Dsqt_BUILD_DIRS)
        list(APPEND _lib_search_paths "${_build_dir}/${_build_subdir}")
        list(APPEND _lib_search_paths "${_build_dir}")
        list(APPEND _plugin_search_paths "${_build_dir}/${_build_subdir}")
    endforeach()

    # Find the main library
    find_library(Dsqt_${COMPONENT}_LIBRARY
        NAMES ${COMPONENT}
        PATHS ${_lib_search_paths}
        NO_DEFAULT_PATH
    )

    # Find the plugin library
    find_library(Dsqt_${COMPONENT}_PLUGIN_LIBRARY
        NAMES ${COMPONENT}plugin
        PATHS ${_plugin_search_paths}
        NO_DEFAULT_PATH
    )

    # Check if header directory exists
    if(EXISTS "${_header_dir}")
        set(Dsqt_${COMPONENT}_INCLUDE_DIR "${_header_dir}" CACHE PATH "Dsqt ${COMPONENT} include directory")
    endif()

    # Determine if component was found
    if(Dsqt_${COMPONENT}_LIBRARY AND Dsqt_${COMPONENT}_INCLUDE_DIR)
        set(Dsqt_${COMPONENT}_FOUND TRUE PARENT_SCOPE)

        # Create imported target for the component if it doesn't exist
        if(NOT TARGET Dsqt::${COMPONENT})
            add_library(Dsqt::${COMPONENT} STATIC IMPORTED)
            set_target_properties(Dsqt::${COMPONENT} PROPERTIES
                IMPORTED_LOCATION "${Dsqt_${COMPONENT}_LIBRARY}"
            )

            # Collect include directories
            set(_include_dirs "${Dsqt_${COMPONENT}_INCLUDE_DIR}")

            # Add subdirectories that exist
            foreach(_subdir IN ITEMS core model network settings ui utility bridge src)
                if(EXISTS "${Dsqt_${COMPONENT}_INCLUDE_DIR}/${_subdir}")
                    list(APPEND _include_dirs "${Dsqt_${COMPONENT}_INCLUDE_DIR}/${_subdir}")
                endif()
            endforeach()

            set_target_properties(Dsqt::${COMPONENT} PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${_include_dirs}"
            )

            # Link plugin if found
            if(Dsqt_${COMPONENT}_PLUGIN_LIBRARY)
                if(NOT TARGET Dsqt::${COMPONENT}plugin)
                    add_library(Dsqt::${COMPONENT}plugin STATIC IMPORTED)
                    set_target_properties(Dsqt::${COMPONENT}plugin PROPERTIES
                        IMPORTED_LOCATION "${Dsqt_${COMPONENT}_PLUGIN_LIBRARY}"
                    )
                endif()
                set_property(TARGET Dsqt::${COMPONENT} APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES Dsqt::${COMPONENT}plugin
                )
            endif()
        endif()
    else()
        set(Dsqt_${COMPONENT}_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

# Process requested components
set(_Dsqt_REQUIRED_VARS "")
set(_Dsqt_FOUND_COMPONENTS "")
set(_Dsqt_MISSING_COMPONENTS "")

foreach(_comp IN LISTS Dsqt_FIND_COMPONENTS)
    if(NOT _comp IN_LIST _Dsqt_KNOWN_COMPONENTS)
        message(WARNING "Unknown Dsqt component: ${_comp}")
        set(Dsqt_${_comp}_FOUND FALSE)
        list(APPEND _Dsqt_MISSING_COMPONENTS ${_comp})
        continue()
    endif()

    _dsqt_find_component(${_comp})

    if(Dsqt_${_comp}_FOUND)
        list(APPEND _Dsqt_FOUND_COMPONENTS ${_comp})
    else()
        list(APPEND _Dsqt_MISSING_COMPONENTS ${_comp})
    endif()
endforeach()

# If no components specified, try to find all
if(NOT Dsqt_FIND_COMPONENTS)
    foreach(_comp IN LISTS _Dsqt_KNOWN_COMPONENTS)
        _dsqt_find_component(${_comp})
        if(Dsqt_${_comp}_FOUND)
            list(APPEND _Dsqt_FOUND_COMPONENTS ${_comp})
        endif()
    endforeach()
endif()

# Use standard args to handle REQUIRED, QUIET, etc.
find_package_handle_standard_args(Dsqt
    REQUIRED_VARS _Dsqt_PLATFORM_DIR
    HANDLE_COMPONENTS
)

# Set output variables
set(Dsqt_COMPONENTS ${_Dsqt_FOUND_COMPONENTS})
set(Dsqt_INCLUDES "")
foreach(_comp IN LISTS _Dsqt_FOUND_COMPONENTS)
    if(Dsqt_${_comp}_INCLUDE_DIR)
        list(APPEND Dsqt_INCLUDES "${Dsqt_${_comp}_INCLUDE_DIR}")
    endif()
endforeach()
