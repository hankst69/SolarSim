#pragma once

#include "geolib/ProgressClass.h"
#include "geolib/CameraPosition.h"
#include "geolib/DateTimeUtc.h"
#include "geolib/SunPath.h"
#include "geolib/SunLight.h"
#include "geolib/TerrainModel.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>

#include <memory>
#include <vector>

/// Hardware accelerated view of the terrain scene, built on Qt's OpenGL
/// integration (QOpenGLWidget/QOpenGLFunctions). This uses Qt's own 3D GPU
/// abstraction: QOpenGLWidget creates and manages the native OpenGL context
/// for whichever platform backend Qt is configured with (WGL on Windows,
/// GLX/EGL on Linux, etc.), so no platform specific windowing code is needed
/// here, unlike the WebGPU backend (GpuSceneRenderer).
///
/// Like GpuSceneRenderer, the CPU still computes per-triangle shading (the
/// sun lighting model of SunLight); the GPU only does the projection,
/// rasterization and depth testing, which is far faster than the
/// painter's-algorithm software rasterizer used by SceneView for larger
/// meshes. The terrain cast shadow raycast (TerrainModel::isInShadow()) is by
/// far the most expensive part of shading a frame, so it is only evaluated
/// when setShadowsEnabled(true) is set; by default a cheap sun-facing preview
/// shading is used instead, keeping date/time changes interactive.
class GLSceneView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit GLSceneView(QWidget* parent = nullptr);
    ~GLSceneView() override;

    void setTerrain(std::shared_ptr<const geo::TerrainModel> terrain);
    void setDateTime(const geo::DateTimeUtc& utc);
    void setCamera(const geo::CameraPosition& camera);
    void setShadowsEnabled(bool enabled);

    const geo::CameraPosition* camera() const { return m_camera.get(); }
    bool shadowsEnabled() const { return m_shadowsEnabled; }

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
    void rebuildSunOverlay();
    void uploadGeometryIfNeeded();
    void uploadSunOverlayIfNeeded();

    std::shared_ptr<const geo::TerrainModel> m_terrain;
    std::unique_ptr<geo::CameraPosition> m_camera;
    std::unique_ptr<geo::SunLight> m_light;
    geo::DateTimeUtc m_utc;

    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vertexBuffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_overlayVertexBuffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_overlayVao;
    int m_vertexCount{0};
    int m_overlayVertexCount{0};
    bool m_geometryDirty{true};
    bool m_overlayDirty{true};
    bool m_glInitialized{false};
    bool m_shadowsEnabled{false};

    std::vector<geo::Vector3> m_sunPathPoints;
    geo::Vector3 m_currentSunPoint;
    bool m_hasCurrentSunPoint{false};

    QPoint m_lastMousePos;


public:
    void setProgressClass(geo::ProgressClassPtr progressClass)
    {
        m_progressClass = progressClass;
    }
protected:
    void progress(int percent, std::string msg) const
    {
        if (m_progressClass) {
            int progressVal = percent % 100;
            m_progressClass->progress(progressVal, std::move(msg));
        }
    }
private:
    geo::ProgressClassPtr m_progressClass;
    int m_progressCount{ 0 };
};
