#include "geolib/TerrainModel.h"

#include "geolib/GeoLocation.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace geo {

TerrainModel::TerrainModel(const HorizonDome& dome, HeightDataSourcePtr source,
                           const Config& config)
    : m_dome(dome), m_source(std::move(source)), m_config(config)
{
    // Limit the model to the extent of the ground plane covered by the dome.
    m_extentM = (m_config.extentM > 0.0) ? std::min(m_config.extentM, m_dome.radius())
                                         : m_dome.radius();
    m_spacingM = (m_config.gridSpacingM > 0.0) ? m_config.gridSpacingM : 1.0;

    const std::size_t maxSamples = std::max<std::size_t>(m_config.maxSamplesPerAxis, 2);
    std::size_t samples =
        static_cast<std::size_t>(std::floor(2.0 * m_extentM / m_spacingM)) + 1;
    if (samples > maxSamples) {
        samples = maxSamples;
        m_spacingM = 2.0 * m_extentM / static_cast<double>(samples - 1);
    }
    m_samples = std::max<std::size_t>(samples, 2);

    sampleHeights();
    buildTerrainMesh();
    rebuildSceneMesh();
}

void TerrainModel::sampleHeights()
{
    const std::size_t count = m_samples * m_samples;
    m_heights.assign(count, 0.0);
    m_valid.assign(count, false);
    m_validSamples = 0;
    if (!m_source) {
        return;
    }

    const GroundPlane& plane = groundPlane();
    const double originAltitude = plane.origin().altitude();
    const double radius = m_extentM;

    for (std::size_t row = 0; row < m_samples; ++row) {
        const double north = -m_extentM + static_cast<double>(row) * m_spacingM;
        for (std::size_t column = 0; column < m_samples; ++column) {
            const double east = -m_extentM + static_cast<double>(column) * m_spacingM;
            if (m_config.clipToDomeCircle && (east * east + north * north) > radius * radius) {
                continue;
            }

            const GeoLocation location = plane.toGeoLocation(Vector3{east, north, 0.0});
            double altitude = 0.0;
            if (!m_source->sampleHeight(location.latitude(), location.longitude(), altitude)) {
                continue;
            }

            double height = altitude - originAltitude;
            if (m_config.applyCurvatureDrop) {
                height -= plane.curvatureDrop(east, north);
            }

            const std::size_t index = row * m_samples + column;
            m_heights[index] = height;
            m_valid[index] = true;
            ++m_validSamples;
        }
    }
}

bool TerrainModel::gridHeight(std::size_t column, std::size_t row, double& heightM) const
{
    if (column >= m_samples || row >= m_samples) {
        return false;
    }
    const std::size_t index = row * m_samples + column;
    if (!m_valid[index]) {
        return false;
    }
    heightM = m_heights[index];
    return true;
}

void TerrainModel::buildTerrainMesh()
{
    m_terrainMesh.clear();
    if (m_validSamples == 0) {
        return;
    }

    std::vector<std::size_t> indices(m_samples * m_samples, 0);
    std::vector<bool> hasVertex(m_samples * m_samples, false);
    m_terrainMesh.reserve(m_validSamples, 2 * m_validSamples);

    for (std::size_t row = 0; row < m_samples; ++row) {
        const double north = -m_extentM + static_cast<double>(row) * m_spacingM;
        for (std::size_t column = 0; column < m_samples; ++column) {
            double height = 0.0;
            if (!gridHeight(column, row, height)) {
                continue;
            }
            const double east = -m_extentM + static_cast<double>(column) * m_spacingM;
            const std::size_t flat = row * m_samples + column;
            indices[flat] = m_terrainMesh.addVertex(Vector3{east, north, height});
            hasVertex[flat] = true;
        }
    }

    for (std::size_t row = 0; row + 1 < m_samples; ++row) {
        for (std::size_t column = 0; column + 1 < m_samples; ++column) {
            const std::size_t i00 = row * m_samples + column;
            const std::size_t i10 = row * m_samples + column + 1;
            const std::size_t i01 = (row + 1) * m_samples + column;
            const std::size_t i11 = (row + 1) * m_samples + column + 1;
            if (hasVertex[i00] && hasVertex[i10] && hasVertex[i11]) {
                m_terrainMesh.addTriangle(indices[i00], indices[i10], indices[i11]);
            }
            if (hasVertex[i00] && hasVertex[i11] && hasVertex[i01]) {
                m_terrainMesh.addTriangle(indices[i00], indices[i11], indices[i01]);
            }
        }
    }
}

