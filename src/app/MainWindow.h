#pragma once

#include "GeoDataSources.h"

#include "geolib/DateTimeUtc.h"
#include "geolib/GeoLocation.h"
#include "geolib/SunPath.h"
#include "geolib/TerrainModel.h"

#include <QMainWindow>

#include <memory>

#if defined(SOLARSIM_USE_OPENGL)
#include "GLSceneView.h"
using SceneViewWidget = GLSceneView;
#else
class SceneView;
using SceneViewWidget = SceneView;
#endif

class QDateEdit;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

/// Main window of the SolarSim application: a terrain scene lit by the
/// simulated sun, with a time slider from sunrise to sunset and interactive
/// camera controls.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onDateChanged();
    void onTimeSliderChanged(int value);
    void onCameraSpinChanged();
    void onCameraChangedByView();
    void onResetCamera();
    void onPlayPauseClicked();
    void onJumpToStart();
    void onJumpToEnd();
    void onPlaybackTick();
    void onLocalTimeToggled(bool checked);
    void initScene();
    void updateScene();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void registerGeoDataSources();
    void buildUi();
    void rebuildScene();
    void rebuildSunPath();
    void applyTime(int sliderValue);
    void updateCameraControls();
    void updateStatus(const geo::DateTimeUtc& utc);
    void updateSunriseSunsetLabels();
    geo::DateTimeUtc displayTimeFor(const geo::DateTimeUtc& utc) const;
    geo::DateTimeUtc timeForSlider(int value) const;
    void setPlaying(bool playing);

    GeoDataSources* m_geo_data_sources{nullptr};

    geo::GeoLocation m_location;
    std::shared_ptr<geo::TerrainModel> m_terrain;
    std::unique_ptr<geo::SunPath> m_sunPath;

    double m_dayStartMinutes{0.0};
    double m_dayEndMinutes{24.0 * 60.0};

    bool m_sceneInitialized{false};
    SceneViewWidget* m_sceneView{nullptr};
    QDateEdit* m_dateEdit{nullptr};
    QCheckBox* m_localTimeCheckBox{nullptr};
    QSlider* m_timeSlider{nullptr};
    QLabel* m_timeLabel{nullptr};
    QLabel* m_sunriseTimeLabel{nullptr};
    QLabel* m_sunsetTimeLabel{nullptr};
    QLabel* m_sunLabel{nullptr};
    QPushButton* m_jumpToStartButton{nullptr};
    QPushButton* m_playPauseButton{nullptr};
    QPushButton* m_jumpToEndButton{nullptr};
    QTimer* m_playTimer{nullptr};
    double m_playStepPerTick{1.0};
    QDoubleSpinBox* m_azimuthSpin{nullptr};
    QDoubleSpinBox* m_elevationSpin{nullptr};
    QDoubleSpinBox* m_rangeSpin{nullptr};

    bool m_updatingControls{false};
    bool m_isPlaying{false};
    geo::DateTimeUtc m_sunriseUtc;
    geo::DateTimeUtc m_sunsetUtc;
    bool m_hasSunriseSunset{false};
};
