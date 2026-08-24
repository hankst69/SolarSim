#pragma once

#include "geolib/data_sources/UtmGridTile.h"

#include <istream>
#include <string>

namespace geo {

/// Reader for the tile files of the USGS open data set "3DEP 1 meter DEM".
///
/// Two text formats are supported:
///  * ASC  - ESRI ASCII grid in the UTM zone of the tile, as produced by the
///           common GeoTIFF conversion tools. The header keys ncols/nrows/
///           xllcorner/yllcorner/cellsize/NODATA_value are evaluated;
///           xllcenter/yllcenter are accepted as well.
///  * XYZ  - one "easting northing height" triple per line; the grid geometry
///           is derived from the sample spacing.
///
/// Neither format carries the UTM zone, so it has to be supplied by the caller
/// (the zone is part of the official tile name).
///
/// The reader only parses; downloading is handled by
/// UsaUsgs3Dep1mTileDownloader.
class UsaUsgs3Dep1mTileReader {
public:
    /// Auto detects the format from the file extension (".xyz" -> XYZ,
    /// everything else -> ASCII grid) and reads the tile.
    static bool readFile(const std::string& path, int zone, UtmGridTile& tile,
                         std::string* error = nullptr);

    static bool readAsciiGrid(std::istream& stream, int zone, UtmGridTile& tile,
                              std::string* error = nullptr);

    static bool readXyz(std::istream& stream, int zone, UtmGridTile& tile,
                        std::string* error = nullptr);
};

} // namespace geo
