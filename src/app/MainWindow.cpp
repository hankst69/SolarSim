#include "MainWindow.h"

#include "HeightDataSources.h"
#if !defined(SOLARSIM_USE_OPENGL)
#include "SceneView.h"
#endif

#include "geolib/CameraPosition.h"
#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/HorizonDome.h"

#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace {

/// Start location of the application, taken from the README example.
constexpr double kHomeLatitudeDeg = 49.56255;
constexpr double kHomeLongitudeDeg = 11.14493;

/// Extent (half size) of the rendered terrain patch in metres.
constexpr double kSceneExtentM = 400.0;

/// Grid spacing of the terrain mesh in metres.
constexpr double kSceneGridSpacingM = 10.0;

/// Resolution of the time slider: one step per minute.
constexpr int kSliderStepsPerMinute = 1;

/// Interval between playback ticks in milliseconds.
constexpr int kPlaybackTickIntervalMs = 40;

/// Total wall-clock duration of a full sunrise-to-sunset playback in
/// milliseconds (about one minute).
constexpr double kPlaybackDurationMs = 60000.0;

double minutesOfDay(const geo::DateTimeUtc& utc)
{
    return utc.hour * 60.0 + utc.minute + utc.second / 60.0;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_location(kHomeLatitudeDeg, kHomeLongitudeDeg)
{
    buildUi();

    const QDate today = QDate::currentDate();
    m_dateEdit->setDate(today);

    rebuildScene();
    rebuildSunPath();
    onResetCamera();

    setWindowTitle(tr("SolarSim"));
    resize(1080, 720);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_sceneView = new SceneViewWidget(central);
    m_sceneView->setObjectName(QStringLiteral("sceneView"));
    layout->addWidget(m_sceneView, 1);

    // Time control: scroll through the day from sunrise to sunset.
    auto* timeBox = new QGroupBox(tr("Date and time of day"), central);
    auto* timeLayout = new QVBoxLayout(timeBox);

    auto* dateLayout = new QHBoxLayout();
    m_dateEdit = new QDateEdit(timeBox);
    m_dateEdit->setObjectName(QStringLiteral("dateEdit"));
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    dateLayout->addWidget(new QLabel(tr("Date:"), timeBox));
    dateLayout->addWidget(m_dateEdit);
    dateLayout->addStretch(1);
    timeLayout->addLayout(dateLayout);

    auto* sliderGrid = new QGridLayout();
    sliderGrid->addWidget(new QLabel(tr("Sunrise"), timeBox), 0, 0, Qt::AlignHCenter);

    m_timeSlider = new QSlider(Qt::Horizontal, timeBox);
    m_timeSlider->setObjectName(QStringLiteral("timeSlider"));
    m_timeSlider->setTracking(true);
    sliderGrid->addWidget(m_timeSlider, 0, 1);

    sliderGrid->addWidget(new QLabel(tr("Sunset"), timeBox), 0, 2, Qt::AlignHCenter);

    m_sunriseTimeLabel = new QLabel(timeBox);
    m_sunriseTimeLabel->setObjectName(QStringLiteral("sunriseTimeLabel"));
    m_sunriseTimeLabel->setAlignment(Qt::AlignHCenter);
    sliderGrid->addWidget(m_sunriseTimeLabel, 1, 0);

    // Playback controls: jump to sunrise, play/pause, jump to sunset, with
    // the current time shown tight next to the play/pause button.
    auto* playLayout = new QHBoxLayout();
    playLayout->addStretch(1);

    m_jumpToStartButton = new QPushButton(tr("|<"), timeBox);
    m_jumpToStartButton->setObjectName(QStringLiteral("jumpToStartButton"));
    playLayout->addWidget(m_jumpToStartButton);

    m_playPauseButton = new QPushButton(tr("Play"), timeBox);
    m_playPauseButton->setObjectName(QStringLiteral("playPauseButton"));
    playLayout->addWidget(m_playPauseButton);

    m_jumpToEndButton = new QPushButton(tr(">|"), timeBox);
    m_jumpToEndButton->setObjectName(QStringLiteral("jumpToEndButton"));
    playLayout->addWidget(m_jumpToEndButton);

    m_timeLabel = new QLabel(timeBox);
    m_timeLabel->setObjectName(QStringLiteral("timeLabel"));
    m_timeLabel->setMinimumWidth(120);
    m_timeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    playLayout->addWidget(m_timeLabel);

    playLayout->addStretch(1);
    sliderGrid->addLayout(playLayout, 1, 1);

    m_sunsetTimeLabel = new QLabel(timeBox);
    m_sunsetTimeLabel->setObjectName(QStringLiteral("sunsetTimeLabel"));
    m_sunsetTimeLabel->setAlignment(Qt::AlignHCenter);
    sliderGrid->addWidget(m_sunsetTimeLabel, 1, 2);

    sliderGrid->setColumnStretch(1, 1);
    timeLayout->addLayout(sliderGrid);

    layout->addWidget(timeBox);

    // Camera control.
    auto* cameraBox = new QGroupBox(tr("Camera"), central);
    auto* cameraLayout = new QHBoxLayout(cameraBox);

    m_azimuthSpin = new QDoubleSpinBox(cameraBox);
    m_azimuthSpin->setObjectName(QStringLiteral("azimuthSpin"));
    m_azimuthSpin->setRange(0.0, 360.0);
    m_azimuthSpin->setWrapping(true);
    m_azimuthSpin->setSuffix(QStringLiteral(" deg"));

    m_elevationSpin = new QDoubleSpinBox(cameraBox);
    m_elevationSpin->setObjectName(QStringLiteral("elevationSpin"));
    m_elevationSpin->setRange(geo::CameraPosition::kMinElevationDeg,
                              geo::CameraPosition::kMaxElevationDeg);
    m_elevationSpin->setSuffix(QStringLiteral(" deg"));

    m_rangeSpin = new QDoubleSpinBox(cameraBox);
    m_rangeSpin->setObjectName(QStringLiteral("rangeSpin"));
    m_rangeSpin->setRange(geo::CameraPosition::kMinRangeM, 20000.0);
    m_rangeSpin->setSingleStep(10.0);
    m_rangeSpin->setSuffix(QStringLiteral(" m"));

    auto* cameraForm = new QFormLayout();
    cameraForm->addRow(tr("Azimuth:"), m_azimuthSpin);
    cameraForm->addRow(tr("Elevation:"), m_elevationSpin);
    cameraForm->addRow(tr("Distance:"), m_rangeSpin);
    cameraLayout->addLayout(cameraForm);

    auto* resetButton = new QPushButton(tr("Reset camera"), cameraBox);
    resetButton->setObjectName(QStringLiteral("resetCameraButton"));
    cameraLayout->addWidget(resetButton);

    m_sunLabel = new QLabel(cameraBox);
    m_sunLabel->setObjectName(QStringLiteral("sunLabel"));
    cameraLayout->addWidget(m_sunLabel, 1);

    layout->addWidget(cameraBox);

    setCentralWidget(central);
    statusBar();

    connect(m_dateEdit, &QDateEdit::dateChanged, this, &MainWindow::onDateChanged);
    connect(m_timeSlider, &QSlider::valueChanged, this, &MainWindow::onTimeSliderChanged);
    connect(m_azimuthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onCameraSpinChanged);
    connect(m_elevationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onCameraSpinChanged);
    connect(m_rangeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onCameraSpinChanged);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::onResetCamera);
    connect(m_sceneView, &SceneViewWidget::cameraChanged, this, &MainWindow::onCameraChangedByView);

    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(kPlaybackTickIntervalMs);
    connect(m_playTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTick);
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(m_jumpToStartButton, &QPushButton::clicked, this, &MainWindow::onJumpToStart);
    connect(m_jumpToEndButton, &QPushButton::clicked, this, &MainWindow::onJumpToEnd);
}

