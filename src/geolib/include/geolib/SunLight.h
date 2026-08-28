#pragma once

#include "geolib/DateTimeUtc.h"
#include "geolib/SunPosition.h"
#include "geolib/TerrainModel.h"
#include "geolib/TriangleMesh.h"
#include "geolib/Vector3.h"

namespace geo {

/// Directional ("area") light source that approximates the sun for a scene.
///
/// The sun is so far away that its rays can be treated as parallel. SunLight
/// therefore models the light as a flat rectangle whose normal is the negated
/// SunPosition::direction() and which is large enough to cover the whole
/// visible scene. All rays leaving the rectangle share the same direction, so a
/// renderer can either use it as a directional light or place an orthographic
/// shadow camera exactly on the rectangle.
class SunLight {
public:
    /// Extra margin added to the radius of the covered scene, as a factor.
    static constexpr double kCoverageMargin = 1.05;

    /// Light for the sun position at the given UTC time, sized so that the
    /// bounding sphere of the scene mesh of the terrain is fully lit.
    SunLight(const TerrainModel& terrain, const DateTimeUtc& utc);

    /// Light for an explicit sun position and an explicit scene radius
    /// (metres) around the local origin.
    SunLight(const SunPosition& sun, double sceneRadiusM, double sceneCentreHeightM = 0.0);

    const SunPosition& sunPosition() const { return m_sun; }
    const DateTimeUtc& time() const { return m_sun.time(); }

    /// True while the sun is above the horizon; below the horizon the light
    /// contributes nothing and only the ambient term remains.
    bool isAboveHorizon() const { return m_sun.elevation() > 0.0; }

    /// Unit vector from the scene towards the sun (local east/north/up).
    Vector3 directionToSun() const { return m_toSun; }

    /// Unit direction the light rays travel in (local east/north/up).
    Vector3 rayDirection() const { return m_toSun * -1.0; }

    /// Centre of the light rectangle in the local east/north/up frame.
    const Vector3& centre() const { return m_centre; }

    /// Half width / half height of the light rectangle in metres; the
    /// rectangle is square and covers the whole scene.
    double halfSize() const { return m_halfSizeM; }

    /// Full edge length of the light rectangle in metres.
    double size() const { return 2.0 * m_halfSizeM; }

    /// Distance of the rectangle from the scene centre along the sun
    /// direction (metres).
    double distance() const { return m_distanceM; }

    /// In plane axes of the light rectangle (unit vectors, perpendicular to
    /// each other and to the sun direction).
    const Vector3& axisU() const { return m_axisU; }
    const Vector3& axisV() const { return m_axisV; }

    /// Corner of the rectangle for u/v in [-1, 1].
    Vector3 cornerAt(double u, double v) const;

    /// Emission point of the ray that hits the given local scene point.
    Vector3 rayOriginFor(const Vector3& localPoint) const;

    /// Relative intensity of the light (cosine of the zenith angle), 0 while
    /// the sun is below the horizon.
    double intensity() const { return m_sun.relativeIrradiance(); }

    /// Near/far plane of an orthographic shadow projection along the rays that
    /// encloses the whole scene.
    double nearPlane() const { return m_distanceM - m_halfSizeM; }
    double farPlane() const { return m_distanceM + m_halfSizeM; }

    /// Radius of the bounding sphere of the given mesh around the local
    /// origin; the light rectangle is sized after it.
    static double coveredRadius(const TriangleMesh& mesh);

private:
    void build(double sceneRadiusM, double sceneCentreHeightM);

    SunPosition m_sun;
    Vector3 m_toSun{0.0, 0.0, 1.0};
    Vector3 m_centre;
    Vector3 m_axisU{1.0, 0.0, 0.0};
    Vector3 m_axisV{0.0, 1.0, 0.0};
    double m_halfSizeM{1.0};
    double m_distanceM{1.0};
};

} // namespace geo
