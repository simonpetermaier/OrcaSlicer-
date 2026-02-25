#ifndef slic3r_ConicalSlicer_hpp_
#define slic3r_ConicalSlicer_hpp_

#include "libslic3r.h"
#include "TriangleMesh.hpp"
#include "Polygon.hpp"
#include "ExPolygon.hpp"
#include "Point.hpp"
#include <vector>
#include <functional>

namespace Slic3r {

/**
 * @brief Represents a conical slicing surface
 *
 * A cone is defined by its apex (top point), axis direction, and half-angle.
 * The cone surface equation: x² + y² = [(z_apex - z) / tan(α)]²
 * where α is the half-angle from the vertical axis.
 */
struct ConeSurface {
    Vec3d apex;           // Apex position (top of cone)
    Vec3d axis;           // Unit vector pointing from apex downward (usually -Z)
    double half_angle;    // Half-angle from axis in radians
    double tan_angle;     // Precomputed tan(half_angle) for performance

    /**
     * @brief Construct a cone surface
     * @param apex Apex position in 3D space
     * @param axis Cone axis direction (will be normalized)
     * @param half_angle_deg Half-angle in degrees (0-90)
     */
    ConeSurface(const Vec3d &apex, const Vec3d &axis, double half_angle_deg);

    /**
     * @brief Compute signed distance from a point to the cone surface
     * @param point 3D point to test
     * @return Signed distance: negative=inside, positive=outside, 0=on surface
     */
    double signed_distance(const Vec3d &point) const;

    /**
     * @brief Classify a point relative to the cone surface
     */
    enum class Classification { Inside, OnSurface, Outside };
    Classification classify(const Vec3d &point, double tolerance = 1e-6) const;
};

/**
 * @brief Stores a 3D intersection line segment from cone-triangle intersection
 *
 * Similar to IntersectionLine in TriangleMeshSlicer, but preserves 3D coordinates.
 */
struct Intersection3DLine {
    Vec3d a, b;           // Start and end points in 3D
    int a_id, b_id;       // Vertex IDs if endpoints are mesh vertices, -1 otherwise
    int edge_a_id, edge_b_id;  // Edge IDs for topology tracking

    // Convert to 2D for polygon operations (project onto XY plane)
    Vec2d a_2d() const { return Vec2d(a.x(), a.y()); }
    Vec2d b_2d() const { return Vec2d(b.x(), b.y()); }
};

/**
 * @brief Main conical slicer class
 *
 * Slices a triangle mesh using conical surfaces instead of planar surfaces.
 * Implements cone-triangle intersection and loop construction algorithms.
 */
class ConicalSlicer {
public:
    /**
     * @brief Construct a conical slicer
     * @param mesh Triangle mesh to slice (indexed_triangle_set)
     * @param bbox Bounding box of the object
     * @param cone_angle_deg Cone half-angle in degrees (0-90)
     * @param layer_height Target layer height for spacing cone surfaces
     */
    ConicalSlicer(const indexed_triangle_set &mesh,
                  const BoundingBoxf3 &bbox,
                  double cone_angle_deg,
                  double layer_height);

    /**
     * @brief Generate cone surfaces for all layers
     * @return Vector of ConeSurface objects, one per layer
     */
    std::vector<ConeSurface> generate_cone_layers() const;

    /**
     * @brief Slice mesh with multiple cone surfaces
     * @param cones Vector of cone surfaces to slice with
     * @param throw_on_cancel Callback to check for cancellation
     * @return Vector of Polygons (one per cone surface)
     */
    std::vector<Polygons> slice_mesh(const std::vector<ConeSurface> &cones,
                                     std::function<void()> throw_on_cancel = []{}) const;

    /**
     * @brief Convert Polygons to ExPolygons (handle winding, holes)
     * @param slices Vector of polygon slices
     * @return Vector of ExPolygons with holes properly detected
     */
    std::vector<ExPolygons> polygons_to_expolygons(const std::vector<Polygons> &slices) const;

private:
    const indexed_triangle_set &m_mesh;
    BoundingBoxf3 m_bbox;
    double m_cone_angle_deg;
    double m_layer_height;

    // Geometry parameters computed from bbox
    Vec3d m_cone_apex;
    Vec3d m_cone_axis;

    /**
     * @brief Intersect a single triangle with a cone surface
     * @param cone Cone surface to intersect with
     * @param v0, v1, v2 Triangle vertices
     * @param idx0, idx1, idx2 Vertex indices in mesh
     * @param lines Output: intersection line segments
     * @return true if intersection found
     */
    bool intersect_triangle_cone(const ConeSurface &cone,
                                 const Vec3f &v0, const Vec3f &v1, const Vec3f &v2,
                                 int idx0, int idx1, int idx2,
                                 std::vector<Intersection3DLine> &lines) const;

    /**
     * @brief Intersect an edge with a cone surface
     * @param cone Cone surface to intersect with
     * @param v0, v1 Edge endpoints
     * @param idx0, idx1 Vertex indices
     * @param intersection_point Output: 3D intersection point
     * @return true if intersection found
     */
    bool intersect_edge_cone(const ConeSurface &cone,
                            const Vec3f &v0, const Vec3f &v1,
                            int idx0, int idx1,
                            Vec3d &intersection_point) const;

    /**
     * @brief Chain intersection segments into closed loops
     * @param lines Vector of 3D intersection line segments
     * @return Polygons (2D projection of closed loops)
     */
    Polygons make_loops(const std::vector<Intersection3DLine> &lines) const;

    /**
     * @brief Compute cone apex from object bounding box
     * @param bbox Object bounding box
     * @return Apex position (centered horizontally, above object)
     */
    static Vec3d compute_cone_apex(const BoundingBoxf3 &bbox);
};

} // namespace Slic3r

#endif // slic3r_ConicalSlicer_hpp_
