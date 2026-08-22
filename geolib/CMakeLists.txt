add_library(geolib
    src/DateTimeUtc.cpp
    src/EarthModel.cpp
    src/GeoLocation.cpp
    src/GroundPlane.cpp
    src/HorizonDome.cpp
    src/SolarPosition.cpp
    src/SunPath.cpp
)

target_include_directories(geolib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
