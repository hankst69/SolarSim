#include "GLSceneView.h"
#include "GLSceneView.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kFieldOfViewDeg = 55.0;
constexpr double kAmbient = 0.18;
constexpr double kSkyFill = 0.12;
constexpr double kSunDiffuse = 0.85;
constexpr float kTerrainColor[3] = {126.0f / 255.0f, 138.0f / 255.0f, 104.0f / 255.0f};
constexpr float kSunOverlayColor[3] = {0.0f, 0.0f, 0.0f};
constexpr int kSunPathSampleCount = 144;

double clamp01(double value)
{
    return std::min(1.0, std::max(0.0, value));
}

const char* kVertexShaderSource = R"GLSL(
#version 330 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

uniform mat4 uViewProj;

out vec3 vColor;

void main()
{
    gl_Position = uViewProj * vec4(inPosition, 1.0);
    vColor = inColor;
}
)GLSL";

const char* kFragmentShaderSource = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 fragColor;

void main()
{
    fragColor = vec4(vColor, 1.0);
}
)GLSL";

} // namespace

GLSceneView::GLSceneView(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);
}

GLSceneView::~GLSceneView()
{
    // Destroy GPU resources while the context is still current.
    makeCurrent();
    m_vertexBuffer.destroy();
    m_vao.destroy();
    m_overlayVertexBuffer.destroy();
    m_overlayVao.destroy();
    doneCurrent();
}

void GLSceneView::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.06f, 0.09f, 0.16f, 1.0f);

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShaderSource);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShaderSource);
    m_program.link();

    m_vao.create();
    m_vao.bind();

    m_vertexBuffer.create();
    m_vertexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_vao.release();

    m_overlayVao.create();
    m_overlayVao.bind();

    m_overlayVertexBuffer.create();
    m_overlayVertexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_overlayVao.release();

    m_glInitialized = true;
    uploadGeometryIfNeeded();
    uploadSunOverlayIfNeeded();
}

void GLSceneView::resizeGL(int /*w*/, int /*h*/)
{
    // QOpenGLWidget already applies glViewport for us before paintGL().
}

void GLSceneView::rebuildLight()
{
    if (!m_terrain) {
        m_light.reset();
        return;
    }
    m_light = std::make_unique<geo::SunLight>(*m_terrain, m_utc);
}

void GLSceneView::rebuildGeometry()
{
    m_geometryDirty = true;
    if (m_glInitialized) {
        update();
    }
}

void GLSceneView::rebuildSunOverlay()
{
    m_sunPathPoints.clear();
    m_hasCurrentSunPoint = false;
    m_overlayDirty = true;

    if (!m_terrain) {
        if (m_glInitialized) {
            update();
        }
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

    if (m_glInitialized) {
        update();
    }
}

void GLSceneView::setTerrain(std::shared_ptr<const geo::TerrainModel> terrain)
{
    m_terrain = std::move(terrain);
    rebuildLight();
    rebuildGeometry();
    rebuildSunOverlay();
}

void GLSceneView::setDateTime(const geo::DateTimeUtc& utc)
{
    m_utc = utc;
    rebuildLight();
    rebuildGeometry();
    rebuildSunOverlay();
}

void GLSceneView::setCamera(const geo::CameraPosition& camera)
{
    m_camera = std::make_unique<geo::CameraPosition>(camera);
    update();
}

void GLSceneView::uploadGeometryIfNeeded()
{
    if (!m_geometryDirty || !m_glInitialized || !m_terrain) {
        return;
    }
    m_geometryDirty = false;

    const geo::TriangleMesh& mesh = m_terrain->sceneMesh();
    const std::vector<geo::Vector3>& vertices = mesh.vertices();

    const bool sunUp = m_light && m_light->isAboveHorizon();
    const geo::Vector3 toSun = m_light ? m_light->directionToSun() : geo::Vector3{0.0, 0.0, 1.0};
    const double sunIntensity = m_light ? m_light->intensity() : 0.0;

    std::vector<Vertex> gpuVertices;
    gpuVertices.reserve(mesh.triangles().size() * 3);

    for (const geo::TriangleMesh::Triangle& tri : mesh.triangles()) {
        const geo::Vector3& a = vertices[tri.a];
        const geo::Vector3& b = vertices[tri.b];
        const geo::Vector3& c = vertices[tri.c];

        geo::Vector3 normal = (b - a).cross(c - a).normalized();
        if (normal.z < 0.0) {
            normal = normal * -1.0;
        }

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

        const float color[3] = {static_cast<float>(kTerrainColor[0] * shade),
                                static_cast<float>(kTerrainColor[1] * shade),
                                static_cast<float>(kTerrainColor[2] * shade)};

        for (const geo::Vector3* v : {&a, &b, &c}) {
            Vertex vertex{};
            vertex.position[0] = static_cast<float>(v->x);
            vertex.position[1] = static_cast<float>(v->y);
            vertex.position[2] = static_cast<float>(v->z);
            vertex.color[0] = color[0];
            vertex.color[1] = color[1];
            vertex.color[2] = color[2];
            gpuVertices.push_back(vertex);
        }
    }

    m_vertexCount = static_cast<int>(gpuVertices.size());

    m_vao.bind();
    m_vertexBuffer.bind();
    m_vertexBuffer.allocate(gpuVertices.data(),
                            static_cast<int>(gpuVertices.size() * sizeof(Vertex)));

    m_program.bind();
    m_program.enableAttributeArray(0);
    m_program.setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));
    m_program.enableAttributeArray(1);
    m_program.setAttributeBuffer(1, GL_FLOAT, offsetof(Vertex, color), 3, sizeof(Vertex));
    m_program.release();

    m_vertexBuffer.release();
    m_vao.release();
}

