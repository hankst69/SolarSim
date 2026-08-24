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
endfunction()

add_geolib_test(UtmProjectionTests geolib/UtmProjectionTests.cpp)

add_geolib_test(Utm32GridTileTests geolib/data_sources/Utm32GridTileTests.cpp)
add_geolib_test(BavariaDgm1TileReaderTests geolib/data_sources/BavariaDgm1TileReaderTests.cpp)
add_geolib_test(BavariaDgm1HeightDataSourceTests
    geolib/data_sources/BavariaDgm1HeightDataSourceTests.cpp)
add_geolib_test(BavariaDgm1TileDownloaderTests
    geolib/data_sources/BavariaDgm1TileDownloaderTests.cpp)

# The downloader test writes into a cache directory below the working
# directory; run every suite in a dedicated place inside the build tree.
set_tests_properties(
    UtmProjectionTests
    Utm32GridTileTests
    BavariaDgm1TileReaderTests
    BavariaDgm1HeightDataSourceTests
    BavariaDgm1TileDownloaderTests
    PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
