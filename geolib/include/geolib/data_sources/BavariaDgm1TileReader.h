#pragma once

#include "geolib/data_sources/Utm32GridTile.h"

#include <istream>
#include <string>

namespace geo {

/// Reader for the tile files of the Bavarian open data set
/// "openData Digitales Gelaendemodell 1m (DGM1)" (LDBV).
///
/// Two text formats are supported:
///  * XYZ  - the official DGM1 delivery format: one "easting northing height"
///           triple per line, sorted, with a regular 1 m spacing in UTM32.
///  * ASC  - ESRI ASCII grid, which many conversion tools produce. The header
///           keys ncols/nrows/xllcorner/yllcorner/cellsize/NODATA_value are
///           evaluated; xllcenter/yllcenter are accepted as well.
///
/// The reader only parses; downloading is handled by
/// BavariaDgm1TileDownloader.
class BavariaDgm1TileReader {
public:
    /// Auto detects the format from the file extension (".asc" -> ASCII grid,
    /// everything else -> XYZ) and reads the tile.
    static bool readFile(const std::string& path, Utm32GridTile& tile,
                         std::string* error = nullptr);

    static bool readXyz(std::istream& stream, Utm32GridTile& tile, std::string* error = nullptr);

    static bool readAsciiGrid(std::istream& stream, Utm32GridTile& tile,
                              std::string* error = nullptr);
};

} // namespace geo
