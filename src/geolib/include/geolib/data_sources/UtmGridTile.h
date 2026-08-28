#pragma once

#include <vector>

namespace geo {

/// A single elevation raster tile given in an arbitrary northern hemisphere
/// UTM zone (e.g. EPSG:269xx for the NAD83 based US zones).
///
/// Like Utm32GridTile and BngGridTile this keeps the data in its native
/// projected grid and projects incoming geodetic coordinates before
/// interpolating, so a 1 m raster is never resampled. Unlike Utm32GridTile the
/// zone is part of the tile, which is required for data sets spanning several
/// zones such as the USGS 3DEP products.
///
/// Row 0 is the southernmost row, column 0 the westernmost column.
class UtmGridTile {
public:
    UtmGridTile() = default;
    UtmGridTile(int zone, double originEastingM, double originNorthingM, double cellSizeM,
                int columns, int rows, std::vector<double> heights, double noDataValue = -9999.0);

    bool valid() const { return m_columns > 1 && m_rows > 1; }

    int zone() const { return m_zone; }
    double originEasting() const { return m_originEastingM; }
    double originNorthing() const { return m_originNorthingM; }
    double cellSize() const { return m_cellSizeM; }
    int columns() const { return m_columns; }
    int rows() const { return m_rows; }

    /// Height at a UTM position of the tile's zone, bilinearly interpolated.
    bool sampleUtm(double eastingM, double northingM, double& heightM) const;

    /// Height at a geodetic position; projects into the tile's zone first.
    bool sampleGeodetic(double latitudeDeg, double longitudeDeg, double& heightM) const;

private:
    bool cellValue(int column, int row, double& value) const;

    int m_zone{1};
    double m_originEastingM{0.0};
    double m_originNorthingM{0.0};
    double m_cellSizeM{1.0};
    int m_columns{0};
    int m_rows{0};
    std::vector<double> m_heights;
    double m_noDataValue{-9999.0};
};

} // namespace geo
