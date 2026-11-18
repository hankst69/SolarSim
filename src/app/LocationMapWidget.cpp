#include "LocationMapWidget.h"

#include "ContinentOutlines.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace {
/// Classic equirectangular/Mercator-style world map width/height ratio.
constexpr double kMapAspectRatio = 2.0;

/// Fraction of the top-level window's size used for the floating preview
/// shown on hover.
constexpr double kFloatingPreviewWindowFraction = 0.9;
} // namespace

LocationMapWidget::LocationMapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(240, 120);
    setCursor(Qt::CrossCursor);
    setToolTip(tr("Click to pick a location"));
    setMouseTracking(true);
}

LocationMapWidget::~LocationMapWidget()
{
    delete m_floatingPreview;
}

void LocationMapWidget::setLocation(double latitudeDeg, double longitudeDeg)
{
    m_latitudeDeg = latitudeDeg;
    m_longitudeDeg = longitudeDeg;
    update();

    if (m_floatingPreview) {
        m_floatingPreview->setLocation(latitudeDeg, longitudeDeg);
    }
}

QSize LocationMapWidget::sizeHint() const
{
    return QSize(360, 180);
}

QRectF LocationMapWidget::mapRect() const
{
    // Letterbox the drawn map so it always keeps the fixed world-map aspect
    // ratio, regardless of the actual widget size granted by the layout.
    const double w = width();
    const double h = height();
    if (w <= 0.0 || h <= 0.0) {
        return QRectF(0, 0, w, h);
    }

    double mapWidth = w;
    double mapHeight = mapWidth / kMapAspectRatio;
    if (mapHeight > h) {
        mapHeight = h;
        mapWidth = mapHeight * kMapAspectRatio;
    }

    const double x = (w - mapWidth) / 2.0;
    const double y = (h - mapHeight) / 2.0;
    return QRectF(x, y, mapWidth, mapHeight);
}

void LocationMapWidget::drawMap(QPainter& painter, const QRectF& rect) const
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Ocean background and land-less equirectangular placeholder map.
    painter.fillRect(rect, QColor(40, 80, 130));

    // Simple graticule: lines every 30 degrees of longitude / latitude.
    painter.setPen(QPen(QColor(90, 130, 170), 1, Qt::DotLine));
    for (int lon = -180; lon <= 180; lon += 30) {
        const double x = rect.left() + (lon + 180.0) / 360.0 * rect.width();
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    for (int lat = -90; lat <= 90; lat += 30) {
        const double y = rect.top() + (90.0 - lat) / 180.0 * rect.height();
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    // Equator and prime meridian highlighted.
    painter.setPen(QPen(QColor(150, 180, 210), 1, Qt::SolidLine));
    painter.drawLine(QPointF(rect.center().x(), rect.top()), QPointF(rect.center().x(), rect.bottom()));
    painter.drawLine(QPointF(rect.left(), rect.center().y()), QPointF(rect.right(), rect.center().y()));

    // Simplified continent outlines, projected from (longitude, latitude)
    // degrees to widget pixel coordinates (equirectangular projection).
    painter.setPen(QPen(QColor(60, 90, 50), 1));
    painter.setBrush(QColor(90, 140, 80));
    for (const auto& outline : app::continentOutlines()) {
        QPainterPath path;
        for (int i = 0; i < outline.size(); ++i) {
            const double lon = outline[i].x();
            const double lat = outline[i].y();
            const double x = rect.left() + (lon + 180.0) / 360.0 * rect.width();
            const double y = rect.top() + (90.0 - lat) / 180.0 * rect.height();
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
    painter.drawRect(rect.adjusted(0, 0, -1, -1));

    // Marker for the currently selected location.
    const double x = rect.left() + (m_longitudeDeg + 180.0) / 360.0 * rect.width();
    const double y = rect.top() + (90.0 - m_latitudeDeg) / 180.0 * rect.height();

    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(QColor(255, 60, 60));
    painter.drawEllipse(QPointF(x, y), 5, 5);
}

void LocationMapWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());
    drawMap(painter, mapRect());
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void LocationMapWidget::enterEvent(QEnterEvent* /*event*/)
#else
void LocationMapWidget::enterEvent(QEvent* /*event*/)
#endif
{
    if (!m_isFloatingPreview) {
        showFloatingPreview();
    }
}

void LocationMapWidget::leaveEvent(QEvent* /*event*/)
{
    if (m_isFloatingPreview) {
        hide();
        return;
    }

    // Don't hide the preview if the cursor just moved onto it.
    if (m_floatingPreview && m_floatingPreview->isVisible()
        && m_floatingPreview->geometry().contains(QCursor::pos())) {
        return;
    }
    hideFloatingPreview();
}

void LocationMapWidget::pickFromPosition(const QPoint& pos)
{
    const QRectF rect = mapRect();
    if (rect.width() <= 0.0 || rect.height() <= 0.0) {
        return;
    }

    const double clampedX = std::clamp<double>(pos.x(), rect.left(), rect.right());
    const double clampedY = std::clamp<double>(pos.y(), rect.top(), rect.bottom());

    const double longitudeDeg = (clampedX - rect.left()) / rect.width() * 360.0 - 180.0;
    const double latitudeDeg = 90.0 - (clampedY - rect.top()) / rect.height() * 180.0;

    m_longitudeDeg = longitudeDeg;
    m_latitudeDeg = latitudeDeg;
    update();

    if (m_floatingPreview) {
        m_floatingPreview->setLocation(latitudeDeg, longitudeDeg);
    }

    emit locationPicked(latitudeDeg, longitudeDeg);
}

void LocationMapWidget::ensureFloatingPreview()
{
    if (m_floatingPreview) {
        return;
    }

    m_floatingPreview = new LocationMapWidget(nullptr);
    m_floatingPreview->m_isFloatingPreview = true;
    m_floatingPreview->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_floatingPreview->setAttribute(Qt::WA_ShowWithoutActivating);
    m_floatingPreview->setLocation(m_latitudeDeg, m_longitudeDeg);

    connect(m_floatingPreview, &LocationMapWidget::locationPicked, this,
            [this](double latitudeDeg, double longitudeDeg) {
                m_latitudeDeg = latitudeDeg;
                m_longitudeDeg = longitudeDeg;
                update();
                emit locationPicked(latitudeDeg, longitudeDeg);
            });
}

void LocationMapWidget::showFloatingPreview()
{
    ensureFloatingPreview();

    // Size the preview relative to the top-level window (the application's
    // main window) rather than the screen, so it never spills onto another
    // monitor and stays visually tied to the application.
    const QWidget* topLevel = window();
    const QRect windowGeometry = topLevel ? topLevel->geometry() : geometry();

    const int previewWidth = std::max(1, static_cast<int>(windowGeometry.width() * kFloatingPreviewWindowFraction));
    const int previewHeight = std::max(1, static_cast<int>(windowGeometry.height() * kFloatingPreviewWindowFraction));

    m_floatingPreview->resize(previewWidth, previewHeight);

    // Centre the preview within the main window.
    const QPoint previewPos(windowGeometry.center().x() - previewWidth / 2,
                             windowGeometry.center().y() - previewHeight / 2);

    m_floatingPreview->move(previewPos);
    m_floatingPreview->setLocation(m_latitudeDeg, m_longitudeDeg);
    m_floatingPreview->show();
    m_floatingPreview->raise();
}

void LocationMapWidget::hideFloatingPreview()
{
    if (m_floatingPreview) {
        m_floatingPreview->hide();
    }
}
