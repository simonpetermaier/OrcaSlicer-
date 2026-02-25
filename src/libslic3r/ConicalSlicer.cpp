#include "ConicalSlicer.hpp"
#include "ClipperUtils.hpp"
#include "Geometry.hpp"
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace Slic3r {

// ============================================================================
// ConeSurface Implementation
// ============================================================================

ConeSurface::ConeSurface(const Vec3d &apex, const Vec3d &axis, double half_angle_deg)
    : apex(apex)
    , axis(axis.normalized())
    , half_angle(half_angle_deg * M_PI / 180.0)
    , tan_angle(std::tan(this->half_angle))
{
}

double ConeSurface::signed_distance(const Vec3d &point) const
{
    // Vector from apex to point
    Vec3d v = point - apex;

    // Project onto cone axis
    double v_axis = v.dot(axis);

    // If above apex (positive axis direction), distance to apex
    if (v_axis > 0)
        return v.norm();

    // Radial distance from axis
    Vec3d v_perp = v - v_axis * axis;
    double r_actual = v_perp.norm();

    // Expected radial distance on cone surface at this axial position
    double r_cone = -v_axis * tan_angle;

    // Signed distance: positive = outside cone, negative = inside
    return r_actual - r_cone;
}

ConeSurface::Classification ConeSurface::classify(const Vec3d &point, double tolerance) const
{
    double dist = signed_distance(point);
    if (std::abs(dist) < tolerance)
        return Classification::OnSurface;
    return (dist > 0) ? Classification::Outside : Classification::Inside;
}

// ============================================================================
// ConicalSlicer Implementation
// ============================================================================

ConicalSlicer::ConicalSlicer(const indexed_triangle_set &mesh,
                             const BoundingBoxf3 &bbox,
                             double cone_angle_deg,
                             double layer_height)
    : m_mesh(mesh)
    , m_bbox(bbox)
    , m_cone_angle_deg(cone_angle_deg)
    , m_layer_height(layer_height)
{
    // Compute cone apex (centered horizontally, above object)
    m_cone_apex = compute_cone_apex(bbox);

    // Cone axis points downward (-Z)
    m_cone_axis = Vec3d(0, 0, -1);
}

Vec3d ConicalSlicer::compute_cone_apex(const BoundingBoxf3 &bbox)
{
    // Center cone horizontally on object bounds
    Vec3d center = bbox.center();

    // Place apex at top of object + small margin
    double apex_z = bbox.max.z() + 0.1;  // 0.1mm above object

    return Vec3d(center.x(), center.y(), apex_z);
}

std::vector<ConeSurface> ConicalSlicer::generate_cone_layers() const
{
    std::vector<ConeSurface> cones;

    // Determine number of layers based on object height
    double object_height = m_bbox.max.z() - m_bbox.min.z();
    int n_layers = std::max(1, static_cast<int>(std::ceil(object_height / m_layer_height)));

    // Generate cone surfaces, evenly spaced from bottom to top
    for (int i = 0; i < n_layers; ++i) {
        // Z-height for this layer (measured at cone apex)
        // Each successive cone has same apex but different "effective" radius at object height
        // For simplicity, we'll space them by layer_height along the axis
        double z_offset = m_layer_height * (i + 1);

        // Create cone with same angle, but apex shifted down to represent the layer
        // Actually, for true conical slicing, we keep the same apex and vary the cone by
        // intersecting at different z-heights. But to maintain separate ConeSurface objects,
        // we can think of each as a "cutting height" on the same cone.

        // Alternative approach: Create cones with same apex and angle, but we'll slice
        // at different effective radii by storing different apex z-values
        Vec3d layer_apex = m_cone_apex;
        layer_apex.z() = m_bbox.min.z() + z_offset;

        cones.emplace_back(m_cone_apex, m_cone_axis, m_cone_angle_deg);
    }

    return cones;
}

bool ConicalSlicer::intersect_edge_cone(
    const ConeSurface &cone,
    const Vec3f &v0, const Vec3f &v1,
    int idx0, int idx1,
    Vec3d &intersection_point) const
{
    Vec3d p0(v0.cast<double>());
    Vec3d p1(v1.cast<double>());

    // Classify endpoints
    auto class0 = cone.classify(p0);
    auto class1 = cone.classify(p1);

    // If both on same side, no intersection (unless both on surface)
    if (class0 == class1 && class0 != ConeSurface::Classification::OnSurface)
        return false;

    // If both on surface, degenerate case - return start point
    if (class0 == ConeSurface::Classification::OnSurface) {
        intersection_point = p0;
        return true;
    }
    if (class1 == ConeSurface::Classification::OnSurface) {
        intersection_point = p1;
        return true;
    }

    // Binary search for intersection point
    // Edge parametrized as: p(t) = p0 + t*(p1-p0), t in [0,1]
    // Find t where signed_distance(p(t)) = 0

    double t_min = 0.0, t_max = 1.0;
    const int max_iterations = 20;
    const double tolerance = 1e-6;

    for (int iter = 0; iter < max_iterations; ++iter) {
        double t_mid = (t_min + t_max) / 2.0;
        Vec3d p_mid = p0 + t_mid * (p1 - p0);
        double dist = cone.signed_distance(p_mid);

        if (std::abs(dist) < tolerance) {
            intersection_point = p_mid;
            return true;
        }

        double dist_min = cone.signed_distance(p0 + t_min * (p1 - p0));
        // Binary search: move interval toward zero crossing
        if ((dist > 0) == (dist_min > 0))
            t_min = t_mid;
        else
            t_max = t_mid;
    }

    // Use final midpoint as approximation
    intersection_point = p0 + ((t_min + t_max) / 2.0) * (p1 - p0);
    return true;
}

bool ConicalSlicer::intersect_triangle_cone(
    const ConeSurface &cone,
    const Vec3f &v0, const Vec3f &v1, const Vec3f &v2,
    int idx0, int idx1, int idx2,
    std::vector<Intersection3DLine> &lines) const
{
    Vec3d p0(v0.cast<double>());
    Vec3d p1(v1.cast<double>());
    Vec3d p2(v2.cast<double>());

    // Classify all vertices
    auto c0 = cone.classify(p0);
    auto c1 = cone.classify(p1);
    auto c2 = cone.classify(p2);

    // Collect intersection points
    std::vector<Vec3d> intersections;
    std::vector<int> vertex_ids;

    // Check each edge for intersection
    Vec3d isect;
    if (intersect_edge_cone(cone, v0, v1, idx0, idx1, isect)) {
        intersections.push_back(isect);
        vertex_ids.push_back(-1);  // Not a mesh vertex
    }
    if (intersect_edge_cone(cone, v1, v2, idx1, idx2, isect)) {
        intersections.push_back(isect);
        vertex_ids.push_back(-1);
    }
    if (intersect_edge_cone(cone, v2, v0, idx2, idx0, isect)) {
        intersections.push_back(isect);
        vertex_ids.push_back(-1);
    }

    // Also add vertices that lie exactly on the surface
    if (c0 == ConeSurface::Classification::OnSurface) {
        intersections.push_back(p0);
        vertex_ids.push_back(idx0);
    }
    if (c1 == ConeSurface::Classification::OnSurface) {
        intersections.push_back(p1);
        vertex_ids.push_back(idx1);
    }
    if (c2 == ConeSurface::Classification::OnSurface) {
        intersections.push_back(p2);
        vertex_ids.push_back(idx2);
    }

    // Must have exactly 2 intersection points to form a line
    if (intersections.size() != 2)
        return false;

    Intersection3DLine line;
    line.a = intersections[0];
    line.b = intersections[1];
    line.a_id = vertex_ids[0];
    line.b_id = vertex_ids[1];
    line.edge_a_id = -1;
    line.edge_b_id = -1;

    lines.push_back(line);
    return true;
}

Polygons ConicalSlicer::make_loops(const std::vector<Intersection3DLine> &lines) const
{
    if (lines.empty())
        return Polygons();

    // Build adjacency map: point -> lines containing that point
    // Use 2D coordinates for matching (XY projection)
    struct PointHash {
        size_t operator()(const Vec2d &p) const {
            // Snap to grid for robust matching (1 micron precision)
            int64_t x = int64_t(std::round(p.x() * 1e3));
            int64_t y = int64_t(std::round(p.y() * 1e3));
            return std::hash<int64_t>()(x) ^ (std::hash<int64_t>()(y) << 1);
        }
    };

    struct PointEqual {
        bool operator()(const Vec2d &a, const Vec2d &b) const {
            return (a - b).norm() < 1e-3;  // 1 micron tolerance
        }
    };

    std::unordered_multimap<Vec2d, size_t, PointHash, PointEqual> point_to_lines;

    for (size_t i = 0; i < lines.size(); ++i) {
        point_to_lines.emplace(lines[i].a_2d(), i);
        point_to_lines.emplace(lines[i].b_2d(), i);
    }

    // Chain segments into closed loops
    std::vector<bool> used(lines.size(), false);
    Polygons result;

    for (size_t start_idx = 0; start_idx < lines.size(); ++start_idx) {
        if (used[start_idx])
            continue;

        Polygon loop;
        size_t current_idx = start_idx;
        Vec2d current_point = lines[current_idx].a_2d();
        loop.points.push_back(Point(scale_(current_point.x()), scale_(current_point.y())));

        used[current_idx] = true;
        current_point = lines[current_idx].b_2d();

        // Follow chain until we return to start
        const size_t max_iterations = lines.size() * 2;
        size_t iterations = 0;

        while (iterations++ < max_iterations) {
            loop.points.push_back(Point(scale_(current_point.x()), scale_(current_point.y())));

            // Find next segment sharing current_point
            auto range = point_to_lines.equal_range(current_point);
            bool found = false;

            for (auto it = range.first; it != range.second; ++it) {
                size_t next_idx = it->second;
                if (used[next_idx])
                    continue;

                const auto &next_line = lines[next_idx];
                Vec2d next_point;

                if (PointEqual()(next_line.a_2d(), current_point))
                    next_point = next_line.b_2d();
                else if (PointEqual()(next_line.b_2d(), current_point))
                    next_point = next_line.a_2d();
                else
                    continue;

                used[next_idx] = true;
                current_point = next_point;
                found = true;
                break;
            }

            if (!found)
                break;  // Open contour - shouldn't happen for closed meshes

            // Check if we've closed the loop
            Vec2d start_point = lines[start_idx].a_2d();
            if (PointEqual()(current_point, start_point)) {
                result.push_back(std::move(loop));
                break;
            }
        }
    }

    return result;
}

std::vector<Polygons> ConicalSlicer::slice_mesh(
    const std::vector<ConeSurface> &cones,
    std::function<void()> throw_on_cancel) const
{
    std::vector<Polygons> all_slices;
    all_slices.reserve(cones.size());

    // Process each cone surface
    for (size_t cone_idx = 0; cone_idx < cones.size(); ++cone_idx) {
        throw_on_cancel();

        const ConeSurface &cone = cones[cone_idx];
        std::vector<Intersection3DLine> lines;

        // Intersect all triangles with this cone
        for (size_t tri_idx = 0; tri_idx < m_mesh.indices.size(); ++tri_idx) {
            const auto &tri = m_mesh.indices[tri_idx];

            intersect_triangle_cone(
                cone,
                m_mesh.vertices[tri[0]],
                m_mesh.vertices[tri[1]],
                m_mesh.vertices[tri[2]],
                tri[0], tri[1], tri[2],
                lines
            );
        }

        // Chain intersection segments into closed loops
        Polygons slice = make_loops(lines);
        all_slices.push_back(std::move(slice));
    }

    return all_slices;
}

std::vector<ExPolygons> ConicalSlicer::polygons_to_expolygons(const std::vector<Polygons> &slices) const
{
    std::vector<ExPolygons> result;
    result.reserve(slices.size());

    for (const Polygons &slice : slices) {
        // Convert to ExPolygons (detect holes and winding)
        ExPolygons expolys = union_ex(slice);
        result.push_back(std::move(expolys));
    }

    return result;
}

} // namespace Slic3r
