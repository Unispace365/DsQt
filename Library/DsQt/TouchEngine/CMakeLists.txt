cmake_minimum_required(VERSION 3.29)

if(NOT WIN32)
    message(FATAL_ERROR "Dsqt.TouchEngine currently targets TouchEngine-Windows; Metal is intentionally unsupported")
endif()

set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS
    Core
    Gui
    GuiPrivate
    Qml
    Quick
    ShaderTools
)
find_package(Vulkan REQUIRED)

qt_standard_project_setup(REQUIRES 6.9)
include(GNUInstallDirs)

set(TOUCHENGINE_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sdk" CACHE PATH
    "Root of a TouchEngine 3.3 Windows SDK")
set(TOUCHENGINE_INCLUDE_DIR "${TOUCHENGINE_SDK_ROOT}/include")
set(TOUCHENGINE_LIBRARY "${TOUCHENGINE_SDK_ROOT}/lib/TouchEngine.lib")
set(TOUCHENGINE_RUNTIME "${TOUCHENGINE_SDK_ROOT}/bin/TouchEngine.dll")

foreach(_required_file
        "${TOUCHENGINE_INCLUDE_DIR}/TouchEngine/TEInstance.h"
        "${TOUCHENGINE_LIBRARY}"
        "${TOUCHENGINE_RUNTIME}")
    if(NOT EXISTS "${_required_file}")
        message(FATAL_ERROR "The TouchEngine 3.3 SDK is incomplete: ${_required_file} is missing")
    endif()
endforeach()

# TEInstanceLinkGetChoices was introduced with the 3.3 SDK. Refuse mixed or
# stale header/lib/DLL payloads at configure time instead of failing at runtime.
file(READ "${TOUCHENGINE_INCLUDE_DIR}/TouchEngine/TEInstance.h" _te_instance_header)
if(NOT _te_instance_header MATCHES "TEInstanceLinkGetChoices")
    message(FATAL_ERROR "Dsqt.TouchEngine requires TouchEngine SDK 3.3 or newer")
endif()
unset(_te_instance_header)

add_library(DsqtTouchEngineSDK SHARED IMPORTED GLOBAL)
set_target_properties(DsqtTouchEngineSDK PROPERTIES
    IMPORTED_LOCATION "${TOUCHENGINE_RUNTIME}"
    IMPORTED_IMPLIB "${TOUCHENGINE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${TOUCHENGINE_INCLUDE_DIR}"
)

add_library(TouchEngine STATIC)
add_library(Dsqt::TouchEngine ALIAS TouchEngine)

set(_touchengine_public_headers
    include/Dsqt/TouchEngine/DsTouchEngineSession.h
    include/Dsqt/TouchEngine/DsTouchEngineTypes.h
    include/Dsqt/TouchEngine/DsTouchEngineView.h
)

set(_touchengine_sources
    src/DsTouchEngineSession.cpp
    src/DsTouchEngineTypes.cpp
    src/DsTouchEngineView.cpp
    src/private/DsTouchEngineSessionPrivate_p.h
    src/private/DsTouchEngineViewRenderer.cpp
    src/private/DsTouchEngineViewRenderer_p.h
    src/private/TouchEngineCore.cpp
    src/private/TouchEngineCore_p.h
    src/private/TouchEngineRhiBackend.cpp
    src/private/TouchEngineRhiBackend_p.h
    src/private/TouchEngineSharedState_p.h
    src/private/d3d/D3D11TouchEngineBackend.cpp
    src/private/d3d/D3D11TouchEngineBackend_p.h
    src/private/d3d/D3D12TouchEngineBackend.cpp
    src/private/d3d/D3D12TouchEngineBackend_p.h
    src/private/opengl/D3D11NvInteropBridge.cpp
    src/private/opengl/D3D11NvInteropBridge_p.h
    src/private/opengl/OpenGLTouchEngineBackend.cpp
    src/private/opengl/OpenGLTouchEngineBackend_p.h
    src/private/vulkan/VulkanTouchEngineBackend.cpp
    src/private/vulkan/VulkanTouchEngineBackend_p.h
)

qt_add_qml_module(TouchEngine
    URI Dsqt.TouchEngine
    VERSION 1.0
    OUTPUT_TARGETS TouchEngineQmlTargets
    QML_FILES
        qml/DsTouchEngineTextureView.qml
    SOURCES
        ${_touchengine_public_headers}
        ${_touchengine_sources}
    DEPENDENCIES
        QtQuick
)

qt_add_shaders(TouchEngine "dsqt_touchengine_shaders"
    PREFIX "/dsqt/touchengine/shaders"
    BASE "resources/shaders"
    HLSL 50
    PRECOMPILE
    BATCHABLE
    OUTPUT_TARGETS TouchEngineShaderTargets
    FILES
        resources/shaders/texture.vert
        resources/shaders/texture.frag
)

set(TouchEngineTargets
    ${TouchEngineQmlTargets}
    ${TouchEngineShaderTargets}
    PARENT_SCOPE
)

target_include_directories(TouchEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Dsqt/TouchEngine"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/private"
)

target_link_libraries(TouchEngine
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    PRIVATE
        Qt6::GuiPrivate
        DsqtTouchEngineSDK
        Vulkan::Vulkan
        d3d11
        d3d12
        dxgi
        opengl32
        version
)

target_compile_definitions(TouchEngine PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    VK_USE_PLATFORM_WIN32_KHR
)

add_custom_command(TARGET TouchEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TOUCHENGINE_RUNTIME}"
        "$<TARGET_FILE_DIR:TouchEngine>"
    VERBATIM
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)
install(FILES "${TOUCHENGINE_RUNTIME}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/$<CONFIG>"
)
install(FILES "${TOUCHENGINE_LIBRARY}"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/$<CONFIG>"
    RENAME TouchEngineSDK.lib
)
install(FILES
    "${TOUCHENGINE_SDK_ROOT}/LICENSE.md"
    "${TOUCHENGINE_SDK_ROOT}/CHANGES.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/Dsqt/TouchEngine"
)
