add_library(geolib
    src/BavariaDgm1HeightDataSource.cpp
    src/DateTimeUtc.cpp
    src/EarthModel.cpp
    src/GeoLocation.cpp
    src/GridHeightDataSource.cpp
    src/GroundPlane.cpp
    src/HeightDataSourceRegistry.cpp
    src/HorizonDome.cpp
    src/SolarPosition.cpp
    src/SunPath.cpp
    src/TerrainModel.cpp
    src/TriangleMesh.cpp
)

target_include_directories(geolib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
