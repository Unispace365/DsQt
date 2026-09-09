# BundledTomlplusplus.cmake
# ──────────────────────────────────────────────────────────────────────────────
# Self-contained cmake config for the tomlplusplus copy that ships inside the
# DsQt installation.  Installed by DsQt's cmake install rules alongside the
# regular DsqtConfig.cmake so that consumers do not need vcpkg.
#
# Installed location:
#   <prefix>/lib/cmake/tomlplusplus/tomlplusplusConfig.cmake
# PACKAGE_PREFIX_DIR resolves to <prefix> (three levels up from this file).
# ──────────────────────────────────────────────────────────────────────────────

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT TARGET tomlplusplus::tomlplusplus)
    add_library(tomlplusplus::tomlplusplus UNKNOWN IMPORTED)

    set(_PREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
    set(_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})

    if(WIN32)
        # TOML_SHARED_LIB=1 ensures __declspec(dllimport) is applied on the
        # consumer side, matching the DLL build that DsQt::Core was compiled against.
        # TOML_HEADER_ONLY=0 opts into the compiled (non-inline) translation unit.
        set(_DEFS "TOML_HEADER_ONLY=0;TOML_SHARED_LIB=1")
        set(_LIB_NAME "tomlplusplus")
    else()
        set(_DEFS "TOML_HEADER_ONLY=0")
        set(_LIB_NAME "tomlplusplus")
    endif()

    set_target_properties(tomlplusplus::tomlplusplus PROPERTIES
        # Consumers must see the same toml++ include path DsQt was compiled against.
        INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/include"

        INTERFACE_COMPILE_DEFINITIONS "${_DEFS}"

        IMPORTED_CONFIGURATIONS       "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG          "${PACKAGE_PREFIX_DIR}/lib/Debug/${_PREFIX}${_LIB_NAME}${_SUFFIX}"
        IMPORTED_LOCATION_RELEASE        "${PACKAGE_PREFIX_DIR}/lib/Release/${_PREFIX}${_LIB_NAME}${_SUFFIX}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL     Release
    )

    target_compile_features(tomlplusplus::tomlplusplus INTERFACE cxx_std_17)
endif()

set(tomlplusplus_FOUND   TRUE)
set(tomlplusplus_VERSION "3.4.0")
