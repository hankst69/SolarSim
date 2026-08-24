#include "geolib/data_sources/WorldCopernicusDem30TileReader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace geo;

namespace {

/// Writes a square HGT raster (big endian, north row first) where the height of
/// each sample equals its row index times 10 plus its column index.
void writeHgt(const std::string& path, int side)
{
    std::ofstream out(path, std::ios::binary);
    for (int row = 0; row < side; ++row) {
        for (int column = 0; column < side; ++column) {
            const int value = row * 10 + column;
            const char bytes[2] = {static_cast<char>((value >> 8) & 0xFF),
                                   static_cast<char>(value & 0xFF)};
            out.write(bytes, 2);
        }
    }
}

void testReadHgt()
{
    const std::string path = "copernicus_test_tile.hgt";
    writeHgt(path, 3);

    const GeoBounds bounds{48.0, 49.0, 11.0, 12.0};
    std::string error;
    const auto tile = WorldCopernicusDem30TileReader::readFile(path, bounds, &error);
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        CHECK_EQ_INT(tile->columns(), 3);
        CHECK_EQ_INT(tile->rows(), 3);

        double height = 0.0;
        // North west corner is the first sample of the first row.
        CHECK_TRUE(tile->sampleHeight(49.0, 11.0, height));
        CHECK_NEAR(height, 0.0, 1e-9);
        // South east corner is the last sample of the last row.
        CHECK_TRUE(tile->sampleHeight(48.0, 12.0, height));
        CHECK_NEAR(height, 22.0, 1e-9);
        // Centre sample.
        CHECK_TRUE(tile->sampleHeight(48.5, 11.5, height));
        CHECK_NEAR(height, 11.0, 1e-9);
    }

    std::remove(path.c_str());
}

/// Negative heights (below the ellipsoid / dead sea areas) must survive the
/// big endian two's complement decoding.
void testNegativeHeights()
{
    std::string raw;
    const int values[4] = {-100, -1, 0, 5};
    for (const int value : values) {
        raw.push_back(static_cast<char>((value >> 8) & 0xFF));
        raw.push_back(static_cast<char>(value & 0xFF));
    }
    std::istringstream stream(raw, std::ios::binary);

    const GeoBounds bounds{31.0, 32.0, 35.0, 36.0};
    const auto tile = WorldCopernicusDem30TileReader::readHgt(stream, bounds);
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        double height = 0.0;
        CHECK_TRUE(tile->sampleHeight(32.0, 35.0, height));
        CHECK_NEAR(height, -100.0, 1e-9);
        CHECK_TRUE(tile->sampleHeight(32.0, 36.0, height));
        CHECK_NEAR(height, -1.0, 1e-9);
    }
}

void testReadAsciiGrid()
{
    std::istringstream stream("ncols 3\n"
                              "nrows 3\n"
                              "xllcorner 11.0\n"
                              "yllcorner 48.0\n"
                              "cellsize 0.5\n"
                              "NODATA_value -32768\n"
                              "10 11 12\n"
                              "20 21 22\n"
                              "30 31 32\n");

    std::string error;
    const auto tile = WorldCopernicusDem30TileReader::readAsciiGrid(stream, &error);
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        CHECK_EQ_INT(tile->columns(), 3);
        CHECK_EQ_INT(tile->rows(), 3);
        double height = 0.0;
        // North west sample centre.
        CHECK_TRUE(tile->sampleHeight(49.25, 11.25, height));
        CHECK_NEAR(height, 10.0, 1e-9);
        // South east sample centre.
        CHECK_TRUE(tile->sampleHeight(48.25, 12.25, height));
        CHECK_NEAR(height, 32.0, 1e-9);
    }
}

void testNoDataIsRejected()
{
    std::istringstream stream("ncols 2\n"
                              "nrows 2\n"
                              "xllcorner 11.0\n"
                              "yllcorner 48.0\n"
                              "cellsize 0.5\n"
                              "NODATA_value -32768\n"
                              "10 -32768\n"
                              "10 10\n");
    const auto tile = WorldCopernicusDem30TileReader::readAsciiGrid(stream);
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        double height = 0.0;
        CHECK_FALSE(tile->sampleHeight(48.5, 11.5, height));
    }
}

void testMalformedInput()
{
    std::string error;
    std::istringstream odd(std::string("\x01\x02\x03", 3), std::ios::binary);
    CHECK_TRUE(WorldCopernicusDem30TileReader::readHgt(odd, GeoBounds{}, &error) == nullptr);
    CHECK_FALSE(error.empty());

    error.clear();
    // 6 samples cannot form a square raster.
    std::istringstream nonSquare(std::string(12, '\0'), std::ios::binary);
    CHECK_TRUE(WorldCopernicusDem30TileReader::readHgt(nonSquare, GeoBounds{}, &error) == nullptr);
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream truncated("ncols 3\nnrows 3\nxllcorner 11\nyllcorner 48\ncellsize 0.5\n1 2\n");
    CHECK_TRUE(WorldCopernicusDem30TileReader::readAsciiGrid(truncated, &error) == nullptr);
    CHECK_FALSE(error.empty());

    error.clear();
    const auto missing = WorldCopernicusDem30TileReader::readFile("does_not_exist.hgt", GeoBounds{}, &error);
    CHECK_TRUE(missing == nullptr);
    CHECK_FALSE(error.empty());
}

} // namespace

int main()
{
    testReadHgt();
    testNegativeHeights();
    testReadAsciiGrid();
    testNoDataIsRejected();
    testMalformedInput();
    return geotest::summarize("WorldCopernicusDem30TileReaderTests");
}
