#pragma once

#include "geolib/Vector3.h"

#include <cstddef>
#include <string>
#include <vector>

namespace geo {

/// Indexed triangle mesh in the local east/north/up frame of a GroundPlane
/// (metres). Used for the ground topology as well as for building models.
class TriangleMesh {
public:
    struct Triangle {
        std::size_t a{0};
        std::size_t b{0};
        std::size_t c{0};
    };

    const std::vector<Vector3>& vertices() const { return m_vertices; }
    const std::vector<Triangle>& triangles() const { return m_triangles; }

    std::size_t addVertex(const Vector3& v);
    void addTriangle(std::size_t a, std::size_t b, std::size_t c);

    void reserve(std::size_t vertexCount, std::size_t triangleCount);
    void clear();
    bool empty() const { return m_triangles.empty(); }

    /// Append another mesh, optionally translated by the given offset.
    void append(const TriangleMesh& other, const Vector3& offset = Vector3{});

    /// Axis aligned bounding box; returns false for an empty mesh.
    bool bounds(Vector3& minCorner, Vector3& maxCorner) const;

    /// Nearest intersection of the ray origin + t * direction (t > minDistance)
    /// with the mesh. Writes the hit distance to distance if given.
    bool intersect(const Vector3& origin, const Vector3& direction,
                   double minDistance = 1e-3, double maxDistance = 1e12,
                   double* distance = nullptr) const;

    /// True if any triangle blocks the ray, i.e. a shadow caster is hit.
    bool isOccluded(const Vector3& origin, const Vector3& direction,
                    double minDistance = 1e-3, double maxDistance = 1e12) const
    {
        return intersect(origin, direction, minDistance, maxDistance, nullptr);
    }

    /// Simple axis aligned box, e.g. as a placeholder building model.
    static TriangleMesh createBox(const Vector3& minCorner, const Vector3& maxCorner);

private:
    std::vector<Vector3> m_vertices;
    std::vector<Triangle> m_triangles;
};

} // namespace geo
