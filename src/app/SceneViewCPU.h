#pragma once

#include "SceneViewBase.h"

#include "geolib/TerrainModel.h"

#include <QColor>
#include <QPointF>
#include <QVector>
#include <QWidget>

#include <memory>
#include <vector>

class QPainter;

/// View of the terrain scene that projects the triangle mesh of a
/// geo::TerrainModel with a simple software painter's algorithm. This is the
/// baseline rendering strategy that always works, regardless of available
/// hardware acceleration; see SceneViewGL and SceneViewWEBGPU for the
/// OpenGL/WebGPU hardware accelerated alternatives selected by MainWindow.
/// Shading combines a weak ambient/diffuse base light with the directional
/// geo::SunLight of the current date/time. Terrain cast shadows
/// (geo::TerrainModel::isInShadow()) are expensive to evaluate and are only
/// computed when setShadowsEnabled(true) is set; by default a cheap
/// sun-facing preview shading is used so camera/date/time interaction stays
/// smooth.
class SceneViewCPU : public QWidget, public SceneViewBase {
    Q_OBJECT

public:
    explicit SceneViewCPU(QWidget* parent = nullptr);

signals:
    void cameraChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void onTerrainChanged() override;
    void onDateTimeChanged() override;
    void onCameraChanged() override;
    void onShadowsEnabledChanged() override;
    void emitCameraChanged() override { emit cameraChanged(); }

private:
    struct Face {
        QPointF screen[3];
        double depth{0.0};
        QColor color;
        bool visible{false};
    };

    void rebuildShading();
    void collectFaces(QVector<Face>& faces);
    void drawSunOverlay(QPainter& painter);
    bool projectPoint(const geo::Vector3& local, QPointF& screen, double& depth) const;
    void updateViewFrame();

    geo::Vector3 m_eye;
    geo::Vector3 m_right;
    geo::Vector3 m_up;
    geo::Vector3 m_forward;
    geo::Vector3 m_target;

    // Per-triangle shading cache: colour of each triangle in mesh.triangles()
    // order. Recomputed only when terrain/time/shadow mode changes, not on
    // every paint.
    std::vector<QColor> m_faceShadeColors;
    bool m_shadingDirty{true};
};
