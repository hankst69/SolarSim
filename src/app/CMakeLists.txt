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

# Optional hardware accelerated rendering backend via WebGPU. Off by default
# because it needs a native WebGPU implementation (wgpu-native or Dawn) to be
# available/findable, or Emscripten as the toolchain for a WebAssembly build,
# where the browser's own WebGPU implementation is used automatically.
option(SOLARSIM_USE_WEBGPU "Render the scene with hardware accelerated WebGPU instead of the software rasterizer" OFF)

add_executable(SolarSim WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
    SceneView.cpp
    SceneView.h
    HeightDataSources.cpp
    HeightDataSources.h
)

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
