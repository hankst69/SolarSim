#include "geolib/TriangleMesh.h"

#include <cmath>
#include <limits>

namespace geo {
namespace {

/// Moeller-Trumbore ray/triangle intersection.
bool intersectTriangle(const Vector3& origin, const Vector3& direction, const Vector3& v0,
                       const Vector3& v1, const Vector3& v2, double& distance)
{
    constexpr double kEpsilon = 1e-12;
    const Vector3 edge1 = v1 - v0;
    const Vector3 edge2 = v2 - v0;
    const Vector3 pvec = direction.cross(edge2);
    const double det = edge1.dot(pvec);
    if (std::fabs(det) < kEpsilon) {
        return false;
    }
    const double invDet = 1.0 / det;
    const Vector3 tvec = origin - v0;
    const double u = tvec.dot(pvec) * invDet;
    if (u < 0.0 || u > 1.0) {
        return false;
    }
    const Vector3 qvec = tvec.cross(edge1);
    const double v = direction.dot(qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }
    distance = edge2.dot(qvec) * invDet;
    return true;
}

} // namespace

std::size_t TriangleMesh::addVertex(const Vector3& v)
{
    m_vertices.push_back(v);
    return m_vertices.size() - 1;
}

void TriangleMesh::addTriangle(std::size_t a, std::size_t b, std::size_t c)
{
    m_triangles.push_back(Triangle{a, b, c});
}

void TriangleMesh::reserve(std::size_t vertexCount, std::size_t triangleCount)
{
    m_vertices.reserve(vertexCount);
    m_triangles.reserve(triangleCount);
}

void TriangleMesh::clear()
{
    m_vertices.clear();
    m_triangles.clear();
}

void TriangleMesh::append(const TriangleMesh& other, const Vector3& offset)
{
    const std::size_t base = m_vertices.size();
    m_vertices.reserve(base + other.m_vertices.size());
    for (const auto& v : other.m_vertices) {
        m_vertices.push_back(v + offset);
    }
    m_triangles.reserve(m_triangles.size() + other.m_triangles.size());
    for (const auto& t : other.m_triangles) {
        m_triangles.push_back(Triangle{t.a + base, t.b + base, t.c + base});
    }
}

bool TriangleMesh::bounds(Vector3& minCorner, Vector3& maxCorner) const
{
    if (m_vertices.empty()) {
        return false;
    }
    minCorner = m_vertices.front();
    maxCorner = m_vertices.front();
    for (const auto& v : m_vertices) {
        minCorner.x = std::fmin(minCorner.x, v.x);
        minCorner.y = std::fmin(minCorner.y, v.y);
        minCorner.z = std::fmin(minCorner.z, v.z);
        maxCorner.x = std::fmax(maxCorner.x, v.x);
        maxCorner.y = std::fmax(maxCorner.y, v.y);
        maxCorner.z = std::fmax(maxCorner.z, v.z);
    }
    return true;
}

bool TriangleMesh::intersect(const Vector3& origin, const Vector3& direction,
                             double minDistance, double maxDistance, double* distance) const
{
    const Vector3 dir = direction.normalized();
    double best = std::numeric_limits<double>::max();
    bool hit = false;
    for (const auto& t : m_triangles) {
        double d = 0.0;
        if (intersectTriangle(origin, dir, m_vertices[t.a], m_vertices[t.b], m_vertices[t.c], d) &&
            d > minDistance && d < maxDistance && d < best) {
            best = d;
            hit = true;
            if (distance == nullptr) {
                break;
            }
        }
    }
    if (hit && distance != nullptr) {
        *distance = best;
    }
    return hit;
}

TriangleMesh TriangleMesh::createBox(const Vector3& minCorner, const Vector3& maxCorner)
{
    TriangleMesh mesh;
    mesh.reserve(8, 12);
    const double x0 = minCorner.x;
    const double y0 = minCorner.y;
    const double z0 = minCorner.z;
    const double x1 = maxCorner.x;
    const double y1 = maxCorner.y;
    const double z1 = maxCorner.z;

    mesh.addVertex({x0, y0, z0});
    mesh.addVertex({x1, y0, z0});
    mesh.addVertex({x1, y1, z0});
    mesh.addVertex({x0, y1, z0});
    mesh.addVertex({x0, y0, z1});
    mesh.addVertex({x1, y0, z1});
    mesh.addVertex({x1, y1, z1});
    mesh.addVertex({x0, y1, z1});

    const int faces[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
                              {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
                              {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
    for (const auto& f : faces) {
        mesh.addTriangle(static_cast<std::size_t>(f[0]), static_cast<std::size_t>(f[1]),
                         static_cast<std::size_t>(f[2]));
    }
    return mesh;
}

} // namespace geo
