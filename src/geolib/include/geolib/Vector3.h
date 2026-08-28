#pragma once

#include <cmath>

namespace geo {

/// Simple cartesian 3D vector used for ECEF and local ENU coordinates (metres).
struct Vector3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vector3() = default;
    Vector3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vector3 operator/(double s) const { return {x / s, y / s, z / s}; }

    double dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }

    Vector3 cross(const Vector3& o) const
    {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }

    double length() const { return std::sqrt(dot(*this)); }

    Vector3 normalized() const
    {
        const double len = length();
        return len > 0.0 ? *this / len : *this;
    }
};

inline Vector3 operator*(double s, const Vector3& v) { return v * s; }

} // namespace geo
