# ─────────────────────────────────────────────────────────────────────────────
# dsqt — source port
#
# DsQt links against the consumer's own Qt kit, so there is no way to ship
# prebuilt binaries that are safe to reuse. This port always builds from
# source; vcpkg's binary cache then makes sure any given
# (library version × compiler × Qt kit) combination is only ever built once.
#
# This file is generated from Library/vcpkg/port/portfile.cmake in the DsQt
# repository. Do not edit it in the registry — edit it there and let the
# "Publish vcpkg port" workflow regenerate it.
# ─────────────────────────────────────────────────────────────────────────────

# DsQt's modules are static libraries whose consumers get imported STATIC
# targets from DsqtConfig.cmake. Building them as DLLs would not match, so the
# linkage is fixed here regardless of what the triplet asks for.
set(VCPKG_LIBRARY_LINKAGE static)

# The TouchEngine SDK ships as a prebuilt DLL that DsQt redistributes, so this
# package contains a DLL despite being a static-linkage port. That combination
# is rejected by default.
set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)

# ─────────────────────────────────────────────────────────────────────────────
# Qt kit selection
# ─────────────────────────────────────────────────────────────────────────────
# vcpkg's ABI hash — the key under which a build gets stored in and retrieved
# from the binary cache — covers the port files, the compiler binary, and the
# *text of the triplet file*. It knows nothing about a Qt installed outside
# vcpkg. So the Qt kit is declared in the triplet, which folds it into the hash
# and gives every kit its own cache entry.
#
# Without this, switching from Qt 6.9 to 6.10 would silently hand you the 6.9
# binaries out of the cache, so an unset value is a hard error rather than a
# guess.
if(NOT DEFINED DSQT_QT_VERSION OR DSQT_QT_VERSION STREQUAL "")
    message(FATAL_ERROR
        "dsqt: the triplet '${TARGET_TRIPLET}' does not set DSQT_QT_VERSION.\n"
        "\n"
        "DsQt is compiled against your own Qt kit, and vcpkg's binary cache is "
        "keyed on the triplet's contents. A triplet without a pinned Qt version "
        "would let binaries built against one Qt kit be reused for another.\n"
        "\n"
        "Use one of the DsQt triplets (overlay-triplets in vcpkg-configuration.json), "
        "or add to your own triplet file:\n"
        "    set(DSQT_QT_VERSION \"6.10.2\")\n")
endif()

if(NOT DEFINED DSQT_QT_ROOT OR DSQT_QT_ROOT STREQUAL "")
    set(DSQT_QT_ROOT "C:/Qt")
endif()
if(NOT DEFINED DSQT_QT_ARCH OR DSQT_QT_ARCH STREQUAL "")
    set(DSQT_QT_ARCH "msvc2022_64")
endif()

message(STATUS "dsqt: building against Qt ${DSQT_QT_VERSION} (${DSQT_QT_ARCH}) from ${DSQT_QT_ROOT}")

# ─────────────────────────────────────────────────────────────────────────────
# Source
# ─────────────────────────────────────────────────────────────────────────────
# DsQt is a private repository. vcpkg_from_git shells out to git, which picks up
# whatever credentials the machine already uses to clone it (Git Credential
# Manager on Windows, or GIT_ASKPASS / a PAT in CI). No extra setup needed for
# anyone who can already clone DsQt.
#
# REF is a full commit SHA, written by the publish workflow.
#
# LFS is required, not optional. DsQt tracks *.svg, *.png, *.jpg and friends
# through Git LFS (see .gitattributes). vcpkg_from_git produces its source
# tarball with `git archive`, which runs the working-tree conversion filters --
# and git-lfs installs filter.lfs.required=true globally. Without an LFS fetch
# first, `git archive` aborts with exit 128 instead of quietly writing pointer
# files. Passing LFS with no value makes vcpkg reuse URL as the LFS endpoint.
#
# Consequence: consumers need git-lfs on PATH. Anyone who can already clone
# DsQt has it.
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL "https://github.com/Unispace365/DsQt.git"
    REF "0000000000000000000000000000000000000000"  # DSQT_REF
    HEAD_REF "ph/develop"
    LFS
)

# ─────────────────────────────────────────────────────────────────────────────
# Build
# ─────────────────────────────────────────────────────────────────────────────
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/Library"
    OPTIONS
        # vcpkg builds one configuration per prefix, so drop the per-config
        # lib/Debug + lib/Release subdirectories DsQt uses by default.
        -DDSQT_INSTALL_CONFIG_SUBDIR=OFF
        -DDSQT_INSTALL_CMAKEDIR=share/dsqt
        -DDSQT_INSTALL_QMLDIR=share/dsqt/qml
        # Registering an install prefix in the user's CMake package registry
        # would point at vcpkg's throwaway staging directory.
        -DDSQT_REGISTER_PACKAGE=OFF
        # spout2 is a real dependency here and installs SpoutDX_static.lib into
        # the shared prefix itself; bundling a second copy is a file conflict.
        -DDSQT_BUNDLE_DEPENDENCIES=OFF
        -DDSQT_BUILD_TESTS=OFF
        -DDSQT_BUILD_TOOLS=OFF
        -DBUILD_DOCUMENTATION=OFF
        # Qt kit, pinned by the triplet (see above).
        "-DDSQT_QT_VERSION=${DSQT_QT_VERSION}"
        "-DDSQT_QT_ROOT=${DSQT_QT_ROOT}"
        "-DDSQT_QT_ARCH=${DSQT_QT_ARCH}"
    MAYBE_UNUSED_VARIABLES
        DSQT_QT_ROOT
        DSQT_QT_ARCH
        DSQT_QT_VERSION
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME dsqt CONFIG_PATH share/dsqt)

# ─────────────────────────────────────────────────────────────────────────────
# Tidy up
# ─────────────────────────────────────────────────────────────────────────────
# Headers and the QML module metadata are configuration-independent; only the
# release copy is kept.
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

# The per-module header installs use
#     install(DIRECTORY . DESTINATION include FILES_MATCHING PATTERN "*.h")
# and CMake recreates the whole source tree under include/ even where nothing
# matched, leaving dozens of empty directories (qml/, res/, doc/, shaders...).
# vcpkg rejects those, since several binary cache backends cannot represent an
# empty directory. Prune them generically rather than maintaining a hardcoded
# list; repeat until stable so nested empties collapse too.
set(_dsqt_pruned TRUE)
while(_dsqt_pruned)
    set(_dsqt_pruned FALSE)
    file(GLOB_RECURSE _dsqt_dirs LIST_DIRECTORIES true "${CURRENT_PACKAGES_DIR}/include/*")
    foreach(_dsqt_dir IN LISTS _dsqt_dirs)
        if(IS_DIRECTORY "${_dsqt_dir}")
            file(GLOB _dsqt_children "${_dsqt_dir}/*")
            if(NOT _dsqt_children)
                file(REMOVE_RECURSE "${_dsqt_dir}")
                set(_dsqt_pruned TRUE)
            endif()
        endif()
    endforeach()
endwhile()

vcpkg_copy_pdbs()

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

# DsQt is proprietary and carries no LICENSE file, so the copyright notice is
# written directly rather than copied out of the source tree.
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright"
"Copyright (c) Downstream / Unispace365. All rights reserved.

Proprietary and confidential. Internal use only; not for redistribution.

Bundled third-party components retain their own licences:
  - TouchEngine SDK (Derivative) — see the TouchEngine SDK licence.
  - Spout2 (BSD-2-Clause), tomlplusplus (MIT) — supplied via vcpkg.
")
