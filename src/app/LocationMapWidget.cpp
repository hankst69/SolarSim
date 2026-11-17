#include "LocationMapWidget.h"

#include "ContinentOutlines.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

LocationMapWidget::LocationMapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(240, 120);
    setCursor(Qt::CrossCursor);
    setToolTip(tr("Click to pick a location"));
}

void LocationMapWidget::setLocation(double latitudeDeg, double longitudeDeg)
{
    m_latitudeDeg = latitudeDeg;
    m_longitudeDeg = longitudeDeg;
    update();
}

QSize LocationMapWidget::sizeHint() const
{
    return QSize(360, 180);
}

void LocationMapWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF mapRect(0, 0, width(), height());

    // Ocean background and land-less equirectangular placeholder map.
    painter.fillRect(mapRect, QColor(40, 80, 130));

    // Simple graticule: lines every 30 degrees of longitude / latitude.
    painter.setPen(QPen(QColor(90, 130, 170), 1, Qt::DotLine));
    for (int lon = -180; lon <= 180; lon += 30) {
        const double x = (lon + 180.0) / 360.0 * width();
        painter.drawLine(QPointF(x, 0), QPointF(x, height()));
    }
    for (int lat = -90; lat <= 90; lat += 30) {
        const double y = (90.0 - lat) / 180.0 * height();
        painter.drawLine(QPointF(0, y), QPointF(width(), y));
    }

    // Equator and prime meridian highlighted.
    painter.setPen(QPen(QColor(150, 180, 210), 1, Qt::SolidLine));
    painter.drawLine(QPointF(width() / 2.0, 0), QPointF(width() / 2.0, height()));
    painter.drawLine(QPointF(0, height() / 2.0), QPointF(width(), height() / 2.0));

    // Simplified continent outlines, projected from (longitude, latitude)
    // degrees to widget pixel coordinates (equirectangular projection).
    painter.setPen(QPen(QColor(60, 90, 50), 1));
    painter.setBrush(QColor(90, 140, 80));
    for (const auto& outline : app::continentOutlines()) {
        QPainterPath path;
        for (int i = 0; i < outline.size(); ++i) {
            const double lon = outline[i].x();
            const double lat = outline[i].y();
            const double x = (lon + 180.0) / 360.0 * width();
            const double y = (90.0 - lat) / 180.0 * height();
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        path.closeSubpath();
        painter.drawPath(path);
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(220, 230, 240));
    painter.drawRect(mapRect.adjusted(0, 0, -1, -1));

    // Marker for the currently selected location.
    const double x = (m_longitudeDeg + 180.0) / 360.0 * width();
    const double y = (90.0 - m_latitudeDeg) / 180.0 * height();

    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(QColor(255, 60, 60));
    painter.drawEllipse(QPointF(x, y), 5, 5);
}

void LocationMapWidget::mousePressEvent(QMouseEvent* event)
{
    pickFromPosition(event->pos());
}

void LocationMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        pickFromPosition(event->pos());
    }
}

void LocationMapWidget::pickFromPosition(const QPoint& pos)
{
    const double clampedX = std::clamp<double>(pos.x(), 0.0, width());
    const double clampedY = std::clamp<double>(pos.y(), 0.0, height());

    const double longitudeDeg = clampedX / std::max(1, width()) * 360.0 - 180.0;
    const double latitudeDeg = 90.0 - clampedY / std::max(1, height()) * 180.0;

    m_longitudeDeg = longitudeDeg;
    m_latitudeDeg = latitudeDeg;
    update();

    emit locationPicked(latitudeDeg, longitudeDeg);
}
