#pragma once

#include "SceneViewBase.h"

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
/// here, unlike the WebGPU backend (SceneViewWEBGPU).
///
/// Like SceneViewWEBGPU, the CPU still computes per-triangle shading (the
/// sun lighting model of SunLight); the GPU only does the projection,
/// rasterization and depth testing, which is far faster than the
/// painter's-algorithm software rasterizer used by SceneViewCPU for larger
/// meshes. The terrain cast shadow raycast (TerrainModel::isInShadow()) is by
/// far the most expensive part of shading a frame, so it is only evaluated
/// when setShadowsEnabled(true) is set; by default a cheap sun-facing preview
/// shading is used instead, keeping date/time changes interactive.
class SceneViewGL : public QOpenGLWidget, protected QOpenGLFunctions, public SceneViewBase {
    Q_OBJECT

public:
    explicit SceneViewGL(QWidget* parent = nullptr);
    ~SceneViewGL() override;

signals:
    void cameraChanged();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void onTerrainChanged() override;
    void onDateTimeChanged() override;
    void onCameraChanged() override;
    void onShadowsEnabledChanged() override;
    void emitCameraChanged() override { emit cameraChanged(); }

private:
    struct Vertex {
        float position[3];
        float color[3];
    };

    void rebuildGeometry();
    void rebuildSunOverlay();
    void uploadGeometryIfNeeded();
    void uploadSunOverlayIfNeeded();

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
};
