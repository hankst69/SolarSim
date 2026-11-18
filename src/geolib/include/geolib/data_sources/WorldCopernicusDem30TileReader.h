#pragma once

#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSource.h"

#include <istream>
#include <memory>
#include <string>

namespace geo {

/// Reader for the tile files of the global Copernicus DEM GLO-30 data set.
///
/// Three formats are supported:
///  * TIF/TIFF - the official delivery format: a (Cloud Optimized) GeoTIFF
///           with a single band of Int16 or Float32 samples, uncompressed or
///           Deflate/LZW compressed, organised in strips or tiles. The
///           GeoTIFF georeference keys are evaluated if present, otherwise
///           `bounds` is used as a fallback.
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
/// WorldCopernicusDem30TileDownloader.
class WorldCopernicusDem30TileReader {
public:
    /// Auto detects the format from the file extension (".asc" -> ASCII grid,
    /// ".tif"/".tiff" -> GeoTIFF, everything else -> HGT) and reads the tile.
    /// `bounds` is used as a fallback for formats that carry no (or an
    /// unrecognised) georeference.
    static std::shared_ptr<GridHeightDataSource> readFile(const std::string& path,
                                                          const GeoBounds& bounds,
                                                          std::string* error = nullptr);

    static std::shared_ptr<GridHeightDataSource> readHgt(std::istream& stream,
                                                         const GeoBounds& bounds,
                                                         std::string* error = nullptr);

    static std::shared_ptr<GridHeightDataSource> readAsciiGrid(std::istream& stream,
                                                               std::string* error = nullptr);

    /// Parses a (Cloud Optimized) GeoTIFF tile. `bounds` is used only if the
    /// file carries no usable GeoTIFF georeference tags.
    static std::shared_ptr<GridHeightDataSource> readGeoTiff(std::istream& stream,
                                                             const GeoBounds& bounds,
                                                             std::string* error = nullptr);
};

} // namespace geo
