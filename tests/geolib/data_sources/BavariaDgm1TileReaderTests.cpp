#include "geolib/data_sources/BavariaDgm1TileReader.h"

#include "TestSupport.h"

#include <iomanip>
#include <sstream>
#include <string>

using namespace geo;

namespace {

/// Builds an XYZ tile: height = 100 + column + 2 * row, 1 m spacing.
std::string makeXyz(int columns, int rows, double originEast = 690000.0,
                    double originNorth = 5334000.0)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            out << (originEast + column) << " " << (originNorth + row) << " "
                << (100.0 + column + 2.0 * row) << "\n";
        }
    }
    return out.str();
}

void testReadXyzGeometry()
{
    std::istringstream in(makeXyz(3, 3));
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(BavariaDgm1TileReader::readXyz(in, tile, &error));
    CHECK_TRUE(tile.valid());
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-9);
    CHECK_NEAR(tile.originEasting(), 690000.0, 1e-6);
    CHECK_NEAR(tile.originNorthing(), 5334000.0, 1e-6);
}

void testReadXyzInterpolation()
{
    std::istringstream in(makeXyz(3, 3));
    Utm32GridTile tile;
    CHECK_TRUE(BavariaDgm1TileReader::readXyz(in, tile));

    double height = 0.0;
    // Grid nodes are exact.
    CHECK_TRUE(tile.sampleUtm(690000.0, 5334000.0, height));
    CHECK_NEAR(height, 100.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(690002.0, 5334002.0, height));
    CHECK_NEAR(height, 106.0, 1e-9);

    // Centre of a cell: bilinear mean of the four corners.
    CHECK_TRUE(tile.sampleUtm(690001.5, 5334001.5, height));
    CHECK_NEAR(height, 104.5, 1e-9);
}

void testReadXyzSeparatorsAndComments()
{
    std::istringstream in("# comment line\n"
                          "690000,5334000,100\n"
                          "690001;5334000;101\n"
                          "690000 5334001 102\n"
                          "690001 5334001 103\n");
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(BavariaDgm1TileReader::readXyz(in, tile, &error));
    CHECK_EQ_INT(tile.columns(), 2);
    CHECK_EQ_INT(tile.rows(), 2);

    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690000.5, 5334000.5, height));
    CHECK_NEAR(height, 101.5, 1e-9);
}

void testReadXyzRejectsEmptyInput()
{
    std::istringstream in("\n# only comments\n");
    Utm32GridTile tile;
    std::string error;
    CHECK_FALSE(BavariaDgm1TileReader::readXyz(in, tile, &error));
    CHECK_FALSE(error.empty());
}

/// ASCII grids list the northernmost row first; the reader must flip them.
void testReadAsciiGridRowOrder()
{
    std::istringstream in("ncols 3\n"
                          "nrows 3\n"
                          "xllcorner 690000\n"
                          "yllcorner 5334000\n"
                          "cellsize 1\n"
                          "NODATA_value -9999\n"
                          "104 105 106\n"
                          "102 103 104\n"
                          "100 101 102\n");
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(BavariaDgm1TileReader::readAsciiGrid(in, tile, &error));
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);

    double height = 0.0;
    // xllcorner is the cell corner, so the first node sits half a cell inside.
    CHECK_TRUE(tile.sampleUtm(690000.5, 5334000.5, height));
    CHECK_NEAR(height, 100.0, 1e-9); // south west
    CHECK_TRUE(tile.sampleUtm(690002.5, 5334002.5, height));
    CHECK_NEAR(height, 106.0, 1e-9); // north east
    CHECK_TRUE(tile.sampleUtm(690002.5, 5334000.5, height));
    CHECK_NEAR(height, 102.0, 1e-9); // south east
}

void testReadAsciiGridCenterReference()
{
    std::istringstream in("ncols 2\n"
                          "nrows 2\n"
                          "xllcenter 690000\n"
                          "yllcenter 5334000\n"
                          "cellsize 1\n"
                          "10 11\n"
                          "12 13\n");
    Utm32GridTile tile;
    CHECK_TRUE(BavariaDgm1TileReader::readAsciiGrid(in, tile));
    CHECK_NEAR(tile.originEasting(), 690000.0, 1e-6);
    CHECK_NEAR(tile.originNorthing(), 5334000.0, 1e-6);

    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690000.0, 5334000.0, height));
    CHECK_NEAR(height, 12.0, 1e-9); // bottom left of the flipped grid
}

void testReadAsciiGridNoData()
{
    std::istringstream in("ncols 2\n"
                          "nrows 2\n"
                          "xllcorner 690000\n"
                          "yllcorner 5334000\n"
                          "cellsize 1\n"
                          "NODATA_value -9999\n"
                          "10 -9999\n"
                          "12 13\n");
    Utm32GridTile tile;
    CHECK_TRUE(BavariaDgm1TileReader::readAsciiGrid(in, tile));

    // The stencil touches the no-data cell, so no value can be produced.
    double height = 0.0;
    CHECK_FALSE(tile.sampleUtm(690001.0, 5334001.0, height));
}

void testReadAsciiGridRejectsTruncatedData()
{
    std::istringstream in("ncols 3\n"
                          "nrows 3\n"
                          "xllcorner 690000\n"
                          "yllcorner 5334000\n"
                          "cellsize 1\n"
                          "1 2 3\n"
                          "4 5\n");
    Utm32GridTile tile;
    std::string error;
    CHECK_FALSE(BavariaDgm1TileReader::readAsciiGrid(in, tile, &error));
    CHECK_FALSE(error.empty());
}

void testReadAsciiGridRejectsIncompleteHeader()
{
    std::istringstream in("ncols 3\ncellsize 1\n1 2 3\n");
    Utm32GridTile tile;
    std::string error;
    CHECK_FALSE(BavariaDgm1TileReader::readAsciiGrid(in, tile, &error));
    CHECK_FALSE(error.empty());
}

} // namespace

int main()
{
    testReadXyzGeometry();
    testReadXyzInterpolation();
    testReadXyzSeparatorsAndComments();
    testReadXyzRejectsEmptyInput();
    testReadAsciiGridRowOrder();
    testReadAsciiGridCenterReference();
    testReadAsciiGridNoData();
    testReadAsciiGridRejectsTruncatedData();
    testReadAsciiGridRejectsIncompleteHeader();
    return geotest::summarize("BavariaDgm1TileReaderTests");
}
