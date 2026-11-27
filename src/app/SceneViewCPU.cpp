#include "SceneViewCPU.h"

#include "geolib/Angle.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

/// Vertical field of view of the perspective projection in degrees.
constexpr double kFieldOfViewDeg = 55.0;

/// Weak base illumination so that shadowed slopes never turn fully black.
constexpr double kAmbient = 0.18;

/// Diffuse contribution of the weak fill light coming from the sky.
constexpr double kSkyFill = 0.12;

/// Diffuse contribution of the sun at full intensity.
constexpr double kSunDiffuse = 0.85;

/// Base albedo colour of the terrain.
const QColor kTerrainColor(126, 138, 104);

const QColor kSunPathColor(0, 0, 0); //(255, 210, 80);
const QColor kSunMarkerColor(255, 240, 170);
const QColor kSunMarkerOutlineColor(40, 35, 22); //(0, 0, 0);
const QColor kCenterCrossColor(255, 0, 0);
const QColor kCenterCubeColor(255, 0, 0);

double clamp01(double value)
{
    return std::min(1.0, std::max(0.0, value));
}

} // namespace

SceneViewCPU::SceneViewCPU(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(480, 360);
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);
}

void SceneViewCPU::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void SceneViewCPU::onTerrainChanged()
{
    m_shadingDirty = true;
    update();
}

void SceneViewCPU::onDateTimeChanged()
{
    m_shadingDirty = true;
    update();
}

void SceneViewCPU::onShadowsEnabledChanged()
{
    m_shadingDirty = true;
    update();
}

void SceneViewCPU::onCameraChanged()
{
    updateViewFrame();
    update();
}

void SceneViewCPU::updateViewFrame()
{
    if (!m_camera) {
        return;
    }
}

void SceneViewCPU::drawSunOverlay(QPainter& painter)
{
    if (!m_camera) {
        return;
    }

    QVector<QPointF> projectedPath;
    //projectedPath.reserve(m_sunPathPoints.size());

    for (const geo::Vector3& point : m_sunPathPoints) {
        QPointF screen;
        double depth = 0.0;
        if (projectPoint(point, screen, depth)) {
            projectedPath.push_back(screen);
        }
    }

    if (projectedPath.size() >= 2) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pathPen(kSunPathColor);
        pathPen.setWidth(2);
        painter.setPen(pathPen);
        painter.setBrush(Qt::NoBrush);

        QPainterPath path(projectedPath[0]);
        for (int i = 1; i < projectedPath.size(); ++i) {
            const QPointF& p0 = projectedPath[i - 1];
            const QPointF& p1 = projectedPath[i];
            path.quadTo((p0 + p1) * 0.5, p1);
        }
        painter.drawPath(path);
    }

    if (m_hasCurrentSunPoint) {
        QPointF sunScreen;
        double sunDepth = 0.0;
        if (projectPoint(m_currentSunPoint, sunScreen, sunDepth)) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(kSunMarkerOutlineColor, 1));
            painter.setBrush(kSunMarkerColor);
            painter.drawEllipse(sunScreen, 5.0, 5.0);
        }
    }

    if (m_hasCenterCross && m_centerCrossPoints.size() >= 4) {
        QPointF a, b, c, d;
        double depth = 0.0;
        geo::Vector3 p0 = m_centerCrossPoints[0];
        geo::Vector3 p1 = m_centerCrossPoints[1];
        geo::Vector3 p2 = m_centerCrossPoints[2];
        geo::Vector3 p3 = m_centerCrossPoints[3];
        p0.z *= m_terrainHeightExaggeration;
        p1.z *= m_terrainHeightExaggeration;
        p2.z *= m_terrainHeightExaggeration;
        p3.z *= m_terrainHeightExaggeration;

        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen crossPen(kCenterCrossColor);
        crossPen.setWidth(2);
        painter.setPen(crossPen);
        painter.setBrush(Qt::NoBrush);

        if (projectPoint(p0, a, depth) && projectPoint(p1, b, depth)) {
            painter.drawLine(a, b);
        }
        if (projectPoint(p2, c, depth) && projectPoint(p3, d, depth)) {
            painter.drawLine(c, d);
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const CompassLabel& label : m_compassLabels) {
        const std::array<float, 3> rgb = compassLabelColor(label.letter);
        painter.setPen(QPen(QColor::fromRgbF(rgb[0], rgb[1], rgb[2]), 2));
        for (std::size_t i = 0; i + 1 < label.lineList.size(); i += 2) {
            QPointF a, b;
            double depth = 0.0;
            if (projectPoint(label.lineList[i], a, depth) &&
                projectPoint(label.lineList[i + 1], b, depth)) {
                painter.drawLine(a, b);
            }
        }
    }
}

