#include "geolib/data_sources/UkEaLidarTileReader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace geo;

namespace {

void testReadAsciiGrid()
{
    std::istringstream stream("ncols 3\n"
                              "nrows 3\n"
                              "xllcorner 400000\n"
                              "yllcorner 200000\n"
                              "cellsize 1\n"
                              "NODATA_value -9999\n"
                              "30 31 32\n"
                              "20 21 22\n"
                              "10 11 12\n");

    BngGridTile tile;
    std::string error;
    CHECK_TRUE(UkEaLidarTileReader::readAsciiGrid(stream, tile, &error));
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-12);
    // Corner reference is shifted to the centre of the first cell.
    CHECK_NEAR(tile.originEasting(), 400000.5, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 200000.5, 1e-9);

    double height = 0.0;
    // Row 0 of the tile is the southernmost, i.e. the last row of the file.
    CHECK_TRUE(tile.sampleBng(400000.5, 200000.5, height));
    CHECK_NEAR(height, 10.0, 1e-12);
    CHECK_TRUE(tile.sampleBng(400002.5, 200002.5, height));
    CHECK_NEAR(height, 32.0, 1e-12);
}

/// The centre based header variant must not shift the raster again.
void testAsciiGridWithCenterReference()
{
    std::istringstream stream("ncols 2\n"
                              "nrows 2\n"
                              "xllcenter 400000\n"
                              "yllcenter 200000\n"
                              "cellsize 1\n"
                              "1 2\n"
                              "3 4\n");
    BngGridTile tile;
    CHECK_TRUE(UkEaLidarTileReader::readAsciiGrid(stream, tile));
    CHECK_NEAR(tile.originEasting(), 400000.0, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 200000.0, 1e-9);

    double height = 0.0;
    CHECK_TRUE(tile.sampleBng(400000.0, 200000.0, height));
    CHECK_NEAR(height, 3.0, 1e-12);
}

void testReadXyz()
{
    std::ostringstream data;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            data << (400000 + column) << " " << (200000 + row) << " " << (row * 10 + column)
                 << "\n";
        }
    }

    std::istringstream stream(data.str());
    BngGridTile tile;
    std::string error;
    CHECK_TRUE(UkEaLidarTileReader::readXyz(stream, tile, &error));
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-12);

    double height = 0.0;
    CHECK_TRUE(tile.sampleBng(400002.0, 200002.0, height));
    CHECK_NEAR(height, 22.0, 1e-12);
}

/// Separators other than blanks are tolerated in the XYZ format.
void testXyzWithSeparators()
{
    std::istringstream stream("# comment\n"
                              "400000,200000,1\n"
                              "400001,200000,2\n"
                              "400000;200001;3\n"
                              "400001;200001;4\n");
    BngGridTile tile;
    CHECK_TRUE(UkEaLidarTileReader::readXyz(stream, tile));
    double height = 0.0;
    CHECK_TRUE(tile.sampleBng(400001.0, 200001.0, height));
    CHECK_NEAR(height, 4.0, 1e-12);
}

void testFileExtensionDispatch()
{
    const std::string ascPath = "uk_lidar_test.asc";
    {
        std::ofstream out(ascPath);
        out << "ncols 2\nnrows 2\nxllcorner 400000\nyllcorner 200000\ncellsize 1\n"
            << "5 5\n5 5\n";
    }
    BngGridTile tile;
    std::string error;
    CHECK_TRUE(UkEaLidarTileReader::readFile(ascPath, tile, &error));
    double height = 0.0;
    CHECK_TRUE(tile.sampleBng(400000.5, 200000.5, height));
    CHECK_NEAR(height, 5.0, 1e-12);
    std::remove(ascPath.c_str());

    const std::string xyzPath = "uk_lidar_test.xyz";
    {
        std::ofstream out(xyzPath);
        out << "400000 200000 7\n400001 200000 7\n400000 200001 7\n400001 200001 7\n";
    }
    BngGridTile xyzTile;
    CHECK_TRUE(UkEaLidarTileReader::readFile(xyzPath, xyzTile, &error));
    CHECK_TRUE(xyzTile.sampleBng(400000.5, 200000.5, height));
    CHECK_NEAR(height, 7.0, 1e-12);
    std::remove(xyzPath.c_str());
}

void testMalformedInput()
{
    std::string error;

    std::istringstream incomplete("ncols 3\nnrows 3\n");
    BngGridTile tile;
    CHECK_FALSE(UkEaLidarTileReader::readAsciiGrid(incomplete, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream truncated(
        "ncols 3\nnrows 3\nxllcorner 400000\nyllcorner 200000\ncellsize 1\n1 2\n");
    CHECK_FALSE(UkEaLidarTileReader::readAsciiGrid(truncated, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream emptyXyz("# nothing here\n");
    CHECK_FALSE(UkEaLidarTileReader::readXyz(emptyXyz, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream singleSample("400000 200000 1\n");
    CHECK_FALSE(UkEaLidarTileReader::readXyz(singleSample, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(UkEaLidarTileReader::readFile("does_not_exist.asc", tile, &error));
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
    return geotest::summarize("UkEaLidarTileReaderTests");
}
