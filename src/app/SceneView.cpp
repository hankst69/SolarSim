#include "SceneView.h"

#include "geolib/Angle.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QWheelEvent>
#include <QtGlobal>

#include <algorithm>
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

double clamp01(double value)
{
    return std::min(1.0, std::max(0.0, value));
}

} // namespace

SceneView::SceneView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(480, 360);
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);
}

void SceneView::setTerrain(std::shared_ptr<const geo::TerrainModel> terrain)
{
    m_terrain = std::move(terrain);
    rebuildLight();
    update();
}

void SceneView::setDateTime(const geo::DateTimeUtc& utc)
{
    m_utc = utc;
    rebuildLight();
    update();
}

void SceneView::setCamera(const geo::CameraPosition& camera)
{
    m_camera = std::make_unique<geo::CameraPosition>(camera);
    updateViewFrame();
    update();
}

void SceneView::rebuildLight()
{
    if (!m_terrain) {
        m_light.reset();
        return;
    }
    m_light = std::make_unique<geo::SunLight>(*m_terrain, m_utc);
}

void SceneView::updateViewFrame()
{
    if (!m_camera) {
        return;
    }

    m_eye = m_camera->localPosition();
    m_forward = m_camera->viewDirection();

    // The scene target is the terrain surface below the dome centre.
    m_target = geo::Vector3{0.0, 0.0, 0.0};
    if (m_terrain) {
        double heightM = 0.0;
        if (m_terrain->heightAt(0.0, 0.0, heightM)) {
            m_target.z = heightM;
        }
    }
    m_forward = (m_target - m_eye).normalized();

    const geo::Vector3 worldUp{0.0, 0.0, 1.0};
    m_right = m_forward.cross(worldUp).normalized();
    if (m_right.length() < 1e-9) {
        m_right = geo::Vector3{1.0, 0.0, 0.0};
    }
    m_up = m_right.cross(m_forward).normalized();
}

bool SceneView::projectPoint(const geo::Vector3& local, QPointF& screen, double& depth) const
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

void SceneView::collectFaces(QVector<Face>& faces) const
{
    const geo::TriangleMesh& mesh = m_terrain->sceneMesh();
    const std::vector<geo::Vector3>& vertices = mesh.vertices();

    const bool sunUp = m_light && m_light->isAboveHorizon();
    const geo::Vector3 toSun = m_light ? m_light->directionToSun() : geo::Vector3{0.0, 0.0, 1.0};
    const double sunIntensity = m_light ? m_light->intensity() : 0.0;

    faces.reserve(static_cast<int>(mesh.triangles().size()));

    for (const geo::TriangleMesh::Triangle& tri : mesh.triangles()) {
        const geo::Vector3& a = vertices[tri.a];
        const geo::Vector3& b = vertices[tri.b];
        const geo::Vector3& c = vertices[tri.c];

        Face face;
        double depthA = 0.0;
        double depthB = 0.0;
        double depthC = 0.0;
        if (!projectPoint(a, face.screen[0], depthA) ||
            !projectPoint(b, face.screen[1], depthB) ||
            !projectPoint(c, face.screen[2], depthC)) {
            continue;
        }
        face.depth = (depthA + depthB + depthC) / 3.0;

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
                const geo::Vector3 centre = (a + b + c) / 3.0;
                if (!m_terrain->isInShadow(centre, toSun)) {
                    shade += kSunDiffuse * lambert * clamp01(sunIntensity + 0.25);
                }
            }
        }

        shade = clamp01(shade);
        face.color = QColor::fromRgbF(kTerrainColor.redF() * shade,
                                      kTerrainColor.greenF() * shade,
                                      kTerrainColor.blueF() * shade);
        face.visible = true;
        faces.push_back(face);
    }

    // Painter's algorithm: draw the far triangles first.
    std::sort(faces.begin(), faces.end(),
              [](const Face& lhs, const Face& rhs) { return lhs.depth > rhs.depth; });
}

void SceneView::paintEvent(QPaintEvent* /*event*/)
{
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

    painter.setPen(Qt::NoPen);
    for (const Face& face : faces) {
        QPolygonF polygon;
        polygon << face.screen[0] << face.screen[1] << face.screen[2];
        painter.setBrush(face.color);
        painter.drawPolygon(polygon);
    }
}

void SceneView::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();
}

void SceneView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_camera || !(event->buttons() & Qt::LeftButton)) {
        return;
    }

    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();
    if (delta.isNull()) {
        return;
    }

    // Dragging orbits the camera around the standpoint.
    setCamera(m_camera->orbited(-delta.x() * 0.4, delta.y() * 0.3));
    emit cameraChanged();
}

void SceneView::wheelEvent(QWheelEvent* event)
{
    if (!m_camera) {
        return;
    }

    const int steps = event->angleDelta().y();
    if (steps == 0) {
        return;
    }

    setCamera(m_camera->zoomed(steps > 0 ? 0.9 : 1.0 / 0.9));
    emit cameraChanged();
    event->accept();
}
