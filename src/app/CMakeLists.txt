# Qt GUI application. Qt6 is preferred, Qt5 works as a fallback.
find_package(Qt6 COMPONENTS Widgets Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.12 COMPONENTS Widgets Network QUIET)
endif()

if(NOT Qt6_FOUND AND NOT Qt5_FOUND)
    message(STATUS "Qt (Widgets) not found - skipping the SolarSim GUI application")
    return()
endif()

set(CMAKE_AUTOMOC ON)

# Optional hardware accelerated rendering backends. Off by default; the
# software rasterizer (SceneView) is used unless one of these is enabled.
# Only one can be active at a time.
option(SOLARSIM_USE_WEBGPU "Render the scene with hardware accelerated WebGPU instead of the software rasterizer" OFF)
option(SOLARSIM_USE_OPENGL "Render the scene with hardware accelerated OpenGL (Qt's QOpenGLWidget) instead of the software rasterizer" OFF)

if(SOLARSIM_USE_WEBGPU AND SOLARSIM_USE_OPENGL)
    message(FATAL_ERROR "SOLARSIM_USE_WEBGPU and SOLARSIM_USE_OPENGL are mutually exclusive")
endif()

add_executable(SolarSim WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
    HeightDataSources.cpp
    HeightDataSources.h
)

if(SOLARSIM_USE_OPENGL)
    target_sources(SolarSim PRIVATE GLSceneView.cpp GLSceneView.h)
    target_compile_definitions(SolarSim PRIVATE SOLARSIM_USE_OPENGL)

    if(Qt6_FOUND)
        find_package(Qt6 COMPONENTS OpenGLWidgets REQUIRED)
        target_link_libraries(SolarSim PRIVATE Qt6::OpenGLWidgets)
    else()
        find_package(Qt5 5.12 COMPONENTS OpenGL REQUIRED)
        target_link_libraries(SolarSim PRIVATE Qt5::OpenGL)
    endif()
else()
    target_sources(SolarSim PRIVATE SceneView.cpp SceneView.h)
endif()

if(SOLARSIM_USE_WEBGPU)
    target_sources(SolarSim PRIVATE GpuSceneRenderer.cpp GpuSceneRenderer.h)
    target_compile_definitions(SolarSim PRIVATE SOLARSIM_USE_WEBGPU)

    if(EMSCRIPTEN)
        # Emscripten ships its own webgpu.h and JS glue; just enable the
        # feature flags on the compiler/linker.
        target_compile_options(SolarSim PRIVATE -sUSE_WEBGPU=1)
        target_link_options(SolarSim PRIVATE -sUSE_WEBGPU=1 -sASYNCIFY)
    else()
        # Native desktop build: expects a WebGPU implementation providing the
        # "webgpu" CMake package (e.g. wgpu-native or Dawn built with
        # find_package support) with a webgpu::webgpu target exposing
        # webgpu.h and the platform surface creation helpers.
        find_package(webgpu QUIET)
        if(webgpu_FOUND)
            target_link_libraries(SolarSim PRIVATE webgpu::webgpu)
        else()
            message(WARNING
                "SOLARSIM_USE_WEBGPU is ON but no native WebGPU package "
                "(wgpu-native / Dawn) was found via find_package(webgpu). "
                "Fetch/install one and point CMAKE_PREFIX_PATH at it, or "
                "build for Emscripten instead.")
        endif()
    endif()
endif()

target_link_libraries(SolarSim PRIVATE geolib)

if(Qt6_FOUND)
    target_link_libraries(SolarSim PRIVATE Qt6::Widgets Qt6::Network)
else()
    target_link_libraries(SolarSim PRIVATE Qt5::Widgets Qt5::Network)
endif()

target_include_directories(SolarSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
