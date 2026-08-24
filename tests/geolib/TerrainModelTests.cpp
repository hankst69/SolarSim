#include "geolib/TerrainModel.h"

#include "TestSupport.h"

#include "geolib/GridHeightDataSource.h"

#include <cmath>
#include <memory>
#include <string>

using namespace geo;

namespace {

constexpr double kMunichLat = 48.1372;
constexpr double kMunichLon = 11.5756;
constexpr double kBaseAltitude = 500.0;

/// Analytic height source: altitude = base + slope * (metres north of origin).
class SlopeSource : public HeightDataSource {
public:
    explicit SlopeSource(double slope = 0.0, double base = kBaseAltitude)
        : m_slope(slope), m_base(base)
    {
    }

    std::string name() const override { return "slope"; }
    GeoBounds coverage() const override { return GeoBounds::world(); }
    double resolutionM() const override { return 1.0; }

    bool sampleHeight(double latitudeDeg, double, double& heightM) const override
    {
        // About 111320 m per degree of latitude.
        heightM = m_base + m_slope * (latitudeDeg - kMunichLat) * 111320.0;
        return true;
    }

private:
    double m_slope;
    double m_base;
};

/// Source that reports a data gap inside a circle around the origin.
class HoleSource : public HeightDataSource {
public:
    std::string name() const override { return "hole"; }
    GeoBounds coverage() const override { return GeoBounds::world(); }
    double resolutionM() const override { return 1.0; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg, double& heightM) const override
    {
        const double north = (latitudeDeg - kMunichLat) * 111320.0;
        const double east = (longitudeDeg - kMunichLon) * 74000.0;
        if (std::sqrt(north * north + east * east) < 30.0) {
            return false;
        }
        heightM = kBaseAltitude;
        return true;
    }
};

HorizonDome dome()
{
    return HorizonDome(GeoLocation(kMunichLat, kMunichLon, kBaseAltitude));
}

TerrainModel::Config config(double extent = 100.0, double spacing = 10.0)
{
    TerrainModel::Config c;
    c.extentM = extent;
    c.gridSpacingM = spacing;
    return c;
}

void testFlatTerrainGeometry()
{
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(0.0), config());

    CHECK_NEAR(terrain.extent(), 100.0, 1e-9);
    CHECK_NEAR(terrain.gridSpacing(), 10.0, 1e-9);
    CHECK_EQ_INT(static_cast<long long>(terrain.samplesPerAxis()), 21);
    CHECK_TRUE(terrain.hasHeightData());

    // The source altitude equals the plane origin altitude, so the terrain sits
    // at height zero at the standpoint.
    double height = 0.0;
    CHECK_TRUE(terrain.heightAt(0.0, 0.0, height));
    CHECK_NEAR(height, 0.0, 1e-6);

    // Away from the origin the curvature drop pulls the surface down slightly.
    // The circular clip leaves the stencil at the very rim incomplete, so a
    // point just inside the extent is used.
    CHECK_TRUE(terrain.heightAt(80.0, 0.0, height));
    CHECK_TRUE(height < 0.0);
    CHECK_TRUE(height > -0.01);
}

void testExtentLimitedByDome()
{
    // A huge requested extent is clamped to the dome radius.
    TerrainModel::Config big = config(1e9, 500.0);
    const HorizonDome d = dome();
    const TerrainModel terrain(d, std::make_shared<SlopeSource>(), big);
    CHECK_NEAR(terrain.extent(), d.radius(), 1e-6);

    // Extent 0 means "use the dome radius".
    TerrainModel::Config automatic;
    automatic.extentM = 0.0;
    automatic.gridSpacingM = 500.0;
    const TerrainModel auto_(d, std::make_shared<SlopeSource>(), automatic);
    CHECK_NEAR(auto_.extent(), d.radius(), 1e-6);
}

