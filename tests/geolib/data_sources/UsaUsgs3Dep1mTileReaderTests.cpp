#include "geolib/data_sources/UsaUsgs3Dep1mTileReader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace geo;

namespace {

constexpr int kZone = 16;

void testReadAsciiGrid()
{
    std::istringstream stream("ncols 3\n"
                              "nrows 3\n"
                              "xllcorner 540000\n"
                              "yllcorner 4400000\n"
                              "cellsize 1\n"
                              "NODATA_value -9999\n"
                              "30 31 32\n"
                              "20 21 22\n"
                              "10 11 12\n");

    UtmGridTile tile;
    std::string error;
    CHECK_TRUE(UsaUsgs3Dep1mTileReader::readAsciiGrid(stream, kZone, tile, &error));
    CHECK_EQ_INT(tile.zone(), kZone);
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-12);
    // Corner reference is shifted to the centre of the first cell.
    CHECK_NEAR(tile.originEasting(), 540000.5, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 4400000.5, 1e-9);

    double height = 0.0;
    // Row 0 of the tile is the southernmost, i.e. the last row of the file.
    CHECK_TRUE(tile.sampleUtm(540000.5, 4400000.5, height));
    CHECK_NEAR(height, 10.0, 1e-12);
    CHECK_TRUE(tile.sampleUtm(540002.5, 4400002.5, height));
    CHECK_NEAR(height, 32.0, 1e-12);
}

/// The centre based header variant must not shift the raster again.
void testAsciiGridWithCenterReference()
{
    std::istringstream stream("ncols 2\n"
                              "nrows 2\n"
                              "xllcenter 540000\n"
                              "yllcenter 4400000\n"
                              "cellsize 1\n"
                              "1 2\n"
                              "3 4\n");
    UtmGridTile tile;
    CHECK_TRUE(UsaUsgs3Dep1mTileReader::readAsciiGrid(stream, kZone, tile));
    CHECK_NEAR(tile.originEasting(), 540000.0, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 4400000.0, 1e-9);

    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(540000.0, 4400000.0, height));
    CHECK_NEAR(height, 3.0, 1e-12);
}

void testReadXyz()
{
    std::ostringstream data;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            data << (540000 + column) << " " << (4400000 + row) << " " << (row * 10 + column)
                 << "\n";
        }
    }

    std::istringstream stream(data.str());
    UtmGridTile tile;
    std::string error;
    CHECK_TRUE(UsaUsgs3Dep1mTileReader::readXyz(stream, kZone, tile, &error));
    CHECK_EQ_INT(tile.zone(), kZone);
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-12);

    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(540002.0, 4400002.0, height));
    CHECK_NEAR(height, 22.0, 1e-12);
}

/// Separators other than blanks are tolerated in the XYZ format.
void testXyzWithSeparators()
{
    std::istringstream stream("# comment\n"
                              "540000,4400000,1\n"
                              "540001,4400000,2\n"
                              "540000;4400001;3\n"
                              "540001;4400001;4\n");
    UtmGridTile tile;
    CHECK_TRUE(UsaUsgs3Dep1mTileReader::readXyz(stream, kZone, tile));
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(540001.0, 4400001.0, height));
    CHECK_NEAR(height, 4.0, 1e-12);
}

void testFileExtensionDispatch()
{
    const std::string ascPath = "usgs_3dep_test.asc";
    {
        std::ofstream out(ascPath);
        out << "ncols 2\nnrows 2\nxllcorner 540000\nyllcorner 4400000\ncellsize 1\n"
            << "5 5\n5 5\n";
    }
    UtmGridTile tile;
    std::string error;
    CHECK_TRUE(UsaUsgs3Dep1mTileReader::readFile(ascPath, kZone, tile, &error));
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(540000.5, 4400000.5, height));
    CHECK_NEAR(height, 5.0, 1e-12);
    std::remove(ascPath.c_str());

    const std::string xyzPath = "usgs_3dep_test.xyz";
    {
        std::ofstream out(xyzPath);
        out << "540000 4400000 7\n540001 4400000 7\n540000 4400001 7\n540001 4400001 7\n";
    }
    UtmGridTile xyzTile;
    CHECK_TRUE(UsaUsgs3Dep1mTileReader::readFile(xyzPath, kZone, xyzTile, &error));
    CHECK_TRUE(xyzTile.sampleUtm(540000.5, 4400000.5, height));
    CHECK_NEAR(height, 7.0, 1e-12);
    std::remove(xyzPath.c_str());
}

void testMalformedInput()
{
    std::string error;

    std::istringstream incomplete("ncols 3\nnrows 3\n");
    UtmGridTile tile;
    CHECK_FALSE(UsaUsgs3Dep1mTileReader::readAsciiGrid(incomplete, kZone, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream truncated(
        "ncols 3\nnrows 3\nxllcorner 540000\nyllcorner 4400000\ncellsize 1\n1 2\n");
    CHECK_FALSE(UsaUsgs3Dep1mTileReader::readAsciiGrid(truncated, kZone, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream emptyXyz("# nothing here\n");
    CHECK_FALSE(UsaUsgs3Dep1mTileReader::readXyz(emptyXyz, kZone, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream singleSample("540000 4400000 1\n");
    CHECK_FALSE(UsaUsgs3Dep1mTileReader::readXyz(singleSample, kZone, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(UsaUsgs3Dep1mTileReader::readFile("does_not_exist.asc", kZone, tile, &error));
    CHECK_FALSE(error.empty());
}

} // namespace

int main()
{
    testReadAsciiGrid();
    testAsciiGridWithCenterReference();
    testReadXyz();
    testXyzWithSeparators();
    testFileExtensionDispatch();
    testMalformedInput();
    return geotest::summarize("UsaUsgs3Dep1mTileReaderTests");
}
