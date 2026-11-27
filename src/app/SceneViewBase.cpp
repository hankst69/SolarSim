#include "SceneViewBase.h"

#include "geolib/Angle.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

/// Number of intervals used for sun path sampling (sampleCount + 1 points).
constexpr int kSunPathSampleCount = 144;

/// Scene center cross radius
constexpr double kHalfLengthM = 5.0; // 10m total arm length.

/// Elevation (degrees above the horizon) of the bottom edge of the compass letters.
constexpr double kCompassBaseElevationDeg = 5.0;

/// Letter height as a fraction of the dome radius.
constexpr double kCompassHeightFraction = 0.08;

/// Letter width relative to the letter height.
constexpr double kCompassAspect = 0.7;

/// Half stroke thickness relative to the letter height.
constexpr double kCompassStrokeHalfWidth = 0.02;

struct GlyphStroke {
    double u0, v0, u1, v1; // unit box, u to the right, v upwards
};

/// Line strokes of the letters in a unit box.
std::vector<GlyphStroke> glyphStrokes(char letter)
{
    switch (letter) {
    case 'N':
        return {{0, 0, 0, 1}, {0, 1, 1, 0}, {1, 0, 1, 1}};
    case 'E':
        return {{1, 1, 0, 1}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0.5, 0.8, 0.5}};
    case 'S':
        return {{1, 1, 0, 1}, {0, 1, 0, 0.5}, {0, 0.5, 1, 0.5}, {1, 0.5, 1, 0}, {1, 0, 0, 0}};
    case 'W':
        return {{0, 1, 0.25, 0}, {0.25, 0, 0.5, 0.7}, {0.5, 0.7, 0.75, 0}, {0.75, 0, 1, 1}};
    default:
        return {};
    }
}

} // namespace

std::array<float, 3> SceneViewBase::compassLabelColor(char letter)
{
    switch (letter) {
    case 'N':
        return {1.0f, 0.15f, 0.15f};
    case 'S':
        return {1.0f, 0.85f, 0.1f};
    case 'E':
        return {0.1f, 0.9f, 0.3f};
    default:
        return {0.2f, 0.5f, 1.0f};
    }
}

void SceneViewBase::setTerrain(std::shared_ptr<const geo::TerrainModel> terrain)
{
    m_terrain = std::move(terrain);
    rebuildLight();
    rebuildSunPathOverlay();
    rebuildCenterCrossOverlay();
    rebuildCenterCubeGeometry();
    rebuildCompassLabels();
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

void SceneViewBase::setTerrainHeightExaggeration(double factor)
{
    const double clampedFactor = std::max(1.0, factor);
    if (m_terrainHeightExaggeration == clampedFactor) {
        return;
    }
    m_terrainHeightExaggeration = clampedFactor;
    onTerrainChanged();
}

void SceneViewBase::setCenterCubeVisible(bool visible)
{
    if (m_centerCubeVisible == visible) {
        return;
    }
    m_centerCubeVisible = visible;
    onCenterCubeVisibleChanged();
}

void SceneViewBase::setCompassVisible(bool visible)
{
  if (m_compassVisible == visible) {
    return;
  }
  m_compassVisible = visible;
  onCompassVisibleChanged();
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

void SceneViewBase::rebuildCenterCrossOverlay()
{
    m_centerCrossPoints.clear();
    m_hasCenterCross = false;

    if (!m_terrain) {
        return;
    }

    double heightM = 0.0;
    m_terrain->heightAt(0.0, 0.0, heightM);
    m_centerBaseHeightM = heightM;

    m_centerCrossPoints.push_back(geo::Vector3{-kHalfLengthM, 0.0, heightM});
    m_centerCrossPoints.push_back(geo::Vector3{kHalfLengthM, 0.0, heightM});
    m_centerCrossPoints.push_back(geo::Vector3{0.0, -kHalfLengthM, heightM});
    m_centerCrossPoints.push_back(geo::Vector3{0.0, kHalfLengthM, heightM});
    m_hasCenterCross = true;
}

void SceneViewBase::rebuildCenterCubeGeometry()
{
    m_centerCubeCorners.clear();

    if (!m_terrain) {
        return;
    }

    constexpr double kHalfSizeM = 5.0; // 10m cube edge length.
    const double baseZ = m_centerBaseHeightM;
    const double topZ = baseZ + 2.0 * kHalfSizeM;

    // Bottom face, counter-clockwise seen from above.
    m_centerCubeCorners.push_back(geo::Vector3{-kHalfSizeM, -kHalfSizeM, baseZ});
    m_centerCubeCorners.push_back(geo::Vector3{kHalfSizeM, -kHalfSizeM, baseZ});
    m_centerCubeCorners.push_back(geo::Vector3{kHalfSizeM, kHalfSizeM, baseZ});
    m_centerCubeCorners.push_back(geo::Vector3{-kHalfSizeM, kHalfSizeM, baseZ});

    // Top face, same order.
    m_centerCubeCorners.push_back(geo::Vector3{-kHalfSizeM, -kHalfSizeM, topZ});
    m_centerCubeCorners.push_back(geo::Vector3{kHalfSizeM, -kHalfSizeM, topZ});
    m_centerCubeCorners.push_back(geo::Vector3{kHalfSizeM, kHalfSizeM, topZ});
    m_centerCubeCorners.push_back(geo::Vector3{-kHalfSizeM, kHalfSizeM, topZ});
}

void SceneViewBase::rebuildCompassLabels()
{
    m_compassLabels.clear();

    if (!m_terrain) {
        return;
    }

    const geo::HorizonDome& dome = m_terrain->dome();
    const double letterHeight = dome.radius() * kCompassHeightFraction;
    const double letterWidth = letterHeight * kCompassAspect;
    const double strokeOffset = letterHeight * kCompassStrokeHalfWidth;
    const geo::Vector3 up{0.0, 0.0, 1.0};

    // Offsets used to fake a thick stroke with plain 1px line primitives.
    const double offsets[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    const struct {
        char letter;
        double azimuthDeg;
    } directions[] = {{'N', 0.0}, {'E', 90.0}, {'S', 180.0}, {'W', 270.0}};

    for (const auto& direction : directions) {
        const geo::Vector3 base = dome.pointOnDome(direction.azimuthDeg, kCompassBaseElevationDeg);

        // Horizontal direction to the right when looking from the centre
        // outwards to the letter.
        const double az = geo::degToRad(direction.azimuthDeg);
        const geo::Vector3 right{std::cos(az), -std::sin(az), 0.0};

        CompassLabel label;
        label.letter = direction.letter;

        for (const GlyphStroke& stroke : glyphStrokes(direction.letter)) {
            const geo::Vector3 a = base + right * ((stroke.u0 - 0.5) * letterWidth) +
                                   up * (stroke.v0 * letterHeight);
            const geo::Vector3 b = base + right * ((stroke.u1 - 0.5) * letterWidth) +
                                   up * (stroke.v1 * letterHeight);
            for (const auto& offset : offsets) {
                const geo::Vector3 shift = right * (offset[0] * strokeOffset) +
                                           up * (offset[1] * strokeOffset);
                label.lineList.push_back(a + shift);
                label.lineList.push_back(b + shift);
            }
        }
        m_compassLabels.push_back(std::move(label));
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