void testMaxSamplesCoarsensSpacing()
{
    TerrainModel::Config c = config(1000.0, 1.0);
    c.maxSamplesPerAxis = 51;
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), c);

    CHECK_EQ_INT(static_cast<long long>(terrain.samplesPerAxis()), 51);
    // 2 * 1000 m across 50 intervals.
    CHECK_NEAR(terrain.gridSpacing(), 40.0, 1e-9);
}

void testHeightAtOutOfRange()
{
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    double height = 0.0;
    CHECK_FALSE(terrain.heightAt(200.0, 0.0, height));
    CHECK_FALSE(terrain.heightAt(0.0, -150.0, height));
    // The corner of the square lies outside the clipped circle.
    CHECK_FALSE(terrain.heightAt(100.0, 100.0, height));
    CHECK_TRUE(terrain.heightAt(50.0, 50.0, height));
}

void testSlopedTerrain()
{
    // 10 % slope rising towards the north.
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(0.1), config());

    double atOrigin = 0.0;
    double atNorth = 0.0;
    double atSouth = 0.0;
    CHECK_TRUE(terrain.heightAt(0.0, 0.0, atOrigin));
    CHECK_TRUE(terrain.heightAt(0.0, 50.0, atNorth));
    CHECK_TRUE(terrain.heightAt(0.0, -50.0, atSouth));

    CHECK_NEAR(atOrigin, 0.0, 0.01);
    CHECK_NEAR(atNorth, 5.0, 0.05);
    CHECK_NEAR(atSouth, -5.0, 0.05);
}

void testSurfacePoint()
{
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(0.1), config());
    Vector3 point;
    CHECK_TRUE(terrain.surfacePoint(0.0, 50.0, point));
    CHECK_NEAR(point.x, 0.0, 1e-9);
    CHECK_NEAR(point.y, 50.0, 1e-9);
    CHECK_NEAR(point.z, 5.0, 0.05);

    CHECK_FALSE(terrain.surfacePoint(500.0, 0.0, point));
}

void testMeshIsBuilt()
{
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    const TriangleMesh& mesh = terrain.terrainMesh();
    CHECK_FALSE(mesh.empty());

    // A full 21x21 grid clipped to a circle keeps most, but not all, cells.
    CHECK_TRUE(mesh.vertices().size() > 300);
    CHECK_TRUE(mesh.vertices().size() <= 21 * 21);

    Vector3 lo;
    Vector3 hi;
    CHECK_TRUE(mesh.bounds(lo, hi));
    CHECK_TRUE(lo.x >= -100.0 - 1e-6);
    CHECK_TRUE(hi.x <= 100.0 + 1e-6);

    // Without a building the scene equals the terrain.
    CHECK_EQ_INT(static_cast<long long>(terrain.sceneMesh().triangles().size()),
                 static_cast<long long>(mesh.triangles().size()));
}

void testClipToDomeCircle()
{
    TerrainModel::Config square = config();
    square.clipToDomeCircle = false;
    const TerrainModel full(dome(), std::make_shared<SlopeSource>(), square);
    CHECK_EQ_INT(static_cast<long long>(full.terrainMesh().vertices().size()), 21 * 21);

    // The circular clip must drop the corners.
    const TerrainModel circular(dome(), std::make_shared<SlopeSource>(), config());
    CHECK_TRUE(circular.terrainMesh().vertices().size() < full.terrainMesh().vertices().size());
}

void testDataGapsProduceHoles()
{
    const TerrainModel terrain(dome(), std::make_shared<HoleSource>(), config());
    CHECK_TRUE(terrain.hasHeightData());

    // Inside the gap no height is available.
    double height = 0.0;
    CHECK_FALSE(terrain.heightAt(0.0, 0.0, height));
    // Outside it the data is fine.
    CHECK_TRUE(terrain.heightAt(80.0, 0.0, height));

    // The mesh has fewer vertices than the full grid.
    CHECK_TRUE(terrain.terrainMesh().vertices().size() < 21 * 21);
}

