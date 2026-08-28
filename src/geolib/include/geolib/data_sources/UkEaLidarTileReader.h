#pragma once

#include "geolib/data_sources/BngGridTile.h"

#include <istream>
#include <string>

namespace geo {

/// Reader for the tile files of the Environment Agency open data set
/// "LIDAR Composite DTM 1 m" (England).
///
/// Two text formats are supported:
///  * ASC  - the official delivery format: an ESRI ASCII grid on the British
///           National Grid. The header keys ncols/nrows/xllcorner/yllcorner/
///           cellsize/NODATA_value are evaluated; xllcenter/yllcenter are
///           accepted as well.
///  * XYZ  - one "easting northing height" triple per line, as produced by
///           common conversion tools; the grid geometry is derived from the
///           sample spacing.
///
/// The reader only parses; downloading is handled by UkEaLidarTileDownloader.
class UkEaLidarTileReader {
public:
    /// Auto detects the format from the file extension (".xyz" -> XYZ,
    /// everything else -> ASCII grid) and reads the tile.
    static bool readFile(const std::string& path, BngGridTile& tile, std::string* error = nullptr);

    static bool readAsciiGrid(std::istream& stream, BngGridTile& tile, std::string* error = nullptr);

    static bool readXyz(std::istream& stream, BngGridTile& tile, std::string* error = nullptr);
};

} // namespace geo
