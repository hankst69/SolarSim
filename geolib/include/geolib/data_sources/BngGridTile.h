#pragma once

#include <vector>

namespace geo {

/// A single elevation raster tile given on the British National Grid
/// (OSGB36, EPSG:27700).
///
/// Like Utm32GridTile this keeps the data in its native projected grid and
/// projects incoming geodetic coordinates before interpolating, so the 1 m
/// raster of the Environment Agency LIDAR data is never resampled.
///
/// Row 0 is the southernmost row, column 0 the westernmost column.
class BngGridTile {
public:
    BngGridTile() = default;
    BngGridTile(double originEastingM, double originNorthingM, double cellSizeM, int columns,
                int rows, std::vector<double> heights, double noDataValue = -9999.0);

    bool valid() const { return m_columns > 1 && m_rows > 1; }

    double originEasting() const { return m_originEastingM; }
    double originNorthing() const { return m_originNorthingM; }
    double cellSize() const { return m_cellSizeM; }
    int columns() const { return m_columns; }
    int rows() const { return m_rows; }

    /// Height at a British National Grid position, bilinearly interpolated.
    bool sampleBng(double eastingM, double northingM, double& heightM) const;

    /// Height at a geodetic position; projects to the National Grid first.
    bool sampleGeodetic(double latitudeDeg, double longitudeDeg, double& heightM) const;

private:
    bool cellValue(int column, int row, double& value) const;

    double m_originEastingM{0.0};
    double m_originNorthingM{0.0};
    double m_cellSizeM{1.0};
    int m_columns{0};
    int m_rows{0};
    std::vector<double> m_heights;
    double m_noDataValue{-9999.0};
};

} // namespace geo
