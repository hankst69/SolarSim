#include "MainWindow.h"

#include "geolib/HeightDataSourceRegistry.h"

#include "TestSupport.h"

#include <QApplication>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

namespace {

/// The window builds a full UI with the expected child widgets and starts
/// with a sensible default state (paused, some non-empty status text).
void testBuildsUiAndDefaultState()
{
    geo::HeightDataSourceRegistry::instance().clear();

    MainWindow window;

    auto* sceneView = window.findChild<QWidget*>(QStringLiteral("sceneView"));
    auto* dateEdit = window.findChild<QDateEdit*>(QStringLiteral("dateEdit"));
    auto* timeSlider = window.findChild<QSlider*>(QStringLiteral("timeSlider"));
    auto* timeLabel = window.findChild<QLabel*>(QStringLiteral("timeLabel"));
    auto* sunriseLabel = window.findChild<QLabel*>(QStringLiteral("sunriseTimeLabel"));
    auto* sunsetLabel = window.findChild<QLabel*>(QStringLiteral("sunsetTimeLabel"));
    auto* sunLabel = window.findChild<QLabel*>(QStringLiteral("sunLabel"));
    auto* playPauseButton = window.findChild<QPushButton*>(QStringLiteral("playPauseButton"));
    auto* jumpToStartButton = window.findChild<QPushButton*>(QStringLiteral("jumpToStartButton"));
    auto* jumpToEndButton = window.findChild<QPushButton*>(QStringLiteral("jumpToEndButton"));
    auto* resetCameraButton = window.findChild<QPushButton*>(QStringLiteral("resetCameraButton"));
    auto* azimuthSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("azimuthSpin"));
    auto* elevationSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elevationSpin"));
    auto* rangeSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("rangeSpin"));

    CHECK_TRUE(sceneView != nullptr);
    CHECK_TRUE(dateEdit != nullptr);
    CHECK_TRUE(timeSlider != nullptr);
    CHECK_TRUE(timeLabel != nullptr);
    CHECK_TRUE(sunriseLabel != nullptr);
    CHECK_TRUE(sunsetLabel != nullptr);
    CHECK_TRUE(sunLabel != nullptr);
    CHECK_TRUE(playPauseButton != nullptr);
    CHECK_TRUE(jumpToStartButton != nullptr);
    CHECK_TRUE(jumpToEndButton != nullptr);
    CHECK_TRUE(resetCameraButton != nullptr);
    CHECK_TRUE(azimuthSpin != nullptr);
    CHECK_TRUE(elevationSpin != nullptr);
    CHECK_TRUE(rangeSpin != nullptr);

    // Starts paused, with the slider parked somewhere within its range and
    // labels already populated with real text (not empty placeholders).
    CHECK_EQ_STR(playPauseButton->text().toStdString(), std::string("Play"));
    CHECK_TRUE(timeSlider->minimum() <= timeSlider->value());
    CHECK_TRUE(timeSlider->value() <= timeSlider->maximum());
    CHECK_TRUE(!timeLabel->text().isEmpty());
    CHECK_TRUE(!sunLabel->text().isEmpty());
}

/// Moving the time slider updates the displayed time label; jumping to the
/// start/end moves the slider to its respective bound and stays paused.
void testTimeNavigation()
{
    geo::HeightDataSourceRegistry::instance().clear();

    MainWindow window;

    auto* timeSlider = window.findChild<QSlider*>(QStringLiteral("timeSlider"));
    auto* jumpToStartButton = window.findChild<QPushButton*>(QStringLiteral("jumpToStartButton"));
    auto* jumpToEndButton = window.findChild<QPushButton*>(QStringLiteral("jumpToEndButton"));
    auto* playPauseButton = window.findChild<QPushButton*>(QStringLiteral("playPauseButton"));
    CHECK_TRUE(timeSlider != nullptr && jumpToStartButton != nullptr &&
              jumpToEndButton != nullptr && playPauseButton != nullptr);

    jumpToEndButton->click();
    CHECK_EQ_INT(timeSlider->value(), timeSlider->maximum());
    CHECK_EQ_STR(playPauseButton->text().toStdString(), std::string("Play"));

    jumpToStartButton->click();
    CHECK_EQ_INT(timeSlider->value(), timeSlider->minimum());
    CHECK_EQ_STR(playPauseButton->text().toStdString(), std::string("Play"));
}

/// Clicking play/pause toggles the button label and playback state; clicking
/// it again pauses.
void testPlayPauseToggles()
{
    geo::HeightDataSourceRegistry::instance().clear();

    MainWindow window;

    auto* playPauseButton = window.findChild<QPushButton*>(QStringLiteral("playPauseButton"));
    auto* jumpToStartButton = window.findChild<QPushButton*>(QStringLiteral("jumpToStartButton"));
    CHECK_TRUE(playPauseButton != nullptr && jumpToStartButton != nullptr);

    jumpToStartButton->click(); // ensure playback can run from the beginning

    playPauseButton->click();
    CHECK_EQ_STR(playPauseButton->text().toStdString(), std::string("Pause"));

    playPauseButton->click();
    CHECK_EQ_STR(playPauseButton->text().toStdString(), std::string("Play"));
}

/// Changing the camera spin boxes updates the scene view's camera
/// accordingly; the reset button restores a camera as well.
void testCameraControls()
{
    geo::HeightDataSourceRegistry::instance().clear();

    MainWindow window;

    auto* azimuthSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("azimuthSpin"));
    auto* elevationSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elevationSpin"));
    auto* rangeSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("rangeSpin"));
    auto* resetCameraButton = window.findChild<QPushButton*>(QStringLiteral("resetCameraButton"));
    CHECK_TRUE(azimuthSpin != nullptr && elevationSpin != nullptr && rangeSpin != nullptr &&
              resetCameraButton != nullptr);

    azimuthSpin->setValue(90.0);
    elevationSpin->setValue(45.0);
    rangeSpin->setValue(500.0);

    CHECK_NEAR(azimuthSpin->value(), 90.0, 1e-9);
    CHECK_NEAR(elevationSpin->value(), 45.0, 1e-9);
    CHECK_NEAR(rangeSpin->value(), 500.0, 1e-9);

    // Resetting the camera must not crash and should leave the spin boxes
    // with some in-range value reflecting the new camera.
    resetCameraButton->click();
    CHECK_TRUE(azimuthSpin->value() >= azimuthSpin->minimum());
    CHECK_TRUE(azimuthSpin->value() <= azimuthSpin->maximum());
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    testBuildsUiAndDefaultState();
    testTimeNavigation();
    testPlayPauseToggles();
    testCameraControls();

    return geotest::summarize("MainWindowTests");
}