void MainWindow::rebuildScene()
{
    // Register the concrete height data sources (Bavaria DGM1, World
    // Copernicus DEM GLO-30) on top of the flat fallback, so the app prefers
    // real elevation data where available.
    geo::HeightDataSourceRegistry& registry = geo::HeightDataSourceRegistry::instance();
    if (registry.sources().empty()) {
        registry.addSource(std::make_shared<geo::FlatHeightDataSource>());
    }
    registerHeightDataSources();

    const geo::HorizonDome dome = geo::HorizonDome::fromHeightDataSourceRegistry(m_location);

    geo::TerrainModel::Config config;
    config.extentM = kSceneExtentM;
    config.gridSpacingM = kSceneGridSpacingM;
    config.clipToDomeCircle = false;

    m_terrain = std::make_shared<geo::TerrainModel>(
        dome, registry.selectSource(m_location), config);

    m_sceneView->setTerrain(m_terrain);
}

void MainWindow::rebuildSunPath()
{
    if (!m_terrain) {
        return;
    }

    setPlaying(false);

    const QDate date = m_dateEdit->date();
    m_sunPath = std::make_unique<geo::SunPath>(m_terrain->dome(), date.year(), date.month(),
                                               date.day());

    // The slider spans sunrise to sunset. Polar day/night falls back to the
    // whole day.
    geo::DateTimeUtc rise;
    geo::DateTimeUtc set;
    if (m_sunPath->sunrise(rise) && m_sunPath->sunset(set)) {
        m_dayStartMinutes = minutesOfDay(rise);
        m_dayEndMinutes = minutesOfDay(set);

        m_sunriseTimeLabel->setText(QStringLiteral("%1:%2 UTC")
                                        .arg(rise.hour, 2, 10, QLatin1Char('0'))
                                        .arg(rise.minute, 2, 10, QLatin1Char('0')));
        m_sunsetTimeLabel->setText(QStringLiteral("%1:%2 UTC")
                                       .arg(set.hour, 2, 10, QLatin1Char('0'))
                                       .arg(set.minute, 2, 10, QLatin1Char('0')));
    } else {
        m_dayStartMinutes = 0.0;
        m_dayEndMinutes = 24.0 * 60.0;

        m_sunriseTimeLabel->setText(tr("--:--"));
        m_sunsetTimeLabel->setText(tr("--:--"));
    }
    if (m_dayEndMinutes <= m_dayStartMinutes) {
        m_dayEndMinutes = m_dayStartMinutes + 1.0;
    }

    const int steps =
        static_cast<int>(std::lround((m_dayEndMinutes - m_dayStartMinutes) * kSliderStepsPerMinute));

    const bool wasUpdating = m_updatingControls;
    m_updatingControls = true;
    m_timeSlider->setRange(0, std::max(1, steps));
    m_timeSlider->setValue(m_timeSlider->maximum() / 2); // start at solar noon
    m_updatingControls = wasUpdating;

    // Playback runs from sunrise to sunset in roughly kPlaybackDurationMs.
    const double ticks = kPlaybackDurationMs / kPlaybackTickIntervalMs;
    m_playStepPerTick = std::max(1.0, m_timeSlider->maximum() / ticks);

    applyTime(m_timeSlider->value());
}

