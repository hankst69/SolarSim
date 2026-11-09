#include "SceneView.h"

#include "geolib/CameraPosition.h"
#include "geolib/DateTimeUtc.h"
#include "geolib/GridHeightDataSource.h"
#include "geolib/HorizonDome.h"
#include "geolib/TerrainModel.h"

#include "TestSupport.h"

#include <QApplication>
#include <QResizeEvent>

#include <memory>

namespace {

    geo::GeoLocation homeLocation()
    {
        return geo::GeoLocation(49.56255, 11.14493);
    }

    std::shared_ptr<geo::TerrainModel> makeTerrain()
    {
        const geo::HorizonDome dome(homeLocation());

        geo::TerrainModel::Config config;
        config.extentM = 100.0;
        config.gridSpacingM = 20.0;
        config.clipToDomeCircle = false;

        return std::make_shared<geo::TerrainModel>(
            dome, std::make_shared<geo::FlatHeightDataSource>(), config);
    }

    /// A freshly constructed view has no camera and no terrain, and must not
    /// crash when asked to render. Shadow simulation defaults to off so the
    /// initial preview is cheap.
    void testInitialStateIsEmpty()
    {
        SceneView view;
        CHECK_TRUE(view.camera() == nullptr);
        CHECK_TRUE(view.shadowsEnabled() == false);

        view.resize(320, 240);
        view.repaint();
    }

    /// setTerrain()/setCamera()/setDateTime() take effect and camera() reports
    /// back the camera that was set.
    void testSetTerrainAndCamera()
    {
        SceneView view;
        view.resize(320, 240);

        const std::shared_ptr<geo::TerrainModel> terrain = makeTerrain();
        view.setTerrain(terrain);

        const geo::CameraPosition camera =
            geo::CameraPosition::fromOrbit(terrain->dome(), 180.0, 30.0, 200.0);
        view.setCamera(camera);

        CHECK_TRUE(view.camera() != nullptr);
        CHECK_NEAR(view.camera()->azimuth(),  180.0, 1e-6);
        CHECK_NEAR(view.camera()->elevation(), 30.0, 0.001);
        CHECK_NEAR(view.camera()->range(),    200.0, 0.005);

        view.setDateTime(geo::DateTimeUtc(2024, 6, 21, 12, 0, 0.0));

        // Painting must not crash with a fully configured scene.
        view.repaint();
    }

    /// setShadowsEnabled() toggles the expensive full lighting simulation on
    /// top of the cheap preview shading; both states must render without
    /// crashing and the accessor must reflect the last value set.
    void testSetShadowsEnabled()
    {
        SceneView view;
        view.resize(320, 240);

        const std::shared_ptr<geo::TerrainModel> terrain = makeTerrain();
        view.setTerrain(terrain);

        const geo::CameraPosition camera =
            geo::CameraPosition::fromOrbit(terrain->dome(), 180.0, 30.0, 200.0);
        view.setCamera(camera);
        view.setDateTime(geo::DateTimeUtc(2024, 6, 21, 12, 0, 0.0));

        CHECK_TRUE(view.shadowsEnabled() == false);
        view.repaint();

        view.setShadowsEnabled(true);
        CHECK_TRUE(view.shadowsEnabled() == true);
        view.repaint();

        view.setShadowsEnabled(false);
        CHECK_TRUE(view.shadowsEnabled() == false);
        view.repaint();
    }

} // namespace


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    testInitialStateIsEmpty();
    testSetTerrainAndCamera();
    testSetShadowsEnabled();

    return geotest::summarize("SceneViewTests");
}
