#include "geolib/data_sources/GermanyDgm5TileReader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace geo;

namespace {

/// XYZ tile with the official 5 m spacing; the height encodes the cell index.
std::string makeXyz(long long originEast, long long originNorth)
{
    std::ostringstream data;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            data << (originEast + column * 5) << " " << (originNorth + row * 5) << " "
                 << (row * 10 + column) << "\n";
        }
    }
    return data.str();
}

void testReadXyz()
{
    std::istringstream stream(makeXyz(690000, 5334000));
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(GermanyDgm5TileReader::readXyz(stream, tile, &error));
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    // The 5 m grid spacing must be derived from the samples.
    CHECK_NEAR(tile.cellSize(), 5.0, 1e-12);
    CHECK_NEAR(tile.originEasting(), 690000.0, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 5334000.0, 1e-9);

    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690010.0, 5334010.0, height));
    CHECK_NEAR(height, 22.0, 1e-12);
    // Halfway between two samples the interpolation must kick in.
    CHECK_TRUE(tile.sampleUtm(690002.5, 5334000.0, height));
    CHECK_NEAR(height, 0.5, 1e-12);
}

/// German exports may use a decimal comma; a comma separated variant without
/// decimals must still be split into three columns.
void testXyzSeparatorsAndDecimalComma()
{
    std::istringstream decimalComma("690000 5334000 1,5\n"
                                    "690005 5334000 2,5\n"
                                    "690000 5334005 3,5\n"
                                    "690005 5334005 4,5\n");
    Utm32GridTile tile;
    CHECK_TRUE(GermanyDgm5TileReader::readXyz(decimalComma, tile));
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690005.0, 5334005.0, height));
    CHECK_NEAR(height, 4.5, 1e-12);

    std::istringstream separated("# comment\n"
                                 "690000,5334000,1\n"
                                 "690005,5334000,2\n"
                                 "690000,5334005,3\n"
                                 "690005,5334005,4\n");
    Utm32GridTile commaTile;
    CHECK_TRUE(GermanyDgm5TileReader::readXyz(separated, commaTile));
    CHECK_TRUE(commaTile.sampleUtm(690005.0, 5334005.0, height));
    CHECK_NEAR(height, 4.0, 1e-12);

    std::istringstream semicolons("690000;5334000;1\n"
                                  "690005;5334000;2\n"
                                  "690000;5334005;3\n"
                                  "690005;5334005;4\n");
    Utm32GridTile semicolonTile;
    CHECK_TRUE(GermanyDgm5TileReader::readXyz(semicolons, semicolonTile));
    CHECK_TRUE(semicolonTile.sampleUtm(690000.0, 5334005.0, height));
    CHECK_NEAR(height, 3.0, 1e-12);
}

void testReadAsciiGrid()
{
    std::istringstream stream("ncols 3\n"
                              "nrows 3\n"
                              "xllcorner 690000\n"
                              "yllcorner 5334000\n"
                              "cellsize 5\n"
                              "NODATA_value -9999\n"
                              "30 31 32\n"
                              "20 21 22\n"
                              "10 11 12\n");
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(GermanyDgm5TileReader::readAsciiGrid(stream, tile, &error));
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    CHECK_NEAR(tile.cellSize(), 5.0, 1e-12);
    // Corner reference is shifted to the centre of the first cell.
    CHECK_NEAR(tile.originEasting(), 690002.5, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 5334002.5, 1e-9);

    double height = 0.0;
    // Row 0 of the tile is the southernmost, i.e. the last row of the file.
    CHECK_TRUE(tile.sampleUtm(690002.5, 5334002.5, height));
    CHECK_NEAR(height, 10.0, 1e-12);
    CHECK_TRUE(tile.sampleUtm(690012.5, 5334012.5, height));
    CHECK_NEAR(height, 32.0, 1e-12);
}

void testAsciiGridWithCenterReference()
{
    std::istringstream stream("ncols 2\n"
                              "nrows 2\n"
                              "xllcenter 690000\n"
                              "yllcenter 5334000\n"
                              "cellsize 5\n"
                              "1 2\n"
                              "3 4\n");
    Utm32GridTile tile;
    CHECK_TRUE(GermanyDgm5TileReader::readAsciiGrid(stream, tile));
    CHECK_NEAR(tile.originEasting(), 690000.0, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 5334000.0, 1e-9);

    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690000.0, 5334000.0, height));
    CHECK_NEAR(height, 3.0, 1e-12);
}

void testFileExtensionDispatch()
{
    const std::string xyzPath = "dgm5_test_tile.xyz";
    {
        std::ofstream out(xyzPath);
        out << makeXyz(690000, 5334000);
    }
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(GermanyDgm5TileReader::readFile(xyzPath, tile, &error));
    CHECK_NEAR(tile.cellSize(), 5.0, 1e-12);
    std::remove(xyzPath.c_str());

    const std::string ascPath = "dgm5_test_tile.asc";
    {
        std::ofstream out(ascPath);
        out << "ncols 2\nnrows 2\nxllcorner 690000\nyllcorner 5334000\ncellsize 5\n"
            << "7 7\n7 7\n";
    }
    Utm32GridTile ascTile;
    CHECK_TRUE(GermanyDgm5TileReader::readFile(ascPath, ascTile, &error));
    double height = 0.0;
    CHECK_TRUE(ascTile.sampleUtm(690002.5, 5334002.5, height));
    CHECK_NEAR(height, 7.0, 1e-12);
    std::remove(ascPath.c_str());
}

void testMalformedInput()
{
    Utm32GridTile tile;
    std::string error;

    std::istringstream empty("# nothing here\n");
    CHECK_FALSE(GermanyDgm5TileReader::readXyz(empty, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream single("690000 5334000 1\n");
    CHECK_FALSE(GermanyDgm5TileReader::readXyz(single, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream incomplete("ncols 3\nnrows 3\n");
    CHECK_FALSE(GermanyDgm5TileReader::readAsciiGrid(incomplete, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    std::istringstream truncated(
        "ncols 3\nnrows 3\nxllcorner 690000\nyllcorner 5334000\ncellsize 5\n1 2\n");
    CHECK_FALSE(GermanyDgm5TileReader::readAsciiGrid(truncated, tile, &error));
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(GermanyDgm5TileReader::readFile("does_not_exist.xyz", tile, &error));
    CHECK_FALSE(error.empty());
}

} // namespace

int main()
{
    testReadXyz();
    testXyzSeparatorsAndDecimalComma();
    testReadAsciiGrid();
    testAsciiGridWithCenterReference();
    testFileExtensionDispatch();
    testMalformedInput();
    return geotest::summarize("GermanyDgm5TileReaderTests");
}