void TerrainModel::rebuildSceneMesh()
{
    m_sceneMesh.clear();
    m_sceneMesh.append(m_terrainMesh);
    m_sceneMesh.append(m_buildingMesh);
}

bool TerrainModel::heightAt(double eastM, double northM, double& heightM) const
{
    if (m_samples < 2 || m_validSamples == 0) {
        return false;
    }
    if (std::fabs(eastM) > m_extentM || std::fabs(northM) > m_extentM) {
        return false;
    }

    const double fx = (eastM + m_extentM) / m_spacingM;
    const double fy = (northM + m_extentM) / m_spacingM;
    const double maxIndex = static_cast<double>(m_samples - 1);
    const auto clampIndex = [&](double f) {
        return static_cast<std::size_t>(std::min(std::max(std::floor(f), 0.0), maxIndex - 1.0));
    };
    const std::size_t x0 = clampIndex(fx);
    const std::size_t y0 = clampIndex(fy);
    const double tx = fx - static_cast<double>(x0);
    const double ty = fy - static_cast<double>(y0);

    double h00 = 0.0;
    double h10 = 0.0;
    double h01 = 0.0;
    double h11 = 0.0;
    if (!gridHeight(x0, y0, h00) || !gridHeight(x0 + 1, y0, h10) ||
        !gridHeight(x0, y0 + 1, h01) || !gridHeight(x0 + 1, y0 + 1, h11)) {
        return false;
    }

    const double bottom = h00 + (h10 - h00) * tx;
    const double top = h01 + (h11 - h01) * tx;
    heightM = bottom + (top - bottom) * ty;
    return true;
}

bool TerrainModel::surfacePoint(double eastM, double northM, Vector3& point) const
{
    double height = 0.0;
    if (!heightAt(eastM, northM, height)) {
        return false;
    }
    point = Vector3{eastM, northM, height};
    return true;
}

void TerrainModel::setBuildingModel(const TriangleMesh& mesh, double eastM, double northM)
{
    double baseHeight = 0.0;
    if (!heightAt(eastM, northM, baseHeight)) {
        baseHeight = 0.0;
    }
    m_buildingMesh.clear();
    m_buildingMesh.append(mesh, Vector3{eastM, northM, baseHeight});
    rebuildSceneMesh();
}

void TerrainModel::setBuildingBox(double widthEastM, double widthNorthM, double heightM,
                                  double eastM, double northM)
{
    const TriangleMesh box = TriangleMesh::createBox(
        Vector3{-0.5 * widthEastM, -0.5 * widthNorthM, 0.0},
        Vector3{0.5 * widthEastM, 0.5 * widthNorthM, heightM});
    setBuildingModel(box, eastM, northM);
}

void TerrainModel::clearBuildingModel()
{
    m_buildingMesh.clear();
    rebuildSceneMesh();
}

bool TerrainModel::isInShadow(const Vector3& localPoint, const Vector3& sunDirection) const
{
    if (sunDirection.z <= 0.0) {
        return true; // sun below the ground plane
    }
    // Start slightly above the surface to avoid self intersection.
    const Vector3 origin{localPoint.x, localPoint.y, localPoint.z + 1e-3};
    return m_sceneMesh.isOccluded(origin, sunDirection, 1e-3, 4.0 * m_extentM + 1.0);
}

bool TerrainModel::isSurfaceInShadow(double eastM, double northM,
                                     const Vector3& sunDirection) const
{
    Vector3 point;
    if (!surfacePoint(eastM, northM, point)) {
        return false;
    }
    return isInShadow(point, sunDirection);
}

} // namespace geo
