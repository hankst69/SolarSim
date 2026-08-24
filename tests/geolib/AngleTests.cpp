#include "geolib/Angle.h"

#include "TestSupport.h"

#include <cmath>

using namespace geo;

namespace {

void testConstant()
{
    CHECK_NEAR(kPi, std::acos(-1.0), 1e-15);
}

void testDegToRad()
{
    CHECK_NEAR(degToRad(0.0), 0.0, 1e-15);
    CHECK_NEAR(degToRad(90.0), kPi / 2.0, 1e-15);
    CHECK_NEAR(degToRad(180.0), kPi, 1e-15);
    CHECK_NEAR(degToRad(360.0), 2.0 * kPi, 1e-15);
    CHECK_NEAR(degToRad(-90.0), -kPi / 2.0, 1e-15);
}

void testRadToDeg()
{
    CHECK_NEAR(radToDeg(0.0), 0.0, 1e-15);
    CHECK_NEAR(radToDeg(kPi / 2.0), 90.0, 1e-13);
    CHECK_NEAR(radToDeg(kPi), 180.0, 1e-13);
    CHECK_NEAR(radToDeg(-kPi), -180.0, 1e-13);
}

void testRoundTrip()
{
    const double values[] = {-359.9, -180.0, -45.5, 0.0, 23.4367, 48.1372, 180.0, 359.9};
    for (double deg : values) {
        CHECK_NEAR(radToDeg(degToRad(deg)), deg, 1e-12);
    }
}

/// The helpers are constexpr, so they must be usable at compile time.
void testConstexpr()
{
    constexpr double halfTurn = degToRad(180.0);
    static_assert(halfTurn > 3.14 && halfTurn < 3.15, "degToRad must be constexpr");

    constexpr double back = radToDeg(kPi);
    static_assert(back > 179.9 && back < 180.1, "radToDeg must be constexpr");

    CHECK_NEAR(halfTurn, kPi, 1e-15);
    CHECK_NEAR(back, 180.0, 1e-13);
}

/// Trigonometry through the conversion must match the well known values.
void testAgainstTrigonometry()
{
    CHECK_NEAR(std::sin(degToRad(30.0)), 0.5, 1e-12);
    CHECK_NEAR(std::cos(degToRad(60.0)), 0.5, 1e-12);
    CHECK_NEAR(std::sin(degToRad(90.0)), 1.0, 1e-12);
    CHECK_NEAR(std::tan(degToRad(45.0)), 1.0, 1e-12);
}

} // namespace

int main()
{
    testConstant();
    testDegToRad();
    testRadToDeg();
    testRoundTrip();
    testConstexpr();
    testAgainstTrigonometry();
    return geotest::summarize("AngleTests");
}
