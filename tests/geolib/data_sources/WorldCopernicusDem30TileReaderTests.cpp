#include "geolib/data_sources/WorldCopernicusDem30TileReader.h"

#include "TestSupport.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
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

namespace {

void putU16(std::vector<unsigned char>& buf, std::uint16_t v)
{
    buf.push_back(static_cast<unsigned char>(v & 0xFF));
    buf.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
}

void putU32(std::vector<unsigned char>& buf, std::uint32_t v)
{
    buf.push_back(static_cast<unsigned char>(v & 0xFF));
    buf.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
    buf.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
    buf.push_back(static_cast<unsigned char>((v >> 24) & 0xFF));
}

/// Writes a minimal uncompressed, single strip, Float32 GeoTIFF (little
/// endian) with a square raster of `side` x `side` samples, each equal to its
/// row index times 10 plus its column index. Carries no GeoTIFF
/// georeference, so the caller supplied bounds are used.
void writeGeoTiff(const std::string& path, int side)
{
    std::vector<unsigned char> raster;
    raster.reserve(static_cast<std::size_t>(side) * side * 4);
    for (int row = 0; row < side; ++row) {
        for (int column = 0; column < side; ++column) {
            const float value = static_cast<float>(row * 10 + column);
            unsigned char bytes[4];
            std::memcpy(bytes, &value, 4);
            raster.insert(raster.end(), bytes, bytes + 4);
        }
    }

    // Layout: header(8) + IFD + raster.
    constexpr int kEntryCount = 8;
    const std::size_t ifdOffset = 8;
    const std::size_t ifdSize = 2 + static_cast<std::size_t>(kEntryCount) * 12 + 4;
    const std::size_t rasterOffset = ifdOffset + ifdSize;

    std::vector<unsigned char> buf;
    buf.push_back('I');
    buf.push_back('I');
    putU16(buf, 42);
    putU32(buf, static_cast<std::uint32_t>(ifdOffset));

    putU16(buf, kEntryCount);

    auto writeEntry = [&](std::uint16_t tag, std::uint16_t type, std::uint32_t count,
                          std::uint32_t valueOrOffset) {
        putU16(buf, tag);
        putU16(buf, type);
        putU32(buf, count);
        putU32(buf, valueOrOffset);
    };

    writeEntry(256, 3, 1, static_cast<std::uint32_t>(side)); // ImageWidth
    writeEntry(257, 3, 1, static_cast<std::uint32_t>(side)); // ImageLength
    writeEntry(258, 3, 1, 32);                              // BitsPerSample
    writeEntry(259, 3, 1, 1);                                // Compression = none
    writeEntry(273, 4, 1, static_cast<std::uint32_t>(rasterOffset)); // StripOffsets
    writeEntry(278, 3, 1, static_cast<std::uint32_t>(side));         // RowsPerStrip
    writeEntry(279, 4, 1, static_cast<std::uint32_t>(raster.size())); // StripByteCounts
    writeEntry(339, 3, 1, 3);                                        // SampleFormat = IEEE float

    putU32(buf, 0); // next IFD offset

    buf.resize(rasterOffset, 0);
    buf.insert(buf.end(), raster.begin(), raster.end());

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
}

} // namespace

void testReadGeoTiff()
{
    const std::string path = "copernicus_test_tile.tif";
    writeGeoTiff(path, 3);

    std::string error;
    const auto tile =
        WorldCopernicusDem30TileReader::readFile(path, GeoBounds{48.0, 49.0, 11.0, 12.0}, &error);
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        CHECK_EQ_INT(tile->columns(), 3);
        CHECK_EQ_INT(tile->rows(), 3);

        double height = 0.0;
        CHECK_TRUE(tile->sampleHeight(49.0, 11.0, height));
        CHECK_NEAR(height, 0.0, 1e-6);
        CHECK_TRUE(tile->sampleHeight(48.0, 12.0, height));
        CHECK_NEAR(height, 22.0, 1e-6);
        CHECK_TRUE(tile->sampleHeight(48.5, 11.5, height));
        CHECK_NEAR(height, 11.0, 1e-6);
    }

    std::remove(path.c_str());
}

} // namespace

int main()
{
    testReadHgt();
    testNegativeHeights();
    testReadAsciiGrid();
    testNoDataIsRejected();
    testMalformedInput();
    testReadGeoTiff();
    return geotest::summarize("WorldCopernicusDem30TileReaderTests");
}
