cmake_minimum_required(VERSION  3.29)

project(TouchEngineQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(QT_QML_GENERATE_QMLLS_INI ON)


# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Quick
    QuickControls2
    Gui
    GuiPrivate
    Concurrent
    ShaderTools
)

qt_standard_project_setup(REQUIRES 6.9)

#set qml import path. This is used by Qt Creator to find the QML files.
set(QML_BIN_ROOT "${CMAKE_BINARY_DIR}/TouchEngineQt")
set(QML_TMP_PATH
     "${CMAKE_SOURCE_DIR};${CMAKE_SOURCE_DIR}\\qml;${QML_BIN_ROOT};${QML_BIN_ROOT}\\qml;${QML_IMPORT_PATH}"
)

#set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/DEPLOY/")

list(REMOVE_DUPLICATES QML_TMP_PATH)

set(QML_IMPORT_PATH
     "${QML_TMP_PATH}"
     CACHE PATH "QT creator qml import path" FORCE
 )


# TouchEngine SDK paths - adjust as needed
set(TOUCHENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TouchEngine/include" CACHE PATH "TouchEngine include directory")
set(TOUCHENGINE_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TouchEngine/lib" CACHE PATH "TouchEngine library directory")
set(TOUCHENGINE_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TouchEngine/bin" CACHE PATH "TouchEngine library directory")

# Find Vulkan SDK
find_package(Vulkan)

# Source files
set(SOURCES
    src/dsQmlTouchEngineManager.cpp
    src/dsTouchEngineTextureNode.cpp
    src/dsQmlTouchEngineOutputView.cpp
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

set(SOURCES_MAIN
    #src/main.cpp
    #${SOURCES}
)

set(HEADERS
    #src/sharedd3dtexture.h
    src/dsQmlTouchEngineManager.h
    src/dsTouchEngineTextureNode.h
    src/dsQmlTouchEngineOutputView.h
    #src/rhitexturebridge.h

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
    #qml/Main.qml
    qml/TouchEngineView.qml
    qml/InstanceControl.qml
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
)


qt_add_qml_module(TouchEngineQt
    URI TouchEngineQt
    STATIC
    VERSION 1.0
    QML_FILES ${QML_FILES}
    SOURCES ${SOURCES} ${HEADERS} ${TOUCHENGINE_INPUT_SOURCES}
    DEPENDENCIES QtQuick TouchEngineQtplugin
    QML_FILES qml/SimpleMain.qml
    SOURCES src/dsQmlTouchEngineTextureInput.h src/dsQmlTouchEngineTextureInput.cpp

)

qt6_add_shaders(TouchEngineQt "shaders"
    PREFIX "/"
    HLSL 50
    PRECOMPILE
    BATCHABLE
    FILES
        resources/shaders/texture.vert
        resources/shaders/texture.frag
)

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

target_include_directories(TouchEngineQt PUBLIC
    src ${TOUCHENGINE_INCLUDE_DIR}
)

target_link_directories(TouchEngineQt PRIVATE
    TouchEngineQtplugin PUBLIC ${TOUCHENGINE_LIB_DIR}
)

target_link_libraries(TouchEngineQt PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Concurrent
    PUBLIC
    TouchEngine
)

# Link D3D12 and Vulkan libraries on Windows
if(WIN32)
    target_link_libraries(TouchEngineQt PRIVATE
        d3d12
        dxgi
    )
    if(Vulkan_FOUND)
        target_link_libraries(TouchEngineQt PRIVATE Vulkan::Vulkan)
        target_compile_definitions(TouchEngineQt PRIVATE TOUCHENGINE_VULKAN_ENABLED)
    endif()
endif()

# Copy TouchEngine DLL to output directory on Windows
if(WIN32)
    add_custom_command(TARGET TouchEngineQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TOUCHENGINE_BIN_DIR}/TouchEngine.dll"
            $<TARGET_FILE_DIR:TouchEngineQt>
    )
    target_compile_definitions(TouchEngineQt PRIVATE NOMINMAX)
endif()

# Set output directories
set_target_properties(TouchEngineQt PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)



install(TARGETS TouchEngineQt
            BUNDLE DESTINATION . # For macOS bundles
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ) # For other platforms

qt_generate_deploy_qml_app_script(
    TARGET TouchEngineQt
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

install(SCRIPT ${deploy_script})

install(CODE "
    #move the dll to the qt directory\n
    file(COPY \"${TOUCHENGINE_BIN_DIR}/TouchEngine.dll\" DESTINATION \"${CMAKE_INSTALL_PREFIX}${CMAKE_INSTALL_BINDIR}\")\n

")

message("
    #move the dll to the qt directory\n
    file(COPY \"${TOUCHENGINE_BIN_DIR}/TouchEngine.dll\" DESTINATION \"${CMAKE_INSTALL_PREFIX}${CMAKE_INSTALL_BINDIR}\")\n

")
