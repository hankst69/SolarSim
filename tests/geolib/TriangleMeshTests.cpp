#include "geolib/TriangleMesh.h"

#include "TestSupport.h"

#include <cmath>

using namespace geo;

namespace {

/// A unit square in the z = 0 plane spanning [0,1] x [0,1].
TriangleMesh makeQuad(double height = 0.0)
{
    TriangleMesh mesh;
    const std::size_t a = mesh.addVertex({0.0, 0.0, height});
    const std::size_t b = mesh.addVertex({1.0, 0.0, height});
    const std::size_t c = mesh.addVertex({1.0, 1.0, height});
    const std::size_t d = mesh.addVertex({0.0, 1.0, height});
    mesh.addTriangle(a, b, c);
    mesh.addTriangle(a, c, d);
    return mesh;
}

void testEmptyMesh()
{
    const TriangleMesh mesh;
    CHECK_TRUE(mesh.empty());
    CHECK_EQ_INT(static_cast<long long>(mesh.vertices().size()), 0);
    CHECK_EQ_INT(static_cast<long long>(mesh.triangles().size()), 0);

    Vector3 lo;
    Vector3 hi;
    CHECK_FALSE(mesh.bounds(lo, hi));

    // Nothing can be hit.
    CHECK_FALSE(mesh.isOccluded({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}));
}

void testAddVertexAndTriangle()
{
    TriangleMesh mesh;
    CHECK_EQ_INT(static_cast<long long>(mesh.addVertex({1.0, 2.0, 3.0})), 0);
    CHECK_EQ_INT(static_cast<long long>(mesh.addVertex({4.0, 5.0, 6.0})), 1);
    CHECK_EQ_INT(static_cast<long long>(mesh.addVertex({7.0, 8.0, 9.0})), 2);
    CHECK_TRUE(mesh.empty()); // no triangles yet

    mesh.addTriangle(0, 1, 2);
    CHECK_FALSE(mesh.empty());
    CHECK_EQ_INT(static_cast<long long>(mesh.triangles().size()), 1);
    CHECK_EQ_INT(static_cast<long long>(mesh.triangles()[0].a), 0);
    CHECK_EQ_INT(static_cast<long long>(mesh.triangles()[0].c), 2);
}

void testClear()
{
    TriangleMesh mesh = makeQuad();
    CHECK_FALSE(mesh.empty());
    mesh.clear();
    CHECK_TRUE(mesh.empty());
    CHECK_EQ_INT(static_cast<long long>(mesh.vertices().size()), 0);
}

void testBounds()
{
    const TriangleMesh mesh = makeQuad(2.0);
    Vector3 lo;
    Vector3 hi;
    CHECK_TRUE(mesh.bounds(lo, hi));
    CHECK_NEAR(lo.x, 0.0, 1e-12);
    CHECK_NEAR(lo.y, 0.0, 1e-12);
    CHECK_NEAR(lo.z, 2.0, 1e-12);
    CHECK_NEAR(hi.x, 1.0, 1e-12);
    CHECK_NEAR(hi.y, 1.0, 1e-12);
    CHECK_NEAR(hi.z, 2.0, 1e-12);
}

void testAppend()
{
    TriangleMesh mesh = makeQuad();
    const TriangleMesh other = makeQuad();

    mesh.append(other, Vector3{10.0, 0.0, 0.0});
    CHECK_EQ_INT(static_cast<long long>(mesh.vertices().size()), 8);
    CHECK_EQ_INT(static_cast<long long>(mesh.triangles().size()), 4);

    // The appended triangles must reference the new vertices, not the old ones.
    CHECK_EQ_INT(static_cast<long long>(mesh.triangles()[2].a), 4);

    Vector3 lo;
    Vector3 hi;
    CHECK_TRUE(mesh.bounds(lo, hi));
    CHECK_NEAR(lo.x, 0.0, 1e-12);
    CHECK_NEAR(hi.x, 11.0, 1e-12);
}

void testAppendEmpty()
{
    TriangleMesh mesh = makeQuad();
    mesh.append(TriangleMesh{});
    CHECK_EQ_INT(static_cast<long long>(mesh.triangles().size()), 2);
}

void testCreateBox()
{
    const TriangleMesh box = TriangleMesh::createBox({-1.0, -2.0, 0.0}, {1.0, 2.0, 3.0});
    CHECK_EQ_INT(static_cast<long long>(box.vertices().size()), 8);
    CHECK_EQ_INT(static_cast<long long>(box.triangles().size()), 12);

    Vector3 lo;
    Vector3 hi;
    CHECK_TRUE(box.bounds(lo, hi));
    CHECK_NEAR(lo.x, -1.0, 1e-12);
    CHECK_NEAR(lo.y, -2.0, 1e-12);
    CHECK_NEAR(lo.z, 0.0, 1e-12);
    CHECK_NEAR(hi.x, 1.0, 1e-12);
    CHECK_NEAR(hi.y, 2.0, 1e-12);
    CHECK_NEAR(hi.z, 3.0, 1e-12);
}

void testIntersectHit()
{
    const TriangleMesh quad = makeQuad(5.0);

    // Straight up through the middle of the quad.
    double distance = 0.0;
    CHECK_TRUE(quad.intersect({0.5, 0.5, 0.0}, {0.0, 0.0, 1.0}, 1e-3, 1e12, &distance));
    CHECK_NEAR(distance, 5.0, 1e-9);

    // The direction does not have to be normalized.
    CHECK_TRUE(quad.intersect({0.5, 0.5, 0.0}, {0.0, 0.0, 7.0}, 1e-3, 1e12, &distance));
    CHECK_NEAR(distance, 5.0, 1e-9);
}

void testIntersectMiss()
{
    const TriangleMesh quad = makeQuad(5.0);

    // Beside the quad.
    CHECK_FALSE(quad.isOccluded({5.0, 5.0, 0.0}, {0.0, 0.0, 1.0}));
    // Pointing away from it.
    CHECK_FALSE(quad.isOccluded({0.5, 0.5, 0.0}, {0.0, 0.0, -1.0}));
    // Parallel to the quad plane.
    CHECK_FALSE(quad.isOccluded({0.5, 0.5, 0.0}, {1.0, 0.0, 0.0}));
}

void testIntersectDistanceLimits()
{
    const TriangleMesh quad = makeQuad(5.0);

    // maxDistance cuts the hit off.
    CHECK_FALSE(quad.isOccluded({0.5, 0.5, 0.0}, {0.0, 0.0, 1.0}, 1e-3, 4.0));
    CHECK_TRUE(quad.isOccluded({0.5, 0.5, 0.0}, {0.0, 0.0, 1.0}, 1e-3, 6.0));

    // minDistance skips a hit that is too close, which is how self
    // intersection at the ray origin is avoided.
    CHECK_FALSE(quad.isOccluded({0.5, 0.5, 0.0}, {0.0, 0.0, 1.0}, 6.0, 1e12));
}

/// The nearest of several hits must be reported.
void testIntersectReturnsNearest()
{
    TriangleMesh mesh = makeQuad(10.0);
    mesh.append(makeQuad(3.0));
    mesh.append(makeQuad(7.0));

    double distance = 0.0;
    CHECK_TRUE(mesh.intersect({0.5, 0.5, 0.0}, {0.0, 0.0, 1.0}, 1e-3, 1e12, &distance));
    CHECK_NEAR(distance, 3.0, 1e-9);
}

void testIntersectEdgeCases()
{
    const TriangleMesh quad = makeQuad(1.0);

    // Just inside the quad near a corner.
    CHECK_TRUE(quad.isOccluded({0.01, 0.01, 0.0}, {0.0, 0.0, 1.0}));
    // Just outside it.
    CHECK_FALSE(quad.isOccluded({-0.01, -0.01, 0.0}, {0.0, 0.0, 1.0}));
    // Along the shared diagonal of the two triangles.
    CHECK_TRUE(quad.isOccluded({0.5, 0.5, 0.0}, {0.0, 0.0, 1.0}));
}

void testIntersectBoxFromInsideAndOutside()
{
    const TriangleMesh box = TriangleMesh::createBox({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});

    // From outside, the near face is hit first.
    double distance = 0.0;
    CHECK_TRUE(box.intersect({0.0, 0.0, -5.0}, {0.0, 0.0, 1.0}, 1e-3, 1e12, &distance));
    CHECK_NEAR(distance, 4.0, 1e-9);

    // From the centre, the far face is hit.
    CHECK_TRUE(box.intersect({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 1e-3, 1e12, &distance));
    CHECK_NEAR(distance, 1.0, 1e-9);

    // A ray missing the box entirely.
    CHECK_FALSE(box.isOccluded({5.0, 5.0, -5.0}, {0.0, 0.0, 1.0}));
}

/// A slanted ray, as used for low sun elevations.
void testIntersectSlantedRay()
{
    const TriangleMesh wall = TriangleMesh::createBox({2.0, -5.0, 0.0}, {3.0, 5.0, 10.0});

    // 45 degrees upward towards the wall: hits it.
    CHECK_TRUE(wall.isOccluded({0.0, 0.0, 0.0}, Vector3(1.0, 0.0, 1.0).normalized()));

    // Steep enough to pass over the wall.
    CHECK_FALSE(wall.isOccluded({0.0, 0.0, 9.5}, Vector3(1.0, 0.0, 1.0).normalized()));

    // Pointing away from the wall.
    CHECK_FALSE(wall.isOccluded({0.0, 0.0, 0.0}, Vector3(-1.0, 0.0, 1.0).normalized()));
}

void testReserveDoesNotChangeContent()
{
    TriangleMesh mesh;
    mesh.reserve(100, 200);
    CHECK_TRUE(mesh.empty());
    CHECK_EQ_INT(static_cast<long long>(mesh.vertices().size()), 0);

    mesh.addVertex({1.0, 1.0, 1.0});
    CHECK_EQ_INT(static_cast<long long>(mesh.vertices().size()), 1);
}

} // namespace

int main()
{
    testEmptyMesh();
    testAddVertexAndTriangle();
    testClear();
    testBounds();
    testAppend();
    testAppendEmpty();
    testCreateBox();
    testIntersectHit();
    testIntersectMiss();
    testIntersectDistanceLimits();
    testIntersectReturnsNearest();
    testIntersectEdgeCases();
    testIntersectBoxFromInsideAndOutside();
    testIntersectSlantedRay();
    testReserveDoesNotChangeContent();
    return geotest::summarize("TriangleMeshTests");
}
