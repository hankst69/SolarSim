cmake_minimum_required(VERSION 3.16)

project(GeoSolar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_GEOLIB_TESTS    "Build the geolib unit tests" ON)
option(BUILD_GEOLIB_EXAMPLES "Build the geolib examples" ON)
option(BUILD_SOLARSIM_APP    "Build the SolarSim Qt GUI application" ON)

add_subdirectory(src/geolib)

if(BUILD_SOLARSIM_APP)
    add_subdirectory(src/app)
endif()

if(BUILD_GEOLIB_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_GEOLIB_EXAMPLES)
    add_subdirectory(src/examples)
endif()

