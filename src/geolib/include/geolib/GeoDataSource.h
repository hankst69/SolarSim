#pragma once

#include "geolib/GeoLocation.h"

#include <memory>
#include <string>
#include <utility>

namespace geo {

/// Axis aligned latitude/longitude bounding box (degrees) describing the area
/// covered by a height data source.
struct GeoBounds {
    double minLatitudeDeg{-90.0};
    double maxLatitudeDeg{90.0};
    double minLongitudeDeg{-180.0};
    double maxLongitudeDeg{180.0};

    static GeoBounds world() { return GeoBounds{}; }

    bool contains(double latitudeDeg, double longitudeDeg) const
    {
        return latitudeDeg >= minLatitudeDeg && latitudeDeg <= maxLatitudeDeg &&
               longitudeDeg >= minLongitudeDeg && longitudeDeg <= maxLongitudeDeg;
    }

    bool contains(const GeoLocation& location) const
    {
        return contains(location.latitude(), location.longitude());
    }
};


/// Abstract interface for GeoLocation related data (Height, Texture, Temperature, ...)
///
class GeoDataSource {
public:
    class ProgressClass {
    public:
        virtual ~ProgressClass() = default;
        virtual void progress(int percent, std::string msg) = 0;
    };

    virtual ~GeoDataSource() = default;

    /// Human readable name of the data set, used for logging and selection
    virtual std::string name() const = 0;

    /// Area for which this source can deliver heights
    virtual GeoBounds coverage() const = 0;

    /// Nominal ground sample distance of the data set in metres. Smaller values
    /// are considered "better" during source selection.
    virtual double resolution() const = 0;

    /// True if the given location lies inside the coverage of this source.
    bool covers(double latitudeDeg, double longitudeDeg) const
    {
        return coverage().contains(latitudeDeg, longitudeDeg);
    }

    bool covers(const GeoLocation& location) const
    {
        return coverage().contains(location);
    }

    void setProgressClass(std::shared_ptr<ProgressClass> progressClass)
    {
        m_progressClass = std::move(progressClass);
    }

protected:
    void progress(int percent, std::string msg) const
    {
        if (m_progressClass) {
            m_progressClass->progress(percent, std::move(msg));
        }
    }

private:
    std::shared_ptr<ProgressClass> m_progressClass;
};

using GeoDataSourcePtr = std::shared_ptr<GeoDataSource>;

} // namespace geo
