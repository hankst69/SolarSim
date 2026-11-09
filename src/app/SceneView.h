#pragma once

#include "geolib/ProgressClass.h"
#include "geolib/CameraPosition.h"
#include "geolib/DateTimeUtc.h"
#include "geolib/SunPath.h"
#include "geolib/SunLight.h"
#include "geolib/TerrainModel.h"

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QVector>
#include <QWidget>

#include <memory>
#include <vector>

#if defined(SOLARSIM_USE_WEBGPU)
#include "GpuSceneRenderer.h"
#endif

class QPainter;

/// View of the terrain scene.
///
/// By default the widget projects the triangle mesh of a geo::TerrainModel
/// with a simple software painter's algorithm. When built with
/// SOLARSIM_USE_WEBGPU, rendering is instead hardware accelerated through the
/// WebGPU API (GpuSceneRenderer), which works identically on desktop (native
/// WebGPU via wgpu-native/Dawn) and in a WebAssembly build running in a
/// browser. Shading combines a weak ambient/diffuse base light with the
/// directional geo::SunLight of the current date/time. Terrain cast shadows
/// (geo::TerrainModel::isInShadow()) are expensive to evaluate and are only
/// computed when setShadowsEnabled(true) is set; by default a cheap
/// sun-facing preview shading is used so camera/date/time interaction stays
/// smooth.
class SceneView : public QWidget {
    Q_OBJECT

public:
    explicit SceneView(QWidget* parent = nullptr);

    void setTerrain(std::shared_ptr<const geo::TerrainModel> terrain);
    void setDateTime(const geo::DateTimeUtc& utc);
    void setCamera(const geo::CameraPosition& camera);
    void setShadowsEnabled(bool enabled);

    const geo::CameraPosition* camera() const { return m_camera.get(); }
    bool shadowsEnabled() const { return m_shadowsEnabled; }

signals:
    void cameraChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct Face {
        QPointF screen[3];
        double depth{0.0};
        QColor color;
        bool visible{false};
    };

    void rebuildLight();
    void rebuildSunPathOverlay();
    void rebuildShading();
    void collectFaces(QVector<Face>& faces);
    void drawSunOverlay(QPainter& painter);
    bool projectPoint(const geo::Vector3& local, QPointF& screen, double& depth) const;
    void updateViewFrame();

    std::shared_ptr<const geo::TerrainModel> m_terrain;
    std::unique_ptr<geo::CameraPosition> m_camera;
    std::unique_ptr<geo::SunLight> m_light;
    geo::DateTimeUtc m_utc;

    geo::Vector3 m_eye;
    geo::Vector3 m_right;
    geo::Vector3 m_up;
    geo::Vector3 m_forward;
    geo::Vector3 m_target;
    std::vector<geo::Vector3> m_sunPathPoints;
    geo::Vector3 m_currentSunPoint;
    bool m_hasCurrentSunPoint{false};

    // Per-triangle shading cache: colour of each triangle in mesh.triangles()
    // order. Recomputed only when terrain/time/shadow mode changes, not on
    // every paint.
    std::vector<QColor> m_faceShadeColors;
    bool m_shadingDirty{true};
    bool m_shadowsEnabled{false};

    QPoint m_lastMousePos;

#if defined(SOLARSIM_USE_WEBGPU)
    std::unique_ptr<GpuSceneRenderer> m_gpuRenderer;
    bool m_gpuInitialized{false};
#endif


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
