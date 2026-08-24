#pragma once

#include <string>
#include <vector>

namespace geo {

/// A single elevation raster tile given in UTM zone 32N (EPSG:25832).
///
/// Unlike GridHeightDataSource (which works on a latitude/longitude raster),
/// this tile keeps its native projected grid and projects incoming geodetic
/// coordinates into UTM before interpolating. That avoids resampling the data
/// and keeps the 1 m grid of the DGM1 exact.
///
/// Row 0 is the southernmost row, column 0 the westernmost column, matching the
/// ordering of the official XYZ files.
class Utm32GridTile {
public:
    Utm32GridTile() = default;
    Utm32GridTile(double originEastingM, double originNorthingM, double cellSizeM, int columns,
                  int rows, std::vector<double> heights, double noDataValue = -9999.0);

    bool valid() const { return m_columns > 1 && m_rows > 1; }

    double originEasting() const { return m_originEastingM; }
    double originNorthing() const { return m_originNorthingM; }
    double cellSize() const { return m_cellSizeM; }
    int columns() const { return m_columns; }
    int rows() const { return m_rows; }

    /// Height at a UTM32 position, bilinearly interpolated.
    bool sampleUtm(double eastingM, double northingM, double& heightM) const;

    /// Height at a geodetic position; projects to UTM32 first.
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