bool SceneViewCPU::projectPoint(const geo::Vector3& local, QPointF& screen, double& depth) const
{
    const geo::Vector3 rel = local - m_eye;

    depth = rel.dot(m_forward);
    if (depth <= 0.1) {
        return false; // behind the camera
    }

    const double halfHeight = std::tan(geo::degToRad(kFieldOfViewDeg * 0.5));
    const double focal = (height() * 0.5) / halfHeight;

    const double x = rel.dot(m_right) / depth;
    const double y = rel.dot(m_up) / depth;

    screen.setX(width() * 0.5 + x * focal);
    screen.setY(height() * 0.5 - y * focal);
    return true;
}

void SceneViewCPU::rebuildShading()
{
    m_faceShadeColors.clear();
    m_shadingDirty = false;

    if (!m_terrain) {
        return;
    }

    const geo::TriangleMesh& mesh = m_terrain->sceneMesh();
    const std::vector<geo::Vector3>& vertices = mesh.vertices();

    const bool sunUp = m_light && m_light->isAboveHorizon();
    const geo::Vector3 toSun = m_light ? m_light->directionToSun() : geo::Vector3{0.0, 0.0, 1.0};
    const double sunIntensity = m_light ? m_light->intensity() : 0.0;

    m_faceShadeColors.reserve(mesh.triangles().size());

    for (const geo::TriangleMesh::Triangle& tri : mesh.triangles()) {
        geo::Vector3 a = vertices[tri.a];
        geo::Vector3 b = vertices[tri.b];
        geo::Vector3 c = vertices[tri.c];
        const geo::Vector3 shadowCentre = (a + b + c) / 3.0;
        a.z *= m_terrainHeightExaggeration;
        b.z *= m_terrainHeightExaggeration;
        c.z *= m_terrainHeightExaggeration;

        geo::Vector3 normal = (b - a).cross(c - a).normalized();
        if (normal.z < 0.0) {
            normal = normal * -1.0;
        }

        // Weak base illumination: constant ambient plus a soft light from the
        // sky (straight above), independent of the sun.
        double shade = kAmbient + kSkyFill * clamp01(normal.z);

        if (sunUp) {
            const double lambert = clamp01(normal.dot(toSun));
            if (lambert > 0.0) {
                // The terrain shadow raycast (isInShadow) is by far the most
                // expensive part of shading a frame. Only pay for it when the
                // user explicitly asked for the full lighting simulation;
                // otherwise show a cheap sun-facing preview.
                bool inShadow = false;
                if (m_shadowsEnabled) {
                    inShadow = m_terrain->isInShadow(shadowCentre, toSun);
                }
                if (!inShadow) {
                    shade += kSunDiffuse * lambert * clamp01(sunIntensity + 0.25);
                }
            }
        }

        shade = clamp01(shade);
        m_faceShadeColors.push_back(QColor::fromRgbF(kTerrainColor.redF() * shade,
                                                      kTerrainColor.greenF() * shade,
                                                      kTerrainColor.blueF() * shade));
    }
}

void SceneViewCPU::collectFaces(QVector<Face>& faces)
{
    if (m_shadingDirty) {
        rebuildShading();
    }

    const geo::TriangleMesh& mesh = m_terrain->sceneMesh();
    const std::vector<geo::Vector3>& vertices = mesh.vertices();

    faces.reserve(static_cast<int>(mesh.triangles().size()));

    std::size_t idx = 0;
    for (const geo::TriangleMesh::Triangle& tri : mesh.triangles()) {
        geo::Vector3 a = vertices[tri.a];
        geo::Vector3 b = vertices[tri.b];
        geo::Vector3 c = vertices[tri.c];
        a.z *= m_terrainHeightExaggeration;
        b.z *= m_terrainHeightExaggeration;
        c.z *= m_terrainHeightExaggeration;

        Face face;
        double depthA = 0.0;
        double depthB = 0.0;
        double depthC = 0.0;
        if (!projectPoint(a, face.screen[0], depthA) ||
            !projectPoint(b, face.screen[1], depthB) ||
            !projectPoint(c, face.screen[2], depthC)) {
            ++idx;
            continue;
        }
        face.depth = (depthA + depthB + depthC) / 3.0;
        face.color = m_faceShadeColors[idx];
        face.visible = true;
        faces.push_back(face);
        ++idx;
    }

    // Painter's algorithm: draw the far triangles first.
    std::sort(faces.begin(), faces.end(),
              [](const Face& lhs, const Face& rhs) { return lhs.depth > rhs.depth; });
}

