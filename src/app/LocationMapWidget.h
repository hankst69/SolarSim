#pragma once

#include <QWidget>

/// A simple equirectangular world map widget that lets the user pick a
/// geographic location by clicking anywhere on the map. The map itself is
/// drawn as a graticule (grid of latitude/longitude lines) rather than a
/// bitmap texture, so no external image resources are required.
class LocationMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit LocationMapWidget(QWidget* parent = nullptr);

    /// Updates the marker position without emitting locationPicked().
    void setLocation(double latitudeDeg, double longitudeDeg);

    QSize sizeHint() const override;

signals:
    /// Emitted when the user clicks (or drags) on the map to pick a new
    /// location.
    void locationPicked(double latitudeDeg, double longitudeDeg);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void pickFromPosition(const QPoint& pos);

    double m_latitudeDeg{0.0};
    double m_longitudeDeg{0.0};
};
