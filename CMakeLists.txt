cmake_minimum_required(VERSION 3.16)

project(GeoSolar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(geolib)

option(GEOSOLAR_BUILD_TESTS "Build the geolib unit tests" ON)
if(GEOSOLAR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
