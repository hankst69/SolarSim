# Test suites for geolib.
#
# Each suite is a small standalone executable that prints its failures and
# returns a non zero exit code, which is all CTest needs. The directory layout
# mirrors the library sources: tests/geolib for the core types,
# tests/geolib/data_sources for the concrete height data sources.

# Adds a test executable named <name> for the given source file.
function(add_geolib_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE geolib)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
    # Some suites write scratch files; keep them inside the build tree.
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

# Core library types.
add_geolib_test(AngleTests geolib/AngleTests.cpp)
add_geolib_test(Vector3Tests geolib/Vector3Tests.cpp)
add_geolib_test(DateTimeUtcTests geolib/DateTimeUtcTests.cpp)
add_geolib_test(EarthModelTests geolib/EarthModelTests.cpp)
add_geolib_test(GeoLocationTests geolib/GeoLocationTests.cpp)
add_geolib_test(GroundPlaneTests geolib/GroundPlaneTests.cpp)
add_geolib_test(HorizonDomeTests geolib/HorizonDomeTests.cpp)
add_geolib_test(UtmProjectionTests geolib/UtmProjectionTests.cpp)
add_geolib_test(BritishNationalGridProjectionTests geolib/BritishNationalGridProjectionTests.cpp)
add_geolib_test(SolarPositionTests geolib/SolarPositionTests.cpp)
add_geolib_test(SunPathTests geolib/SunPathTests.cpp)
add_geolib_test(TriangleMeshTests geolib/TriangleMeshTests.cpp)
add_geolib_test(HeightDataSourceTests geolib/HeightDataSourceTests.cpp)
add_geolib_test(TerrainModelTests geolib/TerrainModelTests.cpp)

# Concrete height data sources.
add_geolib_test(Utm32GridTileTests geolib/data_sources/Utm32GridTileTests.cpp)
add_geolib_test(BavariaDgm1TileReaderTests geolib/data_sources/BavariaDgm1TileReaderTests.cpp)
add_geolib_test(BavariaDgm1HeightDataSourceTests
    geolib/data_sources/BavariaDgm1HeightDataSourceTests.cpp)
add_geolib_test(BavariaDgm1TileDownloaderTests
    geolib/data_sources/BavariaDgm1TileDownloaderTests.cpp)
add_geolib_test(CopernicusDem30TileReaderTests
    geolib/data_sources/CopernicusDem30TileReaderTests.cpp)
add_geolib_test(CopernicusDem30HeightDataSourceTests
    geolib/data_sources/CopernicusDem30HeightDataSourceTests.cpp)
add_geolib_test(CopernicusDem30TileDownloaderTests
    geolib/data_sources/CopernicusDem30TileDownloaderTests.cpp)
add_geolib_test(BngGridTileTests geolib/data_sources/BngGridTileTests.cpp)
add_geolib_test(UkEaLidarTileReaderTests geolib/data_sources/UkEaLidarTileReaderTests.cpp)
add_geolib_test(UkEaLidarHeightDataSourceTests
    geolib/data_sources/UkEaLidarHeightDataSourceTests.cpp)
add_geolib_test(UkEaLidarTileDownloaderTests
    geolib/data_sources/UkEaLidarTileDownloaderTests.cpp)
