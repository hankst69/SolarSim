#pragma once

#include "geolib/GroundPlane.h"
#include "geolib/HeightDataSource.h"
#include "geolib/HorizonDome.h"
#include "geolib/TriangleMesh.h"
#include "geolib/Vector3.h"

#include <cstddef>
#include <vector>

namespace geo {

/// Height field of the GroundPlane, built from a HeightDataSource and limited
/// to the extent of a HorizonDome. The terrain is sampled on a regular grid in
/// the local east/north frame of the ground plane; heights are stored relative
/// to the ground plane (i.e. earth curvature drop is already applied).
///
/// The resulting triangle mesh can be used to cast shadows: for a given sun
/// direction, isInShadow() traces a ray towards the sun and reports whether the
/// terrain (or an additional building model) blocks it.
class TerrainModel {
public:
    struct Config {
        /// Extent of the modelled area, measured from the standpoint in metres.
        /// Values <= 0 mean "use the radius of the horizon dome".
        double extentM{0.0};
        /// Distance between two grid samples in metres.
        double gridSpacingM{1.0};
        /// Maximum number of samples per axis; the spacing is coarsened if the
        /// requested extent/spacing would exceed it.
        std::size_t maxSamplesPerAxis{1024};
        /// Cut the modelled area to a circle (dome footprint) instead of a
        /// square.
        bool clipToDomeCircle{true};
        /// Subtract the earth curvature drop from the sampled heights.
        bool applyCurvatureDrop{true};
    };

    TerrainModel(const HorizonDome& dome, HeightDataSourcePtr source, const Config& config = {});

    const HorizonDome& dome() const { return m_dome; }
    const GroundPlane& groundPlane() const { return m_dome.groundPlane(); }
    const HeightDataSourcePtr& source() const { return m_source; }
    const Config& config() const { return m_config; }

    /// Extent (half size) of the modelled area in metres.
    double extent() const { return m_extentM; }
    double gridSpacing() const { return m_spacingM; }
    std::size_t samplesPerAxis() const { return m_samples; }

    /// True if at least one grid sample could be read from the data source.
    bool hasHeightData() const { return m_validSamples > 0; }

    /// Height of the terrain above the ground plane at the given local
    /// east/north offset (bilinear interpolation of the sampled grid).
    bool heightAt(double eastM, double northM, double& heightM) const;

    /// Terrain surface point in local east/north/up coordinates.
    bool surfacePoint(double eastM, double northM, Vector3& point) const;

    /// Terrain mesh (local east/north/up frame, metres).
    const TriangleMesh& terrainMesh() const { return m_terrainMesh; }

    /// Optional detailed building model placed in the centre of the ground
    /// plane. The mesh is expected in local east/north/up coordinates relative
    /// to its own origin; it is lifted onto the terrain height of the given
    /// footprint centre.
    void setBuildingModel(const TriangleMesh& mesh, double eastM = 0.0, double northM = 0.0);

    /// Convenience: place a simple box shaped building of the given footprint
    /// and height in the centre of the ground plane.
    void setBuildingBox(double widthEastM, double widthNorthM, double heightM,
                        double eastM = 0.0, double northM = 0.0);

    void clearBuildingModel();
    bool hasBuildingModel() const { return !m_buildingMesh.empty(); }
    const TriangleMesh& buildingMesh() const { return m_buildingMesh; }

    /// Terrain and building combined, ready for rendering or ray casting.
    const TriangleMesh& sceneMesh() const { return m_sceneMesh; }

    /// True if the sun direction (unit vector in the local east/north/up frame,
    /// e.g. SunPosition::direction()) is blocked at the given local point.
    bool isInShadow(const Vector3& localPoint, const Vector3& sunDirection) const;

    /// True if the terrain surface at the given east/north offset is shadowed.
    bool isSurfaceInShadow(double eastM, double northM, const Vector3& sunDirection) const;

private:
    void sampleHeights();
    void buildTerrainMesh();
    void rebuildSceneMesh();
    bool gridHeight(std::size_t column, std::size_t row, double& heightM) const;

    HorizonDome m_dome;
    HeightDataSourcePtr m_source;
    Config m_config;
    double m_extentM{0.0};
    double m_spacingM{1.0};
    std::size_t m_samples{0};
    std::size_t m_validSamples{0};
    std::vector<double> m_heights;
    std::vector<bool> m_valid;
    TriangleMesh m_terrainMesh;
    TriangleMesh m_buildingMesh;
    TriangleMesh m_sceneMesh;
};

} // namespace geo
