if(NOT WIN32)
    message(STATUS "Skipping HeightDataSourcesExample because it currently uses WinHTTP on Windows")
    return()
endif()

add_executable(HeightDataSourcesExample
    HeightDataSourcesExample.cpp
)

target_link_libraries(HeightDataSourcesExample PRIVATE geolib winhttp)
