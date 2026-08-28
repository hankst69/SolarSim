if(NOT WIN32)
    message(STATUS "Skipping HeightDataSourcesSample because it currently uses WinHTTP on Windows")
    return()
endif()

add_executable(HeightDataSourcesSample
    HeightDataSourcesSample.cpp
)

target_link_libraries(HeightDataSourcesSample PRIVATE geolib winhttp)
