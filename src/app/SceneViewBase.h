#pragma once

#include "geolib/ProgressClass.h"
#include "geolib/CameraPosition.h"
#include "geolib/DateTimeUtc.h"
#include "geolib/SunPath.h"
#include "geolib/SunLight.h"
#include "geolib/TerrainModel.h"

#include <QPoint>

#include <memory>
#include <string>
#include <vector>

class QMouseEvent;
class QWheelEvent;

/// Base class holding the state and behaviour shared by the different
/// terrain scene rendering strategies (SceneViewCPU: software rasterizer,
/// SceneViewGL: OpenGL, SceneViewWEBGPU: WebGPU).
///
/// This is intentionally not a QObject/QWidget itself: Qt does not support
/// deriving a common QWidget base shared by both a QWidget subclass and a
/// QOpenGLWidget subclass (that would be an unsupported QObject diamond).
/// Instead each concrete rendering strategy derives from both its own Qt
/// widget base (QWidget/QOpenGLWidget) and from SceneViewBase, declares its
/// own Q_OBJECT/cameraChanged() signal, and forwards state changes and
/// mouse-driven camera input into this base via the protected hooks below.
class SceneViewBase {
public:
    virtual ~SceneViewBase() = default;

    void setTerrain(std::shared_ptr<const geo::TerrainModel> terrain);
    void setDateTime(const geo::DateTimeUtc& utc);
    void setCamera(const geo::CameraPosition& camera);
    void setShadowsEnabled(bool enabled);

    const geo::CameraPosition* camera() const { return m_camera.get(); }
    bool shadowsEnabled() const { return m_shadowsEnabled; }

    void setProgressClass(geo::ProgressClassPtr progressClass)
    {
        m_progressClass = progressClass;
    }

protected:
    /// Hooks invoked after the shared state has been updated; derived
    /// classes implement these to perform the rendering-strategy specific
    /// work (upload GPU geometry, mark software shading cache dirty, etc.).
    virtual void onTerrainChanged() {}
    virtual void onDateTimeChanged() {}
    virtual void onCameraChanged() {}
    virtual void onShadowsEnabledChanged() {}

    /// Invoked by the shared mouse orbit/zoom handling below so the concrete
    /// widget can emit its own Qt cameraChanged() signal.
    virtual void emitCameraChanged() {}

    void rebuildLight();
    void rebuildSunPathOverlay();

    /// Shared mouse-driven camera orbit/zoom handling. Concrete classes call
    /// these from their mousePressEvent()/mouseMoveEvent()/wheelEvent()
    /// overrides.
    void handleOrbitMousePress(const QPoint& pos);
    void handleOrbitMouseMove(QMouseEvent* event);
    void handleOrbitWheel(QWheelEvent* event);

    void progress(int percent, std::string msg) const
    {
        if (m_progressClass) {
            int progressVal = percent % 100;
            m_progressClass->progress(progressVal, std::move(msg));
        }
    }

    std::shared_ptr<const geo::TerrainModel> m_terrain;
    std::unique_ptr<geo::CameraPosition> m_camera;
    std::unique_ptr<geo::SunLight> m_light;
    geo::DateTimeUtc m_utc;

    std::vector<geo::Vector3> m_sunPathPoints;
    geo::Vector3 m_currentSunPoint;
    bool m_hasCurrentSunPoint{false};

    bool m_shadowsEnabled{false};

    QPoint m_lastMousePos;

    geo::ProgressClassPtr m_progressClass;
    int m_progressCount{0};
};
