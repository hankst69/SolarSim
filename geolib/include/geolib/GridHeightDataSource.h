#pragma once

#include "geolib/HeightDataSource.h"

#include <string>
#include <vector>

namespace geo {

/// Fallback source delivering a constant height for the whole world.
/// Used when no real elevation data set is available for a location.
class FlatHeightDataSource : public HeightDataSource {
public:
    explicit FlatHeightDataSource(double heightM = 0.0, double resolutionM = 1000.0)
        : m_heightM(heightM), m_resolutionM(resolutionM)
    {
    }

    std::string name() const override { return "Flat terrain (fallback)"; }
    GeoBounds coverage() const override { return GeoBounds::world(); }
    double resolutionM() const override { return m_resolutionM; }

    bool sampleHeight(double, double, double& heightM) const override
    {
        heightM = m_heightM;
        return true;
    }

private:
    double m_heightM{0.0};
    double m_resolutionM{1000.0};
};

/// Height data source backed by a regular latitude/longitude raster held in
/// memory. Concrete data sets (DGM1 tiles, SRTM HGT tiles, GeoTIFF exports,
/// ...) are decoded by their own reader and handed over as a raster.
///
/// Row 0 is the northernmost row, column 0 the westernmost column, which
/// matches the layout of most DEM raster formats.
class GridHeightDataSource : public HeightDataSource {
public:
    GridHeightDataSource(std::string name, GeoBounds bounds, int columns, int rows,
                         std::vector<double> heights, double resolutionM,
                         double noDataValue = -32768.0);

    std::string name() const override { return m_name; }
    GeoBounds coverage() const override { return m_bounds; }
    double resolutionM() const override { return m_resolutionM; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg,
                      double& heightM) const override;

    int columns() const { return m_columns; }
    int rows() const { return m_rows; }

private:
    bool cellValue(int column, int row, double& value) const;

    std::string m_name;
    GeoBounds m_bounds;
    int m_columns{0};
    int m_rows{0};
    std::vector<double> m_heights;
    double m_resolutionM{1.0};
    double m_noDataValue{-32768.0};
};

} // namespace geo
