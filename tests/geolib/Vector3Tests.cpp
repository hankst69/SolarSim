#include "geolib/Vector3.h"

#include "TestSupport.h"

using namespace geo;

namespace {

void testConstruction()
{
    const Vector3 zero;
    CHECK_NEAR(zero.x, 0.0, 1e-12);
    CHECK_NEAR(zero.y, 0.0, 1e-12);
    CHECK_NEAR(zero.z, 0.0, 1e-12);

    const Vector3 v(1.0, 2.0, 3.0);
    CHECK_NEAR(v.x, 1.0, 1e-12);
    CHECK_NEAR(v.y, 2.0, 1e-12);
    CHECK_NEAR(v.z, 3.0, 1e-12);
}

void testArithmetic()
{
    const Vector3 a(1.0, 2.0, 3.0);
    const Vector3 b(4.0, 5.0, 6.0);

    const Vector3 sum = a + b;
    CHECK_NEAR(sum.x, 5.0, 1e-12);
    CHECK_NEAR(sum.y, 7.0, 1e-12);
    CHECK_NEAR(sum.z, 9.0, 1e-12);

    const Vector3 diff = b - a;
    CHECK_NEAR(diff.x, 3.0, 1e-12);
    CHECK_NEAR(diff.y, 3.0, 1e-12);
    CHECK_NEAR(diff.z, 3.0, 1e-12);

    const Vector3 scaled = a * 2.0;
    CHECK_NEAR(scaled.x, 2.0, 1e-12);
    CHECK_NEAR(scaled.z, 6.0, 1e-12);

    const Vector3 divided = b / 2.0;
    CHECK_NEAR(divided.x, 2.0, 1e-12);
    CHECK_NEAR(divided.z, 3.0, 1e-12);

    // Scalar multiplication is commutative.
    const Vector3 preScaled = 3.0 * a;
    CHECK_NEAR(preScaled.x, 3.0, 1e-12);
    CHECK_NEAR(preScaled.y, 6.0, 1e-12);
    CHECK_NEAR(preScaled.z, 9.0, 1e-12);
}

void testDot()
{
    const Vector3 a(1.0, 2.0, 3.0);
    const Vector3 b(4.0, -5.0, 6.0);
    CHECK_NEAR(a.dot(b), 4.0 - 10.0 + 18.0, 1e-12);

    // Orthogonal vectors have a zero dot product.
    CHECK_NEAR(Vector3(1.0, 0.0, 0.0).dot(Vector3(0.0, 1.0, 0.0)), 0.0, 1e-12);
}

void testCross()
{
    const Vector3 x(1.0, 0.0, 0.0);
    const Vector3 y(0.0, 1.0, 0.0);

    // Right handed frame: x cross y = z.
    const Vector3 z = x.cross(y);
    CHECK_NEAR(z.x, 0.0, 1e-12);
    CHECK_NEAR(z.y, 0.0, 1e-12);
    CHECK_NEAR(z.z, 1.0, 1e-12);

    // Anti commutative.
    const Vector3 minusZ = y.cross(x);
    CHECK_NEAR(minusZ.z, -1.0, 1e-12);

    // A vector crossed with itself vanishes.
    const Vector3 self = Vector3(1.0, 2.0, 3.0).cross(Vector3(1.0, 2.0, 3.0));
    CHECK_NEAR(self.length(), 0.0, 1e-12);

    // The result is orthogonal to both inputs.
    const Vector3 a(1.0, 2.0, 3.0);
    const Vector3 b(-2.0, 0.5, 4.0);
    const Vector3 c = a.cross(b);
    CHECK_NEAR(c.dot(a), 0.0, 1e-9);
    CHECK_NEAR(c.dot(b), 0.0, 1e-9);
}

void testLengthAndNormalized()
{
    CHECK_NEAR(Vector3(3.0, 4.0, 0.0).length(), 5.0, 1e-12);
    CHECK_NEAR(Vector3(1.0, 2.0, 2.0).length(), 3.0, 1e-12);

    const Vector3 unit = Vector3(0.0, 0.0, 5.0).normalized();
    CHECK_NEAR(unit.length(), 1.0, 1e-12);
    CHECK_NEAR(unit.z, 1.0, 1e-12);

    const Vector3 n = Vector3(1.0, 2.0, 3.0).normalized();
    CHECK_NEAR(n.length(), 1.0, 1e-12);

    // Normalizing the zero vector must not divide by zero.
    const Vector3 zero = Vector3().normalized();
    CHECK_NEAR(zero.length(), 0.0, 1e-12);
}

} // namespace

int main()
{
    testConstruction();
    testArithmetic();
    testDot();
    testCross();
    testLengthAndNormalized();
    return geotest::summarize("Vector3Tests");
}
