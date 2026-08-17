# BundledTomlplusplus.cmake
# ──────────────────────────────────────────────────────────────────────────────
# NOTE: this file is currently UNUSED — nothing installs or includes it. DsQt
# resolves toml++ through find_package(tomlplusplus CONFIG), i.e. vcpkg's own
# config, in both the vcpkg and the build_and_install.bat flows. Keep it only if
# you intend to revive a vcpkg-free installation; otherwise it can be deleted.
#
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

    set_target_properties(tomlplusplus::tomlplusplus PROPERTIES
        # Consumers must see the same toml++ include path DsQt was compiled against.
        INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/include"

        # TOML_HEADER_ONLY=0 opts into the compiled (non-inline) translation unit.
        # TOML_SHARED_LIB is deliberately NOT set: the DsQt triplets link toml++
        # statically, so no __declspec(dllimport) should be applied. Setting it
        # would produce LNK2019s against __imp_ symbols that do not exist.
        INTERFACE_COMPILE_DEFINITIONS "TOML_HEADER_ONLY=0"

        IMPORTED_CONFIGURATIONS       "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG          "${PACKAGE_PREFIX_DIR}/lib/Debug/tomlplusplus.lib"
        IMPORTED_LOCATION_RELEASE        "${PACKAGE_PREFIX_DIR}/lib/Release/tomlplusplus.lib"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL     Release
    )

    target_compile_features(tomlplusplus::tomlplusplus INTERFACE cxx_std_17)
endif()

set(tomlplusplus_FOUND   TRUE)
set(tomlplusplus_VERSION "3.4.0")