void testNullSourceYieldsNoData()
{
    const TerrainModel terrain(dome(), nullptr, config());
    CHECK_FALSE(terrain.hasHeightData());
    CHECK_TRUE(terrain.terrainMesh().empty());

    double height = 0.0;
    CHECK_FALSE(terrain.heightAt(0.0, 0.0, height));
}

void testBuildingBox()
{
    TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    CHECK_FALSE(terrain.hasBuildingModel());
    const std::size_t terrainTriangles = terrain.sceneMesh().triangles().size();

    terrain.setBuildingBox(10.0, 6.0, 8.0);
    CHECK_TRUE(terrain.hasBuildingModel());
    CHECK_EQ_INT(static_cast<long long>(terrain.buildingMesh().triangles().size()), 12);
    CHECK_EQ_INT(static_cast<long long>(terrain.sceneMesh().triangles().size()),
                 static_cast<long long>(terrainTriangles + 12));

    // Footprint centred on the origin, rising 8 m above the local terrain.
    Vector3 lo;
    Vector3 hi;
    CHECK_TRUE(terrain.buildingMesh().bounds(lo, hi));
    CHECK_NEAR(lo.x, -5.0, 1e-6);
    CHECK_NEAR(hi.x, 5.0, 1e-6);
    CHECK_NEAR(lo.y, -3.0, 1e-6);
    CHECK_NEAR(hi.y, 3.0, 1e-6);
    CHECK_NEAR(hi.z - lo.z, 8.0, 1e-6);

    terrain.clearBuildingModel();
    CHECK_FALSE(terrain.hasBuildingModel());
    CHECK_EQ_INT(static_cast<long long>(terrain.sceneMesh().triangles().size()),
                 static_cast<long long>(terrainTriangles));
}

/// The building must be lifted onto the terrain height of its footprint.
void testBuildingSitsOnSlope()
{
    TerrainModel terrain(dome(), std::make_shared<SlopeSource>(0.1), config());
    terrain.setBuildingBox(4.0, 4.0, 5.0, 0.0, 50.0);

    Vector3 lo;
    Vector3 hi;
    CHECK_TRUE(terrain.buildingMesh().bounds(lo, hi));
    // Terrain is about 5 m up at 50 m north.
    CHECK_NEAR(lo.z, 5.0, 0.05);
    CHECK_NEAR(hi.z, 10.0, 0.05);
}

void testSetBuildingModel()
{
    TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    const TriangleMesh box = TriangleMesh::createBox({-1.0, -1.0, 0.0}, {1.0, 1.0, 3.0});

    terrain.setBuildingModel(box, 20.0, -30.0);
    Vector3 lo;
    Vector3 hi;
    CHECK_TRUE(terrain.buildingMesh().bounds(lo, hi));
    CHECK_NEAR(lo.x, 19.0, 1e-6);
    CHECK_NEAR(hi.x, 21.0, 1e-6);
    CHECK_NEAR(lo.y, -31.0, 1e-6);
    CHECK_NEAR(hi.y, -29.0, 1e-6);
}

void testShadowBelowHorizon()
{
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    // A sun direction pointing downwards always means shadow.
    CHECK_TRUE(terrain.isInShadow(Vector3{0.0, 0.0, 0.0}, Vector3{0.0, 0.0, -1.0}));
    CHECK_TRUE(terrain.isInShadow(Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0}));
}

void testFlatTerrainIsLit()
{
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    // Sun straight overhead on flat ground: no shadow.
    CHECK_FALSE(terrain.isSurfaceInShadow(0.0, 0.0, Vector3{0.0, 0.0, 1.0}));
    // A moderately high sun is unobstructed as well.
    CHECK_FALSE(
        terrain.isSurfaceInShadow(0.0, 0.0, Vector3(1.0, 0.0, 1.0).normalized()));
}

