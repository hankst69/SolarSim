#pragma once
#pragma once

#include <QWidget>
#include <QEvent>
#include <QRectF>

class QPainter;
class QTimer;

/// A simple world map widget that lets the user pick a geographic location
/// by clicking anywhere on the map. The map itself is drawn as a graticule
/// (grid of latitude/longitude lines) plus simplified continent outlines
/// rather than a bitmap texture, so no external image resources are
/// required.
///
/// The drawn map always keeps a fixed 2:1 width/height aspect ratio (the
/// usual proportions of an equirectangular/Mercator-style world map),
/// letterboxing within whatever space the surrounding layout grants the
/// widget. While the mouse hovers over the widget, a larger floating
/// preview of the same map is shown so the user can pick a location more
/// precisely.
class LocationMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit LocationMapWidget(QWidget* parent = nullptr);
    ~LocationMapWidget() override;

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override;
#else
    void enterEvent(QEvent* event) override;
#endif
    void leaveEvent(QEvent* event) override;

private:
    void pickFromPosition(const QPoint& pos);
    QRectF mapRect() const;
    void drawMap(QPainter& painter, const QRectF& rect) const;
    void ensureFloatingPreview();
    void showFloatingPreview();
    void hideFloatingPreview();

    double m_latitudeDeg{0.0};
    double m_longitudeDeg{0.0};

    bool m_isFloatingPreview{false};
    LocationMapWidget* m_floatingPreview{nullptr};
};
