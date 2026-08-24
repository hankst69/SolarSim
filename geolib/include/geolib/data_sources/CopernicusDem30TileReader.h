#pragma once

#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSource.h"

#include <istream>
#include <memory>
#include <string>

namespace geo {

/// Reader for the tile files of the global Copernicus DEM GLO-30 data set.
///
/// Two formats are supported:
///  * HGT  - the plain SRTM style raster many GLO-30 mirrors provide: big
///           endian signed 16 bit samples, north row first, covering exactly
///           one degree square (usually 3601 x 3601 or 1201 x 1201 samples).
///           The file carries no georeference, so the bounds of the tile have
///           to be supplied by the caller.
///  * ASC  - ESRI ASCII grid in degrees, as produced by common conversion
///           tools. The header keys ncols/nrows/xllcorner/yllcorner/cellsize/
///           NODATA_value are evaluated; xllcenter/yllcenter are accepted too.
///
/// The reader only parses; downloading is handled by
/// CopernicusDem30TileDownloader.
class CopernicusDem30TileReader {
public:
    /// Auto detects the format from the file extension (".asc" -> ASCII grid,
    /// everything else -> HGT) and reads the tile. `bounds` is used for the
    /// formats that carry no georeference.
    static std::shared_ptr<GridHeightDataSource> readFile(const std::string& path,
                                                          const GeoBounds& bounds,
                                                          std::string* error = nullptr);

    static std::shared_ptr<GridHeightDataSource> readHgt(std::istream& stream,
                                                         const GeoBounds& bounds,
                                                         std::string* error = nullptr);

    static std::shared_ptr<GridHeightDataSource> readAsciiGrid(std::istream& stream,
                                                               std::string* error = nullptr);
};

} // namespace geo