void GLSceneView::uploadSunOverlayIfNeeded()
{
    if (!m_overlayDirty || !m_glInitialized) {
        return;
    }
    m_overlayDirty = false;

    std::vector<Vertex> overlayVertices;
    overlayVertices.reserve(m_sunPathPoints.size() + (m_hasCurrentSunPoint ? 4 : 0));

    for (const geo::Vector3& point : m_sunPathPoints) {
        Vertex vertex{};
        vertex.position[0] = static_cast<float>(point.x);
        vertex.position[1] = static_cast<float>(point.y);
        vertex.position[2] = static_cast<float>(point.z);
        vertex.color[0] = kSunOverlayColor[0];
        vertex.color[1] = kSunOverlayColor[1];
        vertex.color[2] = kSunOverlayColor[2];
        overlayVertices.push_back(vertex);
    }

    if (m_hasCurrentSunPoint && m_terrain) {
        const geo::Vector3 normal = m_currentSunPoint.normalized();
        geo::Vector3 tangentU = normal.cross(geo::Vector3{0.0, 0.0, 1.0});
        if (tangentU.length() < 1e-9) {
            tangentU = normal.cross(geo::Vector3{1.0, 0.0, 0.0});
        }
        tangentU = tangentU.normalized();
        const geo::Vector3 tangentV = normal.cross(tangentU).normalized();

        const double markerSizeM = std::clamp(m_terrain->dome().radius() * 0.005, 2.0, 30.0);
        const geo::Vector3 p0 = m_currentSunPoint - tangentU * markerSizeM;
        const geo::Vector3 p1 = m_currentSunPoint + tangentU * markerSizeM;
        const geo::Vector3 p2 = m_currentSunPoint - tangentV * markerSizeM;
        const geo::Vector3 p3 = m_currentSunPoint + tangentV * markerSizeM;

        for (const geo::Vector3& p : {p0, p1, p2, p3}) {
            Vertex vertex{};
            vertex.position[0] = static_cast<float>(p.x);
            vertex.position[1] = static_cast<float>(p.y);
            vertex.position[2] = static_cast<float>(p.z);
            vertex.color[0] = kSunOverlayColor[0];
            vertex.color[1] = kSunOverlayColor[1];
            vertex.color[2] = kSunOverlayColor[2];
            overlayVertices.push_back(vertex);
        }
    }

    m_overlayVertexCount = static_cast<int>(overlayVertices.size());

    m_overlayVao.bind();
    m_overlayVertexBuffer.bind();
    m_overlayVertexBuffer.allocate(overlayVertices.data(),
                                   static_cast<int>(overlayVertices.size() * sizeof(Vertex)));

    m_program.bind();
    m_program.enableAttributeArray(0);
    m_program.setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));
    m_program.enableAttributeArray(1);
    m_program.setAttributeBuffer(1, GL_FLOAT, offsetof(Vertex, color), 3, sizeof(Vertex));
    m_program.release();

    m_overlayVertexBuffer.release();
    m_overlayVao.release();
}

void GLSceneView::paintGL()
{
    const bool sunUp = m_light && m_light->isAboveHorizon();
    if (sunUp) {
        glClearColor(0.38f, 0.58f, 0.84f, 1.0f);
    } else {
        glClearColor(0.06f, 0.09f, 0.16f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadGeometryIfNeeded();
    uploadSunOverlayIfNeeded();

    if (!m_terrain || !m_camera || m_vertexCount == 0) {
        return;
    }

    const geo::Vector3 eye = m_camera->localPosition();
    geo::Vector3 target{0.0, 0.0, 0.0};
    double heightM = 0.0;
    if (m_terrain->heightAt(0.0, 0.0, heightM)) {
        target.z = heightM;
    }

    QMatrix4x4 view;
    view.lookAt(QVector3D(static_cast<float>(eye.x), static_cast<float>(eye.y),
                          static_cast<float>(eye.z)),
                QVector3D(static_cast<float>(target.x), static_cast<float>(target.y),
                          static_cast<float>(target.z)),
                QVector3D(0.0f, 0.0f, 1.0f));

    const qreal aspect = height() > 0 ? static_cast<qreal>(width()) / height() : 1.0;
    QMatrix4x4 projection;
    projection.perspective(static_cast<float>(kFieldOfViewDeg), static_cast<float>(aspect), 0.5f,
                           100000.0f);

    const QMatrix4x4 viewProj = projection * view;

    m_program.bind();
    m_program.setUniformValue("uViewProj", viewProj);

    m_vao.bind();
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    m_vao.release();

    const int pathCount = static_cast<int>(m_sunPathPoints.size());
    if (pathCount >= 2 && m_overlayVertexCount >= pathCount) {
        m_overlayVao.bind();
        glDrawArrays(GL_LINE_STRIP, 0, pathCount);

        const int markerOffset = pathCount;
        const int markerCount = m_overlayVertexCount - pathCount;
        if (markerCount >= 4) {
            glDrawArrays(GL_LINES, markerOffset, markerCount);
        }
        m_overlayVao.release();
    }

    m_program.release();
}

void GLSceneView::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();
}

void GLSceneView::mouseMoveEvent(QMouseEvent* event)
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
    emit cameraChanged();
}

void GLSceneView::wheelEvent(QWheelEvent* event)
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
