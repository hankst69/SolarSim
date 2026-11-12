#include "SceneViewBase.h"

#include <QMouseEvent>
#include <QWheelEvent>

namespace {

/// Number of intervals used for sun path sampling (sampleCount + 1 points).
constexpr int kSunPathSampleCount = 144;

} // namespace

void SceneViewBase::setTerrain(std::shared_ptr<const geo::TerrainModel> terrain)
{
    m_terrain = std::move(terrain);
    rebuildLight();
    rebuildSunPathOverlay();
    onTerrainChanged();
}

void SceneViewBase::setDateTime(const geo::DateTimeUtc& utc)
{
    m_utc = utc;
    rebuildLight();
    rebuildSunPathOverlay();
    onDateTimeChanged();
}

void SceneViewBase::setCamera(const geo::CameraPosition& camera)
{
    m_camera = std::make_unique<geo::CameraPosition>(camera);
    onCameraChanged();
}

void SceneViewBase::setShadowsEnabled(bool enabled)
{
    if (m_shadowsEnabled == enabled) {
        return;
    }
    m_shadowsEnabled = enabled;
    onShadowsEnabledChanged();
}

void SceneViewBase::rebuildLight()
{
    if (!m_terrain) {
        m_light.reset();
        return;
    }
    m_light = std::make_unique<geo::SunLight>(*m_terrain, m_utc);
}

void SceneViewBase::rebuildSunPathOverlay()
{
    m_sunPathPoints.clear();
    m_hasCurrentSunPoint = false;

    if (!m_terrain) {
        return;
    }

    const geo::HorizonDome& dome = m_terrain->dome();
    const geo::SunPath path(dome, m_utc.year, m_utc.month, m_utc.day, kSunPathSampleCount);

    m_sunPathPoints = path.arcPoints();

    geo::DateTimeUtc sunriseUtc;
    if (path.sunrise(sunriseUtc)) {
        const geo::SunPosition sunrisePos(dome.standpoint(), sunriseUtc);
        m_sunPathPoints.insert(m_sunPathPoints.begin(), sunrisePos.projectOnDome(dome));
    }

    geo::DateTimeUtc sunsetUtc;
    if (path.sunset(sunsetUtc)) {
        const geo::SunPosition sunsetPos(dome.standpoint(), sunsetUtc);
        m_sunPathPoints.push_back(sunsetPos.projectOnDome(dome));
    }

    const geo::SunPosition nowPos(dome.standpoint(), m_utc);
    if (nowPos.isAboveHorizon()) {
        m_currentSunPoint = nowPos.projectOnDome(dome);
        m_hasCurrentSunPoint = true;
    }
}

void SceneViewBase::handleOrbitMousePress(const QPoint& pos)
{
    m_lastMousePos = pos;
}

void SceneViewBase::handleOrbitMouseMove(QMouseEvent* event)
{
    if (!m_camera || !(event->buttons() & Qt::LeftButton)) {
        return;
    }

    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();
    if (delta.isNull()) {
        return;
    }

    setCamera(m_camera->orbited(-delta.x() * 0.4, delta.y() * 0.3));
    emitCameraChanged();
}

void SceneViewBase::handleOrbitWheel(QWheelEvent* event)
{
    if (!m_camera) {
        return;
    }

    const int steps = event->angleDelta().y();
    if (steps == 0) {
        return;
    }

    setCamera(m_camera->zoomed(steps > 0 ? 0.9 : 1.0 / 0.9));
    emitCameraChanged();
    event->accept();
}