geo::DateTimeUtc MainWindow::timeForSlider(int value) const
{
    const QDate date = m_dateEdit->date();

    double minutes = m_dayStartMinutes + static_cast<double>(value) / kSliderStepsPerMinute;
    minutes = std::min(minutes, m_dayEndMinutes);

    const int hour = static_cast<int>(minutes / 60.0);
    const int minute = static_cast<int>(minutes) % 60;
    const double second = (minutes - std::floor(minutes)) * 60.0;

    return geo::DateTimeUtc(date.year(), date.month(), date.day(), hour, minute, second);
}

void MainWindow::applyTime(int sliderValue)
{
    const geo::DateTimeUtc utc = timeForSlider(sliderValue);
    m_sceneView->setDateTime(utc);
    updateStatus(utc);
}

void MainWindow::updateStatus(const geo::DateTimeUtc& utc)
{
    m_timeLabel->setText(QStringLiteral("%1:%2 UTC")
                             .arg(utc.hour, 2, 10, QLatin1Char('0'))
                             .arg(utc.minute, 2, 10, QLatin1Char('0')));

    const geo::SunPosition sun(m_location, utc);
    m_sunLabel->setText(tr("Sun: azimuth %1 deg, elevation %2 deg")
                            .arg(sun.azimuth(), 0, 'f', 1)
                            .arg(sun.elevation(), 0, 'f', 1));

    statusBar()->showMessage(tr("Location %1, %2 - terrain %3")
                                 .arg(m_location.latitude(), 0, 'f', 5)
                                 .arg(m_location.longitude(), 0, 'f', 5)
                                 .arg(m_terrain && m_terrain->hasHeightData() && m_terrain->source()
                                          ? QString::fromStdString(m_terrain->source()->name())
                                          : tr("no elevation data")));
}

