#pragma once

#include "geolib/CameraPosition.h"
#include "geolib/DateTimeUtc.h"
#include "geolib/SunLight.h"
#include "geolib/TerrainModel.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>

#include <memory>

/// Hardware accelerated view of the terrain scene, built on Qt's OpenGL
/// integration (QOpenGLWidget/QOpenGLFunctions). This uses Qt's own 3D GPU
/// abstraction: QOpenGLWidget creates and manages the native OpenGL context
/// for whichever platform backend Qt is configured with (WGL on Windows,
/// GLX/EGL on Linux, etc.), so no platform specific windowing code is needed
/// here, unlike the WebGPU backend (GpuSceneRenderer).
///
/// Like GpuSceneRenderer, the CPU still computes per-triangle shading (the
/// sun/shadow lighting model of SunLight/TerrainModel::isInShadow()); the GPU
/// only does the projection, rasterization and depth testing, which is far
/// faster than the painter's-algorithm software rasterizer used by SceneView
/// for larger meshes.
class GLSceneView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit GLSceneView(QWidget* parent = nullptr);
    ~GLSceneView() override;

    void setTerrain(std::shared_ptr<const geo::TerrainModel> terrain);
    void setDateTime(const geo::DateTimeUtc& utc);
    void setCamera(const geo::CameraPosition& camera);

    const geo::CameraPosition* camera() const { return m_camera.get(); }

signals:
    void cameraChanged();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct Vertex {
        float position[3];
        float color[3];
    };

    void rebuildLight();
    void rebuildGeometry();
    void uploadGeometryIfNeeded();

    std::shared_ptr<const geo::TerrainModel> m_terrain;
    std::unique_ptr<geo::CameraPosition> m_camera;
    std::unique_ptr<geo::SunLight> m_light;
    geo::DateTimeUtc m_utc;

    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vertexBuffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    int m_vertexCount{0};
    bool m_geometryDirty{true};
    bool m_glInitialized{false};

    QPoint m_lastMousePos;
};
