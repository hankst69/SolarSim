#include "geolib/data_sources/UtmGridTile.h"

#include "geolib/UtmProjection.h"

#include <cmath>
#include <utility>

namespace geo {

UtmGridTile::UtmGridTile(int zone, double originEastingM, double originNorthingM, double cellSizeM,
                         int columns, int rows, std::vector<double> heights, double noDataValue)
    : m_zone(zone),
      m_originEastingM(originEastingM),
      m_originNorthingM(originNorthingM),
      m_cellSizeM(cellSizeM),
      m_columns(columns),
      m_rows(rows),
      m_heights(std::move(heights)),
      m_noDataValue(noDataValue)
{
}

bool UtmGridTile::cellValue(int column, int row, double& value) const
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

bool UtmGridTile::sampleUtm(double eastingM, double northingM, double& heightM) const
{
    if (!valid() || m_cellSizeM <= 0.0) {
        return false;
    }

    const double fx = (eastingM - m_originEastingM) / m_cellSizeM;
    const double fy = (northingM - m_originNorthingM) / m_cellSizeM;
    if (fx < 0.0 || fy < 0.0 || fx > (m_columns - 1) || fy > (m_rows - 1)) {
        return false;
    }

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

    const double bottom = h00 + (h10 - h00) * tx;
    const double top = h01 + (h11 - h01) * tx;
    heightM = bottom + (top - bottom) * ty;
    return true;
}

bool UtmGridTile::sampleGeodetic(double latitudeDeg, double longitudeDeg, double& heightM) const
{
    double easting = 0.0;
    double northing = 0.0;
    UtmProjection(m_zone).forward(latitudeDeg, longitudeDeg, easting, northing);
    return sampleUtm(easting, northing, heightM);
}

} // namespace geo
