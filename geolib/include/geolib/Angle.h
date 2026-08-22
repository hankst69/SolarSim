#pragma once

#include <cmath>

namespace geo {

constexpr double kPi = 3.14159265358979323846;

constexpr double degToRad(double deg) { return deg * kPi / 180.0; }
constexpr double radToDeg(double rad) { return rad * 180.0 / kPi; }

} // namespace geo
