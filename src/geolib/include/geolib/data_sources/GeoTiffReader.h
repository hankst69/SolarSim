#pragma once

#include "geolib/GridHeightDataSource.h"

#include <cstdint>
#include <istream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace geo {

/// Minimal, dependency free reader for (Geo)TIFF raster files.
///
/// Only the subset needed to read single band elevation data as found in
/// Cloud Optimized GeoTIFFs (e.g. Copernicus DEM GLO-30, many USGS 3DEP
/// products, ...) is implemented:
///  - a single band of UInt16/Int16, UInt32/Int32/Float32, or Float64 samples
///  - uncompressed or Deflate/(Adobe-)Deflate/LZW compressed
///  - organised in strips or tiles
///  - optional horizontal differencing predictor
///
/// This class only parses the file into an in-memory raster (plus the raw
/// TIFF tags, for callers that need additional metadata); turning the result
/// into a `GridHeightDataSource` is done by `toGridHeightDataSource()`.
class GeoTiffReader {
public:
    /// One parsed TIFF tag: for numeric types the values are available via
    /// `ints`/`doubles` depending on the original TIFF type.
    struct TagValue {
        std::uint16_t type{0};
        std::vector<std::uint64_t> ints;
        std::vector<double> doubles;

        std::uint64_t asInt(std::size_t index = 0) const
        {
            return index < ints.size() ? ints[index] : 0;
        }
        double asDouble(std::size_t index = 0) const
        {
            if (index < doubles.size()) {
                return doubles[index];
            }
            return index < ints.size() ? static_cast<double>(ints[index]) : 0.0;
        }
    };

    /// Decoded single band raster plus the georeference bounds, derived
    /// either from the GeoTIFF ModelPixelScale/ModelTiepoint tags or from the
    /// fallback bounds passed to `read()`.
    struct Raster {
        int columns{0};
        int rows{0};
        /// Row 0 is the northernmost row (the top row as stored in the TIFF),
        /// matching the layout expected by `GridHeightDataSource`.
        std::vector<double> values;
        /// Geographic bounds, either taken from the GeoTIFF georeference or
        /// the fallback bounds supplied to `read()`.
        GeoBounds bounds;
        /// No-data sentinel value used in `values`.
        double noDataValue{-32768.0};
    };

    /// Parses `stream` into `raster`. `fallbackBounds` is used if the file
    /// carries no (recognised) GeoTIFF georeference tags. Returns false and
    /// sets `*error` on failure.
    bool read(std::istream& stream, const GeoBounds& fallbackBounds, Raster& raster,
                     std::string* error = nullptr);

    /// Convenience wrapper turning a successfully parsed raster into a
    /// `GridHeightDataSource` with the given display name.
    static std::shared_ptr<GridHeightDataSource> toGridHeightDataSource(const Raster& raster,
                                                                        const std::string& name);

    /// Access to the raw parsed tags, keyed by TIFF tag id, for callers that
    /// need to evaluate georeference or metadata tags beyond the default
    /// handling performed by `read()`. Only valid for the file last passed to
    /// `read()`.
    const std::map<std::uint16_t, TagValue>& tags() const { return m_tags; }

private:
    bool parse(const std::vector<char>& bytes, std::string* error);
    bool parseIfd(std::uint32_t ifdOffset, std::string* error);
    void readTypedValue(std::uint16_t type, std::size_t offset, TagValue& value);

    std::uint16_t readU16(std::size_t offset) const;
    std::uint32_t readU32(std::size_t offset) const;

    const std::vector<char>* m_bytes{nullptr};
    bool m_bigEndian{false};
    std::map<std::uint16_t, TagValue> m_tags;
};

} // namespace geo