void SceneViewCPU::collectCenterCubeFaces(QVector<Face>& faces)
{
    if (!m_centerCubeVisible || !m_hasCenterCross || m_centerCubeCorners.size() < 8) {
        return;
    }

    std::vector<geo::Vector3> corners;
    corners.reserve(8);
    // The cube's own 10m edge length is never stretched by the height
    // exaggeration; only its base position is shifted vertically by the
    // same amount the exaggerated terrain surface moves, so it keeps
    // sitting on the (exaggerated) ground instead of sinking below it.
    const double baseZ = m_centerCubeCorners[0].z;
    const double shiftZ = baseZ * m_terrainHeightExaggeration - baseZ;
    for (const geo::Vector3& corner : m_centerCubeCorners) {
        geo::Vector3 p = corner;
        p.z += shiftZ;
        corners.push_back(p);
    }

    constexpr int kFaceIndices[6][4] = {
        {0, 1, 2, 3}, // bottom
        {4, 7, 6, 5}, // top
        {0, 4, 5, 1}, // south
        {1, 5, 6, 2}, // east
        {2, 6, 7, 3}, // north
        {3, 7, 4, 0}, // west
    };

    for (const auto& faceIdx : kFaceIndices) {
        const geo::Vector3& a = corners[faceIdx[0]];
        const geo::Vector3& b = corners[faceIdx[1]];
        const geo::Vector3& c = corners[faceIdx[2]];
        const geo::Vector3& d = corners[faceIdx[3]];

        for (const auto& tri : {std::array<const geo::Vector3*, 3>{&a, &b, &c},
                                std::array<const geo::Vector3*, 3>{&a, &c, &d}}) {
            Face face;
            double depthA = 0.0, depthB = 0.0, depthC = 0.0;
            if (!projectPoint(*tri[0], face.screen[0], depthA) ||
                !projectPoint(*tri[1], face.screen[1], depthB) ||
                !projectPoint(*tri[2], face.screen[2], depthC)) {
                continue;
            }
            face.depth = (depthA + depthB + depthC) / 3.0;
            face.color = kCenterCubeColor;
            face.visible = true;
            faces.push_back(face);
        }
    }
}

void SceneViewCPU::paintEvent(QPaintEvent* /*event*/)
{
    if (!m_camera) {
        return;
    }

    m_eye = m_camera->localPosition();
    m_eye.z *= m_terrainHeightExaggeration;
    m_forward = m_camera->viewDirection();

    // The scene target is the terrain surface below the dome centre.
    m_target = geo::Vector3{ 0.0, 0.0, 0.0 };
    if (m_terrain) {
        double heightM = 0.0;
        if (m_terrain->heightAt(0.0, 0.0, heightM)) {
            m_target.z = heightM * m_terrainHeightExaggeration;
        }
    }
    m_forward = (m_target - m_eye).normalized();

    const geo::Vector3 worldUp{ 0.0, 0.0, 1.0 };
    m_right = m_forward.cross(worldUp).normalized();
    if (m_right.length() < 1e-9) {
        m_right = geo::Vector3{ 1.0, 0.0, 0.0 };
    }
    m_up = m_right.cross(m_forward).normalized();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Sky gradient; it darkens once the sun is below the horizon.
    const bool sunUp = m_light && m_light->isAboveHorizon();
    QLinearGradient sky(0, 0, 0, height());
    if (sunUp) {
        sky.setColorAt(0.0, QColor(96, 148, 214));
        sky.setColorAt(1.0, QColor(196, 216, 236));
    } else {
        sky.setColorAt(0.0, QColor(16, 22, 40));
        sky.setColorAt(1.0, QColor(52, 60, 84));
    }
    painter.fillRect(rect(), sky);

    if (!m_terrain || !m_camera) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("No terrain loaded"));
        return;
    }

    QVector<Face> faces;
    collectFaces(faces);
    collectCenterCubeFaces(faces);
    std::sort(faces.begin(), faces.end(),
              [](const Face& lhs, const Face& rhs) { return lhs.depth > rhs.depth; });

    painter.setPen(Qt::NoPen);
    for (const Face& face : faces) {
        QPolygonF polygon;
        polygon << face.screen[0] << face.screen[1] << face.screen[2];
        painter.setBrush(face.color);
        painter.drawPolygon(polygon);
    }

    drawSunOverlay(painter);
}

void SceneViewCPU::mousePressEvent(QMouseEvent* event)
{
    handleOrbitMousePress(event->pos());
}

void SceneViewCPU::mouseMoveEvent(QMouseEvent* event)
{
    handleOrbitMouseMove(event);
}

void SceneViewCPU::wheelEvent(QWheelEvent* event)
{
    handleOrbitWheel(event);
}
