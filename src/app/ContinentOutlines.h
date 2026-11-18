#pragma once

#include <QVector>
#include <QPointF>

namespace app {

/// Highly simplified continent outlines for the location picker map.
/// Coordinates are given as (longitude, latitude) in degrees and are not
/// meant to be geographically precise - they only provide enough visual
/// context to recognize continents on a small equirectangular map.
inline const QVector<QVector<QPointF>>& continentOutlines()
{
    static const QVector<QVector<QPointF>> outlines = {
        // North America
        {
            {-168, 66}, {-161, 70}, {-141, 70}, {-125, 55}, {-130, 52},
            {-123, 48}, {-124, 40}, {-117, 32}, {-106, 23}, {-97, 20},
            {-90, 16}, {-83, 9}, {-80, 8}, {-77, 8}, {-81, 18},
            {-80, 25}, {-81, 31}, {-75, 35}, {-71, 41}, {-70, 43},
            {-66, 45}, {-60, 47}, {-55, 51}, {-65, 60}, {-75, 62},
            {-85, 68}, {-95, 70}, {-110, 72}, {-130, 70}, {-150, 70},
            {-168, 66}
        },
        // South America
        {
            {-77, 8}, {-80, 2}, {-80, -5}, {-81, -15}, {-71, -18},
            {-70, -30}, {-71, -40}, {-74, -52}, {-68, -55}, {-65, -55},
            {-62, -45}, {-58, -35}, {-48, -25}, {-35, -8}, {-40, 0},
            {-50, 1}, {-60, 8}, {-66, 10}, {-77, 8}
        },
        // Africa
        {
            {-17, 15}, {-16, 21}, {-10, 30}, {0, 36}, {10, 37},
            {20, 33}, {32, 31}, {35, 27}, {43, 12}, {51, 10},
            {51, 2}, {42, -5}, {40, -15}, {35, -24}, {27, -33},
            {18, -34}, {15, -27}, {12, -18}, {13, -6}, {9, 4},
            {4, 6}, {-5, 5}, {-10, 7}, {-17, 15}
        },
        // Europe
        {
            {-10, 44}, {-9, 38}, {-1, 37}, {3, 38}, {9, 44},
            {7, 48}, {0, 50}, {2, 51}, {8, 54}, {12, 55},
            {18, 55}, {23, 54}, {30, 60}, {28, 65}, {25, 70},
            {15, 68}, {5, 62}, {5, 58}, {-5, 56}, {-9, 51},
            {-10, 44}
        },
        // Asia
        {
            {28, 65}, {40, 66}, {55, 68}, {70, 70}, {90, 73},
            {110, 73}, {130, 72}, {140, 60}, {135, 50}, {142, 45},
            {140, 36}, {130, 33}, {122, 30}, {121, 25}, {110, 18},
            {106, 10}, {100, 6}, {95, 5}, {98, 15}, {92, 22},
            {88, 22}, {80, 8}, {77, 8}, {72, 20}, {62, 25},
            {55, 27}, {48, 30}, {44, 33}, {36, 36}, {33, 36},
            {28, 42}, {27, 50}, {28, 58}, {28, 65}
        },
        // Australia
        {
            {113, -22}, {114, -29}, {118, -35}, {128, -33}, {137, -35},
            {141, -38}, {150, -38}, {153, -28}, {145, -17}, {142, -11},
            {136, -12}, {131, -12}, {126, -14}, {122, -18}, {113, -22}
        },
        // Greenland
        {
            {-45, 60}, {-55, 65}, {-50, 72}, {-35, 78}, {-20, 81},
            {-25, 70}, {-35, 65}, {-45, 60}
        },
        // Antarctica (strip at bottom of map)
        {
            {-180, -75}, {-90, -78}, {0, -75}, {90, -78}, {180, -75},
            {180, -90}, {-180, -90}, {-180, -75}
        }
    };
    return outlines;
}

namespace detail {

/// Catmull-Rom interpolation of a closed polygon, used to turn the coarse,
/// hand-authored outlines above into smoother, more natural looking
/// coastlines for the higher resolution floating preview. This does not add
/// genuine geographic detail, but avoids the obviously blocky look of the
/// low vertex-count polygons when zoomed in.
inline QPointF catmullRom(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, double t)
{
    const double t2 = t * t;
    const double t3 = t2 * t;

    const double x = 0.5 * ((2.0 * p1.x())
        + (-p0.x() + p2.x()) * t
        + (2.0 * p0.x() - 5.0 * p1.x() + 4.0 * p2.x() - p3.x()) * t2
        + (-p0.x() + 3.0 * p1.x() - 3.0 * p2.x() + p3.x()) * t3);

    const double y = 0.5 * ((2.0 * p1.y())
        + (-p0.y() + p2.y()) * t
        + (2.0 * p0.y() - 5.0 * p1.y() + 4.0 * p2.y() - p3.y()) * t2
        + (-p0.y() + 3.0 * p1.y() - 3.0 * p2.y() + p3.y()) * t3);

    return QPointF(x, y);
}

inline QVector<QPointF> smoothClosedPolygon(const QVector<QPointF>& polygon, int subdivisionsPerSegment)
{
    const int n = polygon.size();
    if (n < 4 || subdivisionsPerSegment <= 1) {
        return polygon;
    }

    QVector<QPointF> smoothed;
    smoothed.reserve(n * subdivisionsPerSegment);

    for (int i = 0; i < n; ++i) {
        const QPointF& p0 = polygon[(i - 1 + n) % n];
        const QPointF& p1 = polygon[i];
        const QPointF& p2 = polygon[(i + 1) % n];
        const QPointF& p3 = polygon[(i + 2) % n];

        for (int s = 0; s < subdivisionsPerSegment; ++s) {
            const double t = static_cast<double>(s) / subdivisionsPerSegment;
            smoothed.append(catmullRom(p0, p1, p2, p3, t));
        }
    }

    return smoothed;
}

} // namespace detail

/// Smoothed, higher resolution version of continentOutlines(), intended for
/// use when the location map is shown enlarged (e.g. in the floating
/// hover preview) where the coarse polygon edges of the base outlines
/// would otherwise be clearly visible.
inline const QVector<QVector<QPointF>>& continentOutlinesDetailed()
{
    static const QVector<QVector<QPointF>> detailedOutlines = [] {
        QVector<QVector<QPointF>> result;
        result.reserve(continentOutlines().size());
        for (const auto& outline : continentOutlines()) {
            result.append(detail::smoothClosedPolygon(outline, 8));
        }
        return result;
    }();
    return detailedOutlines;
}

} // namespace app

