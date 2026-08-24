#pragma once

#include "geolib/data_sources/Utm32GridTile.h"

#include <istream>
#include <string>

namespace geo {

/// Reader for the tile files of the German open data set
/// "Digitales Geländemodell Gitterweite 5 m (DGM5)" (BKG).
///
/// Three text formats are supported:
///  * XYZ  - the official delivery format: one "easting northing height" triple
///           per line, sorted, with a regular 5 m spacing in UTM32.
///  * ASC  - ESRI ASCII grid, which the BKG conversion tools produce. The
///           header keys ncols/nrows/xllcorner/yllcorner/cellsize/NODATA_value
///           are evaluated; xllcenter/yllcenter are accepted as well.
///  * GridDB text export ("G01"/"GK") style files are not supported.
///
/// Unlike the DGM1 tiles the samples may carry a decimal comma in some German
/// exports; such values are normalised while parsing.
///
/// The reader only parses; downloading is handled by BkgDgm5TileDownloader.
class BkgDgm5TileReader {
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
