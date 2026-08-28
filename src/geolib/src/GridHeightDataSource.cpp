#include "geolib/GridHeightDataSource.h"

#include <cmath>
#include <utility>

namespace geo {

GridHeightDataSource::GridHeightDataSource(std::string name, GeoBounds bounds, int columns,
                                           int rows, std::vector<double> heights,
                                           double resolutionM, double noDataValue)
    : m_name(std::move(name)),
      m_bounds(bounds),
      m_columns(columns),
      m_rows(rows),
      m_heights(std::move(heights)),
      m_resolutionM(resolutionM),
      m_noDataValue(noDataValue)
{
}

bool GridHeightDataSource::cellValue(int column, int row, double& value) const
{
    if (column < 0 || row < 0 || column >= m_columns || row >= m_rows) {
        return false;
    }
    const double v = m_heights[static_cast<std::size_t>(row) * m_columns + column];
    if (std::isnan(v) || v == m_noDataValue) {
        return false;
    }
    value = v;
    return true;
}

bool GridHeightDataSource::sampleHeight(double latitudeDeg, double longitudeDeg,
                                        double& heightM) const
{
    if (m_columns < 2 || m_rows < 2 || !m_bounds.contains(latitudeDeg, longitudeDeg)) {
        return false;
    }

    const double lonSpan = m_bounds.maxLongitudeDeg - m_bounds.minLongitudeDeg;
    const double latSpan = m_bounds.maxLatitudeDeg - m_bounds.minLatitudeDeg;
    if (lonSpan <= 0.0 || latSpan <= 0.0) {
        return false;
    }

    // Fractional raster coordinates; row 0 is the northern edge.
    const double fx = (longitudeDeg - m_bounds.minLongitudeDeg) / lonSpan * (m_columns - 1);
    const double fy = (m_bounds.maxLatitudeDeg - latitudeDeg) / latSpan * (m_rows - 1);

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = (x0 + 1 < m_columns) ? x0 + 1 : x0;
    const int y1 = (y0 + 1 < m_rows) ? y0 + 1 : y0;
    const double tx = fx - x0;
    const double ty = fy - y0;

    double h00 = 0.0;
    double h10 = 0.0;
    double h01 = 0.0;
    double h11 = 0.0;
    if (!cellValue(x0, y0, h00) || !cellValue(x1, y0, h10) || !cellValue(x0, y1, h01) ||
        !cellValue(x1, y1, h11)) {
        return false;
    }

    const double top = h00 + (h10 - h00) * tx;
    const double bottom = h01 + (h11 - h01) * tx;
    heightM = top + (bottom - top) * ty;
    return true;
}

} // namespace geo
