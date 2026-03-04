cmake_minimum_required(VERSION 3.29)

# Spout2 is Windows-only (DirectX 11 shared textures)
if(NOT WIN32)
    return()
endif()

SET(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 REQUIRED COMPONENTS
    GuiPrivate
    ShaderTools
)
qt_standard_project_setup(REQUIRES 6.9)

# ----------------------------------------------------------------------
# Spout2: prefer an installed copy, fall back to fetching from source.
# SpoutGL always creates a "Spout" shared-lib target, which is why our
# own library is named DsSpout (see below).
# ----------------------------------------------------------------------
find_package(Spout2 QUIET)
if(Spout2_FOUND)
    message(STATUS "DsQt/Spout: Using installed Spout2 ${Spout2_VERSION}")
    set(_spout_dx_target Spout2::SpoutDX_static)
else()
    message(STATUS "DsQt/Spout: Spout2 not found locally — fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        Spout2
        GIT_REPOSITORY https://github.com/leadedge/Spout2.git
        GIT_TAG        2.007.017
    )
    set(SPOUT_BUILD_SPOUTDX ON  CACHE BOOL "" FORCE)
    set(SPOUT_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SPOUT_BUILD_CMT     OFF CACHE BOOL "" FORCE)  # Don't override MSVC runtime globally
    set(SKIP_INSTALL_ALL    ON  CACHE BOOL "" FORCE)   # Don't install Spout2 targets
    FetchContent_MakeAvailable(Spout2)
    set(_spout_dx_target SpoutDX_static)
endif()

# ----------------------------------------------------------------------
# Vulkan (optional — enables Vulkan backend for Spout texture import)
# ----------------------------------------------------------------------
find_package(Vulkan QUIET)

# ----------------------------------------------------------------------
# Spout module target
# Named DsSpout to avoid collision with Spout2's own "Spout" target.
# The ALIAS Dsqt::Spout provides the canonical namespace.
# ----------------------------------------------------------------------
add_library(DsSpout STATIC)
add_library(Dsqt::Spout ALIAS DsSpout)

set(SOURCES
    src/dsSpoutReceiver.cpp
    src/dsQmlSpoutReceiverView.cpp
    src/dsSpoutTextureNode.cpp
    src/dsSpoutMaterial.cpp
    src/dsSpoutTextureImporter.cpp
    src/dsSpoutD3D11Importer.cpp
    src/dsSpoutD3D12Importer.cpp
    src/dsSpoutOpenGLImporter.cpp
)

set(HEADERS
    src/dsSpoutReceiver.h
    src/dsQmlSpoutReceiverView.h
    src/dsSpoutTextureNode.h
    src/dsSpoutMaterial.h
    src/dsSpoutTextureImporter.h
    src/dsSpoutD3D11Importer.h
    src/dsSpoutD3D12Importer.h
    src/dsSpoutOpenGLImporter.h
)

if(Vulkan_FOUND)
    list(APPEND SOURCES src/dsSpoutVulkanImporter.cpp)
    list(APPEND HEADERS src/dsSpoutVulkanImporter.h)
endif()

qt_add_qml_module(DsSpout
    URI Dsqt.Spout
    VERSION 1.0
    OUTPUT_TARGETS SpoutQmlTargets
    SOURCES ${SOURCES} ${HEADERS}
    DEPENDENCIES QtQuick
)

qt6_add_shaders(DsSpout "shaders"
    PREFIX "/"
    HLSL 50
    PRECOMPILE
    BATCHABLE
    OUTPUT_TARGETS SpoutShaderTargets
    FILES
        resources/shaders/texture.vert
        resources/shaders/texture.frag
)

set(SpoutTargets ${SpoutQmlTargets} ${SpoutShaderTargets} PARENT_SCOPE)

target_link_libraries(DsSpout
    PUBLIC
        Qt6::Core
        Qt6::Quick
        Qt6::Gui
        Qt6::GuiPrivate
        DsSpoutplugin
        ${_spout_dx_target}
        d3d11
        d3d12
        dxgi
        opengl32
)

if(Vulkan_FOUND)
    target_link_libraries(DsSpout PUBLIC Vulkan::Vulkan)
    target_compile_definitions(DsSpout PRIVATE DSSPOUT_VULKAN_ENABLED)
endif()

target_include_directories(DsSpout
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if(Spout2_FOUND)
    # vcpkg installs SpoutDX headers under include/SpoutDX/ — add the subdirectory
    get_target_property(_spout_include_dir Spout2::SpoutDX_static INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(DsSpout PRIVATE "${_spout_include_dir}/SpoutDX")
else()
    target_include_directories(DsSpout PRIVATE
        $<BUILD_INTERFACE:${spout2_SOURCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX>
    )
endif()

target_compile_definitions(DsSpout PRIVATE NOMINMAX)

set_target_properties(DsSpout PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# ----------------------------------------------------------------------
# Installation rules (headers only - target install is in main CMakeLists.txt)
# ----------------------------------------------------------------------
install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)