void MainWindow::updateCameraControls()
{
    const geo::CameraPosition* camera = m_sceneView->camera();
    if (!camera) {
        return;
    }

    m_updatingControls = true;
    m_azimuthSpin->setValue(camera->azimuth());
    m_elevationSpin->setValue(camera->elevation());
    m_rangeSpin->setValue(camera->range());
    m_updatingControls = false;
}

void MainWindow::onDateChanged()
{
    rebuildSunPath();
}

void MainWindow::onTimeSliderChanged(int value)
{
    if (m_updatingControls) {
        return;
    }
    applyTime(value);
}

void MainWindow::onCameraSpinChanged()
{
    if (m_updatingControls || !m_terrain) {
        return;
    }

    m_sceneView->setCamera(geo::CameraPosition::fromOrbit(m_terrain->dome(),
                                                          m_azimuthSpin->value(),
                                                          m_elevationSpin->value(),
                                                          m_rangeSpin->value()));
}

void MainWindow::onCameraChangedByView()
{
    updateCameraControls();
}

void MainWindow::onResetCamera()
{
    if (!m_terrain) {
        return;
    }

    // Start with an overview of the whole modelled patch.
    const geo::CameraPosition camera = geo::CameraPosition::fromOrbit(
        m_terrain->dome(), m_terrain->dome().standpoint().latitude() >= 0.0 ? 180.0 : 0.0, 25.0,
        m_terrain->extent() * 1.5);

    m_sceneView->setCamera(camera);
    updateCameraControls();
}

void MainWindow::setPlaying(bool playing)
{
    if (m_isPlaying == playing) {
        return;
    }

    m_isPlaying = playing;
    if (m_isPlaying) {
        // Restart from the beginning if playback had already reached the end.
        if (m_timeSlider->value() >= m_timeSlider->maximum()) {
            m_timeSlider->setValue(m_timeSlider->minimum());
        }
        m_playPauseButton->setText(tr("Pause"));
        m_playTimer->start();
    } else {
        m_playTimer->stop();
        m_playPauseButton->setText(tr("Play"));
    }
}

void MainWindow::onPlayPauseClicked()
{
    setPlaying(!m_isPlaying);
}

void MainWindow::onJumpToStart()
{
    setPlaying(false);
    m_timeSlider->setValue(m_timeSlider->minimum());
}

void MainWindow::onJumpToEnd()
{
    setPlaying(false);
    m_timeSlider->setValue(m_timeSlider->maximum());
}

void MainWindow::onPlaybackTick()
{
    const int next = m_timeSlider->value() + static_cast<int>(std::lround(m_playStepPerTick));
    if (next >= m_timeSlider->maximum()) {
        m_timeSlider->setValue(m_timeSlider->maximum());
        setPlaying(false);
        return;
    }
    m_timeSlider->setValue(next);
}