void testBuildingCastsShadow()
{
    TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    terrain.setBuildingBox(10.0, 10.0, 20.0);

    // Low sun from the east: a point just west of the building is shadowed.
    const Vector3 lowEast = Vector3(1.0, 0.0, 0.3).normalized();
    CHECK_TRUE(terrain.isSurfaceInShadow(-10.0, 0.0, lowEast));

    // The same point is lit when the sun comes from the west instead.
    const Vector3 lowWest = Vector3(-1.0, 0.0, 0.3).normalized();
    CHECK_FALSE(terrain.isSurfaceInShadow(-10.0, 0.0, lowWest));

    // Far away from the building nothing blocks the low sun.
    CHECK_FALSE(terrain.isSurfaceInShadow(-95.0, 90.0, lowEast));
}

void testShadowOutsideModelIsNotReported()
{
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), config());
    // No surface data outside the extent, so no shadow decision is possible.
    CHECK_FALSE(terrain.isSurfaceInShadow(500.0, 0.0, Vector3{0.0, 0.0, 1.0}));
}

void testAccessors()
{
    const auto source = std::make_shared<SlopeSource>();
    const TerrainModel terrain(dome(), source, config());
    CHECK_TRUE(terrain.source() == source);
    CHECK_NEAR(terrain.groundPlane().origin().latitude(), kMunichLat, 1e-12);
    CHECK_NEAR(terrain.dome().viewHeight(), HorizonDome::kDefaultViewHeightM, 1e-12);
    CHECK_NEAR(terrain.config().gridSpacingM, 10.0, 1e-12);
}

void testCurvatureDropCanBeDisabled()
{
    TerrainModel::Config c = config();
    c.applyCurvatureDrop = false;
    const TerrainModel terrain(dome(), std::make_shared<SlopeSource>(), c);

    // Without the drop, flat data stays exactly at zero everywhere.
    double height = 0.0;
    CHECK_TRUE(terrain.heightAt(80.0, 0.0, height));
    CHECK_NEAR(height, 0.0, 1e-9);
}

/// A real grid based source must work as well, not only the analytic ones.
void testWithGridHeightDataSource()
{
    // 1 degree box around Munich, rising towards the north.
    const std::vector<double> heights{
        kBaseAltitude + 20.0, kBaseAltitude + 20.0, kBaseAltitude + 20.0,
        kBaseAltitude,        kBaseAltitude,        kBaseAltitude,
        kBaseAltitude - 20.0, kBaseAltitude - 20.0, kBaseAltitude - 20.0};
    const auto grid = std::make_shared<GridHeightDataSource>(
        "grid", GeoBounds{kMunichLat - 0.5, kMunichLat + 0.5, kMunichLon - 0.5, kMunichLon + 0.5},
        3, 3, heights, 1000.0);

    const TerrainModel terrain(dome(), grid, config(1000.0, 100.0));
    CHECK_TRUE(terrain.hasHeightData());

    double atOrigin = 0.0;
    double atNorth = 0.0;
    CHECK_TRUE(terrain.heightAt(0.0, 0.0, atOrigin));
    CHECK_TRUE(terrain.heightAt(0.0, 800.0, atNorth));
    CHECK_NEAR(atOrigin, 0.0, 0.5);
    CHECK_TRUE(atNorth > atOrigin);
}

} // namespace

int main()
{
    testFlatTerrainGeometry();
    testExtentLimitedByDome();
    testMaxSamplesCoarsensSpacing();
    testHeightAtOutOfRange();
    testSlopedTerrain();
    testSurfacePoint();
    testMeshIsBuilt();
    testClipToDomeCircle();
    testDataGapsProduceHoles();
    testNullSourceYieldsNoData();
    testBuildingBox();
    testBuildingSitsOnSlope();
    testSetBuildingModel();
    testShadowBelowHorizon();
    testFlatTerrainIsLit();
    testBuildingCastsShadow();
    testShadowOutsideModelIsNotReported();
    testAccessors();
    testCurvatureDropCanBeDisabled();
    testWithGridHeightDataSource();
    return geotest::summarize("TerrainModelTests");
}
