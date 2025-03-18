#pragma once

#include <array>
#include <memory>
#include <vector>
#include <variant>

#include "Vector2D.h"
#include "Affine2D.h"

class ConvexPolygon
{
public:
    ConvexPolygon();
    ConvexPolygon( ConvexPolygon &&other );
    ConvexPolygon( const ConvexPolygon &other );
    ConvexPolygon( std::vector<Vector2D> contour );

    // Only use counterclockwise, convex contours here
    ConvexPolygon( std::vector<Vector2D> contour, bool direction );

    ConvexPolygon &operator=( ConvexPolygon &&other );
    ConvexPolygon &operator=( const ConvexPolygon &other );

    ConvexPolygon intersect( const ConvexPolygon &other ) const;
    ConvexPolygon inverse() const;

    bool inside( const Vector2D &point ) const;
    double area() const;
private:
    // ConvexPolygon can only store convex shapes
    // Counterclockwise contour is considered to have positive area
    // Internally contour will be stored counterclockwise
    // Direction is true, if contour was not inverted in constructor
    std::vector<Vector2D> contour;
    bool direction;
};

class ComplexPolygon
{
public:
    ComplexPolygon();
    ComplexPolygon( ComplexPolygon &&other );
    ComplexPolygon( const ComplexPolygon &other );

    // Counterclockwise contours are areas, clockwise contours are holes.
    ComplexPolygon( const std::vector<std::vector<Vector2D>> &contours );

    // Creates rectangle with points p0 and p1 at diagonals and sides parallel to coordinate axis
    ComplexPolygon( const Vector2D &p0, const Vector2D &p1 );

    // Applies transform to ComplexPolygon( {0, 0}, {1, 1} )
    ComplexPolygon( const Affine2D &transform );

    // Creates triangle with points p0, p1, p2
    ComplexPolygon( const Vector2D &p0, const Vector2D &p1, const Vector2D &p2 );

    ComplexPolygon &operator=( ComplexPolygon &&other );
    ComplexPolygon &operator=( const ComplexPolygon &other );

    ComplexPolygon operator&&( const ComplexPolygon &other ) const;
    ComplexPolygon operator||( const ComplexPolygon &other ) const;
    ComplexPolygon operator!() const;

    bool inside( const Vector2D &point ) const;
    double area() const;

    bool carcass( const Vector2D &point ) const;

private:
    class Side
    {
    public:
        // Positions of side's ends (start, finish) in points
        // Order matters, sides, that only differ in order, can both be stored in sides
        size_t s, f;
    };

    class Triangle
    {
    public:
        // Positions of triangle's sides in sides
        // Next sides always have common point in point, start of one is finish of another
        size_t a, b, c;
    };

    // Entities are not repeating, unless repeats are unused
    // Triangles are not intersecting, excluding points and sides
    // All triangles are counterclockwise directed
    // List of points can contain unused points
    // List of sides can contain unused sides
    // List of triangles can NOT contain unused triangles
    // Order of these entities in their containers is irrelevant
    std::vector<Vector2D> points;
    std::vector<Side> sides;
    std::vector<Triangle> triangles;

    // This value is false, when ComplexPolygon was initialized with clockwise contours
    // In that case ComplexPolygon represents inside out shape, while triangles are still stored counterclockwise
    bool counterclockwise;

    // Helpers:

    static void intersect( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r );
    static void unite( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r );
    static void subtract( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r );

    void intersect( const ComplexPolygon &other, ComplexPolygon &r ) const;
    void unite( const ComplexPolygon &other, ComplexPolygon &r ) const;
    void subtract( const ComplexPolygon &other, ComplexPolygon &r ) const;

    bool inside( const Triangle &u, const Vector2D &point ) const;
    double doubleArea( const Triangle &u ) const;

    void establishTriangleTopology( bool ccw );
    void establishQuadrangleTopology( bool ccw );
};
