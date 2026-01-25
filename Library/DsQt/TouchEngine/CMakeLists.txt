cmake_minimum_required(VERSION  3.29)


SET(QT_NO_PRIVATE_MODULE_WARNING ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    # Concurrent
    # Core
    # Gui
    # Qml
    # Quick
    GuiPrivate 
    ShaderTools
    QuickControls2
)
qt_standard_project_setup(REQUIRES 6.9)

add_library(TouchEngine)
#set qml import path. This is used by Qt Creator to find the QML files.
# set(QML_BIN_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
# message("TOUCHE ENGINE PLACEMENT: ${QML_BIN_ROOT}")
# set(QML_TMP_PATH
#      "${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_SOURCE_DIR}\\qml;${QML_BIN_ROOT};${QML_BIN_ROOT}\\qml;${QML_IMPORT_PATH}"
# )

# #set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/DEPLOY/")

# list(REMOVE_DUPLICATES QML_TMP_PATH)

# set(QML_IMPORT_PATH
#      "${QML_TMP_PATH}"
#      CACHE PATH "QT creator qml import path" FORCE
#  )


# TouchEngine SDK paths - adjust as needed
set(TOUCHENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TouchEngine/include" CACHE PATH "TouchEngine include directory")
set(TOUCHENGINE_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TouchEngine/lib" CACHE PATH "TouchEngine library directory")
set(TOUCHENGINE_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TouchEngine/bin" CACHE PATH "TouchEngine library directory")

# Create an imported target for the TouchEngine SDK to avoid name conflict with our module
add_library(TouchEngineSDK SHARED IMPORTED)
set_target_properties(TouchEngineSDK PROPERTIES
    IMPORTED_LOCATION "${TOUCHENGINE_BIN_DIR}/TouchEngine.dll"
    IMPORTED_IMPLIB "${TOUCHENGINE_LIB_DIR}/TouchEngine.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${TOUCHENGINE_INCLUDE_DIR}"
)

# Find Vulkan SDK
find_package(Vulkan)

# Source files
set(SOURCES
    src/dsQmlTouchEngineManager.cpp
    src/dsTouchEngineTextureNode.cpp
    src/dsQmlTouchEngineTextureOutputView.cpp
    src/OpenGLImage.cpp
    src/OpenGlRenderer.cpp
    src/OpenGLTexture.cpp
    src/Renderer.cpp
    src/dsTouchEngineMaterial.cpp
    src/dsQmlTouchEngineInstance.cpp
)

# D3D12 and Vulkan renderer sources (Windows only)
if(WIN32)
    list(APPEND SOURCES
        src/D3D12Texture.cpp
        src/D3D12Image.cpp
        src/D3D12Renderer.cpp
        src/VulkanTexture.cpp
        src/VulkanImage.cpp
        src/VulkanRenderer.cpp
    )
endif()


set(HEADERS

    src/dsQmlTouchEngineManager.h
    src/dsTouchEngineTextureNode.h
    src/dsQmlTouchEngineTextureOutputView.h
    src/platform_headers.h
    src/OpenGLImage.h
    src/OpenGLRenderer.h
    src/OpenGLTexture.h
    src/Renderer.h
    src/dsTouchEngineMaterial.h
    src/dsQmlTouchEngineInstance.h
)

# D3D12 and Vulkan renderer headers (Windows only)
if(WIN32)
    list(APPEND HEADERS
        src/D3D12Texture.h
        src/D3D12Image.h
        src/D3D12Renderer.h
        src/VulkanTexture.h
        src/VulkanImage.h
        src/VulkanRenderer.h
    )
endif()

set(QML_FILES

    qml/DsTouchEngineTextureView.qml
    qml/InstanceControl.qml
    #qml/debug/DsTENumericInputView.qml
)

set(TOUCHENGINE_INPUT_SOURCES
    src/dsQmlTouchEngineInputBase.h
    src/dsQmlTouchEngineInputBase.cpp
    src/dsQmlTouchEngineStringInput.h
    src/dsQmlTouchEngineStringInput.cpp
    src/dsQmlTouchEngineTableInput.h
    src/dsQmlTouchEngineTableInput.cpp
    src/dsQmlTouchEngineNumericInput.h
    src/dsQmlTouchEngineNumericInput.cpp
    src/dsQmlTouchEngineFloatBufferInput.h
    src/dsQmlTouchEngineFloatBufferInput.cpp
    src/dsQmlTouchEngineTextureInput.h
    src/dsQmlTouchEngineTextureInput.cpp
)

set(TOUCHENGINE_OUTPUT_SOURCES
    src/dsQmlTouchEngineOutputBase.h
    src/dsQmlTouchEngineOutputBase.cpp
    src/dsQmlTouchEngineStringOutput.h
    src/dsQmlTouchEngineStringOutput.cpp
    src/dsQmlTouchEngineNumericOutput.h
    src/dsQmlTouchEngineNumericOutput.cpp
    src/dsQmlTouchEngineTableOutput.h
    src/dsQmlTouchEngineTableOutput.cpp
    src/dsQmlTouchEngineFloatBufferOutput.h
    src/dsQmlTouchEngineFloatBufferOutput.cpp
)

qt_add_qml_module(TouchEngine
    URI DsQt.TouchEngine
    VERSION 1.0
    OUTPUT_TARGETS TouchEngineTargets
    QML_FILES ${QML_FILES}
    SOURCES ${SOURCES} ${HEADERS} ${TOUCHENGINE_INPUT_SOURCES} ${TOUCHENGINE_OUTPUT_SOURCES}
    DEPENDENCIES QtQuick
)



qt6_add_shaders(TouchEngine "shaders"
    PREFIX "/"
    HLSL 50
    PRECOMPILE
    BATCHABLE
    OUTPUT_TARGETS TouchEngineShaderTargets
    FILES
        resources/shaders/texture.vert
        resources/shaders/texture.frag
)

set(TouchEngineTargets ${TouchEngineTargets} ${TouchEngineShaderTargets} PARENT_SCOPE)

#qt_add_resources(TouchEngineQt "resources"
#    PREFIX "/"
#    FILES
#        resources/TD/Testbed.tox
#)

# Optional: Add shaders if they exist
#if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/texture.vert.qsb")
#    target_sources(TouchEngineQt PRIVATE
#        resources/shaders/texture.vert.qsb
#        resources/shaders/texture.frag.qsb
#    )
#    message(STATUS "Including compiled shader files")
#else()
#    message(STATUS "Shader files not found - will use Qt's default rendering")
#    message(STATUS "Run compile_shaders.bat (Windows) or compile_shaders.sh (Unix) to compile shaders")
#endif()



target_link_directories(TouchEngine PRIVATE
    TouchEngineplugin PUBLIC $<BUILD_INTERFACE:${TOUCHENGINE_LIB_DIR}>
)

target_link_libraries(TouchEngine
    PUBLIC
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Concurrent
        TouchEngineSDK
        TouchEngineplugin
)

target_include_directories(TouchEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

# Link D3D12 and Vulkan libraries on Windows
if(WIN32)
    target_link_libraries(TouchEngine PUBLIC
        d3d12
        dxgi
    )
    if(Vulkan_FOUND)
        target_link_libraries(TouchEngine PUBLIC Vulkan::Vulkan)
        target_compile_definitions(TouchEngine PRIVATE TOUCHENGINE_VULKAN_ENABLED)
    endif()
endif()

# Copy TouchEngine DLL to output directory on Windows
if(WIN32)
    add_custom_command(TARGET TouchEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TOUCHENGINE_BIN_DIR}/TouchEngine.dll"
            $<TARGET_FILE_DIR:TouchEngine>
    )
    target_compile_definitions(TouchEngine PRIVATE NOMINMAX)
endif()

# Set output directories
set_target_properties(TouchEngine PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# ----------------------------------------------------------------------
# Installation rules
# ----------------------------------------------------------------------

install(TARGETS TouchEngine
    EXPORT DsQtTargets
    BUNDLE DESTINATION . # For macOS bundles
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
) # For other platforms

# qt_generate_deploy_qml_app_script(
#     TARGET TouchEngine
#     OUTPUT_SCRIPT deploy_script
#     NO_UNSUPPORTED_PLATFORM_ERROR
#     DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
# )

# install(SCRIPT ${deploy_script})

install(FILES "${TOUCHENGINE_BIN_DIR}/TouchEngine.dll"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install public header files
install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h" 
        PATTERN "*.hpp"
    # Optional: exclude internal/private headers if needed
    # PATTERN "internal/*" EXCLUDE
)
