#include "3dmath.h"
#include "mini3d.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize ("O3")
#endif

#if !defined(MIN)
#define MIN(a, b) (((a)<(b))?(a):(b))
#endif

#if !defined(MAX)
#define MAX(a, b) (((a)>(b))?(a):(b))
#endif

typedef struct Vector2D
{
    float dx;
    float dy;
} Vector2D;

static Point2D project_3d_to_2d_basis(
    Point3D* p,
    Vector3D* bx,
    Vector3D* by
)
{
    return (Point2D){
        .x = Vector3DDot(*(Vector3D*)(void*)p, *bx),
        .y = Vector3DDot(*(Vector3D*)(void*)p, *by),
    };
}

static float sign (Point2D p1, Point2D p2, Point2D p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

static int PointInTriangle (Point2D pt, Point2D v1, Point2D v2, Point2D v3)
{
    float d1, d2, d3;
    int has_neg, has_pos;

    d1 = sign(pt, v1, v2);
    d2 = sign(pt, v2, v3);
    d3 = sign(pt, v3, v1);

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

static int test_circle_point(
    Point2D circle,
    float r,
    Point2D p
)
{
    return (p.x - circle.x) * (p.x - circle.x)
        + (p.y - circle.y) * (p.y - circle.y)
        < r * r;
}

static void normalize_vector2D(Vector2D* v)
{
    float l = fisr(v->dx * v->dx + v->dy * v->dy);
    v->dx = v->dx * l;
    v->dy = v->dy * l;
}

static float vector2DDot(Vector2D a, Vector2D b)
{
    return a.dx * b.dx + a.dy * b.dy;
}

static Vector2D point2D_difference(Point2D a, Point2D b)
{
    return (Vector2D){
        .dx = b.x - a.x,
        .dy = b.y - a.y
    };
}

static float point2D_distance(Point2D a, Point2D b)
{
    return sqrtf((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y-b.y));
}

static int test_circle_line_segment(
    Point2D circle,
    float r,
    Point2D a,
    Point2D b
)
{
    if (test_circle_point(circle, r, a)) return 1;
    if (test_circle_point(circle, r, b)) return 1;
    
    // inverse slope
    Vector2D inverse = {
        .dx = b.y - a.y,
        .dy = a.x - b.x
    };
    normalize_vector2D(&inverse);
    
    Vector2D v = point2D_difference(
        a,
        circle
    );
    
    float line_displacement = vector2DDot(inverse, v);
    
    // check that the circle is close enough to the infinite line
    if (line_displacement < r && line_displacement > -r)
    {
        Vector2D along = point2D_difference(a, b);
        
        // get projection along line
        float line_component = vector2DDot(along, v);
        if (line_component > 0 && line_component < point2D_distance(a, b))
        {
            return 1;
        }
    }
        
    return 0;
}

// return 0 if bounding check cannot rule out collision
static int
test_sphere_triangle_bounding(
    Point3D* sphere,
    float r,
    Point3D* p1,
    Point3D* p2,
    Point3D* p3
)
{
    Point3D min, max;
    min.x = MIN(MIN(p1->x, p2->x), p3->x) - r;
    min.y = MIN(MIN(p1->y, p2->y), p3->y) - r;
    min.z = MIN(MIN(p1->z, p2->z), p3->z) - r;
    max.x = MAX(MAX(p1->x, p2->x), p3->x) + r;
    max.y = MAX(MAX(p1->y, p2->y), p3->y) + r;
    max.z = MAX(MAX(p1->z, p2->z), p3->z) + r;
    
    if (sphere->x <= min.x) return 1;
    if (sphere->y <= min.y) return 1;
    if (sphere->z <= min.z) return 1;
    if (sphere->x >= max.x) return 1;
    if (sphere->y >= max.y) return 1;
    if (sphere->z >= max.z) return 1;
    
    return 0;
}

// return 1 if collision, and sets o_normal
int test_sphere_triangle(
    Point3D* sphere,
    float r,
    Point3D* p1,
    Point3D* p2,
    Point3D* p3,
    Vector3D* o_normal
)
{
    if (Point3D_equal(p1, p2)) return 0;
    if (Point3D_equal(p1, p3)) return 0;
    if (Point3D_equal(p2, p3)) return 0;
    
    Vector3D n = normal(p1, p2, p3);
    Vector3D v = Point3D_difference(sphere, p1);
    float plane_displacement = Vector3DDot(n, v);
    
    if (test_sphere_triangle_bounding(sphere, r, p1, p2, p3)) return 0;
    
    if (plane_displacement < r && plane_displacement > -r)
    {        
        // Sphere intersects the plane.
        // project everything to one plane.
        Vector3D basis_x = Point3D_difference(p1, p2);
        basis_x = Vector3D_normalize(basis_x);
        Vector3D basis_y = Vector3DCross(basis_x, n);
        
        Point2D q1 = project_3d_to_2d_basis(p1, &basis_x, &basis_y);
        Point2D q2 = project_3d_to_2d_basis(p2, &basis_x, &basis_y);
        Point2D q3 = project_3d_to_2d_basis(p3, &basis_x, &basis_y);
        Point2D qs = project_3d_to_2d_basis(sphere, &basis_x, &basis_y);
        
        // center of sphere inside triangle. Definitely collides.
        if (PointInTriangle(qs, q1, q2, q3))
        {
            if (o_normal)
            {
                memcpy(o_normal, &n, sizeof(Vector3D));
            }
            return 1;
        }
        
        // sphere projects to a circle with a smaller diameter
        r = sqrtf(r*r - plane_displacement*plane_displacement);
        
        // check all three line segments
        if (test_circle_line_segment(qs, r, q1, q2))
        {
            
            if (o_normal)
            {
                // XXX
                memcpy(o_normal, &n, sizeof(Vector3D));
            }
            return 1;
        }
        
        if (test_circle_line_segment(qs, r, q2, q3))
        {
            if (o_normal)
            {
                // XXX
                memcpy(o_normal, &n, sizeof(Vector3D));
            }
            return 1;
        }
        
        if (test_circle_line_segment(qs, r, q3, q1))
        {
            if (o_normal)
            {
                // XXX
                memcpy(o_normal, &n, sizeof(Vector3D));
            }
            return 1;
        }
    }
    
    return 0;
}