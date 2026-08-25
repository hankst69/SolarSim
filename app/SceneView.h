#pragma once

#include "geolib/CameraPosition.h"
#include "geolib/DateTimeUtc.h"
#include "geolib/SunLight.h"
#include "geolib/TerrainModel.h"

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QVector>
#include <QWidget>

#include <memory>

/// Software rendered view of the terrain scene.
///
/// The widget projects the triangle mesh of a geo::TerrainModel with a simple
/// painter's algorithm. Shading combines a weak ambient/diffuse base light with
/// the directional geo::SunLight of the current date/time, including terrain
/// cast shadows.
class SceneView : public QWidget {
    Q_OBJECT

public:
    explicit SceneView(QWidget* parent = nullptr);

    void setTerrain(std::shared_ptr<const geo::TerrainModel> terrain);
    void setDateTime(const geo::DateTimeUtc& utc);
    void setCamera(const geo::CameraPosition& camera);

    const geo::CameraPosition* camera() const { return m_camera.get(); }

signals:
    void cameraChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
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
    void collectFaces(QVector<Face>& faces) const;
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

    QPoint m_lastMousePos;
};
