cmake_minimum_required(VERSION 3.16)

project(GeoSolar LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_GEOLIB_TESTS    "Build the geolib unit tests" ON)
option(BUILD_GEOLIB_EXAMPLES "Build the geolib examples" ON)
option(BUILD_SOLARSIM_APP    "Build the SolarSim Qt GUI application" ON)

add_subdirectory(src/geolib)

add_library(mongoose STATIC src/third_party/mongoose/mongoose.c)
target_include_directories(mongoose PUBLIC src/third_party/mongoose)
target_compile_definitions(mongoose PUBLIC
    MG_TLS=MG_TLS_BUILTIN              # HTTPS without OpenSSL
    MG_MAX_RECV_SIZE=\(512*1024*1024\) # tiles are larger than the 3 MB default
)
if(WIN32)
    target_link_libraries(mongoose PUBLIC ws2_32)
endif()

# then link it to the app target:
# target_link_libraries(<your_app_target> PRIVATE mongoose)

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

