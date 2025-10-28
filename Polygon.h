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
    struct Side
    {
        // Positions of side's ends (start, finish) in points
        // Order matters, sides, that only differ in order, can both be stored in `sides`
        size_t s, f;
    };

    struct Triangle
    {
        // Positions of triangle's sides in sides
        // Next side always have common point with previous, start of a side is finish of a previous side
        size_t a, b, c;
    };

    ComplexPolygon();
    ComplexPolygon( ComplexPolygon &&other );
    ComplexPolygon( const ComplexPolygon &other );

    // Creates simple polygon
    ComplexPolygon( const std::vector<Vector2D> &contour );

    // Counterclockwise contours are areas, clockwise contours are holes
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
    ComplexPolygon operator-( const ComplexPolygon &other ) const;
    ComplexPolygon operator!() const;

    bool inside( const Vector2D &point ) const;
    double area() const;

    bool carcass( const Vector2D &point ) const;

    // Access
    template<typename V>
    struct V3
    {
        V a, b, c;
    };

    template<typename V>
    struct Data
    {
    public:
        using Type = V;

        Triangle t;
        V3<Side> s;
        V3<V> p;
    };

    template<typename P>
    class Iterator
    {
    private:
        P &polygon;
        size_t id;
    public:
        Iterator( P &p, size_t i ) : polygon( p ), id( i )
        {}

        Iterator &operator++()
        {
            ++id;
            return *this;
        }

        bool operator==( const Iterator &other ) const
        {
            return id == other.id;
        }

        bool operator!=( const Iterator &other ) const
        {
            return id != other.id;
        }

        template<class From, class To>
        using copyConst = std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, std::remove_reference_t<To>, To>;

        Data<copyConst<P, Vector2D&>> operator*() const
        {
            return polygon[id];
        }
    };

    Data<Vector2D> operator[]( size_t id ) const;
    Data<Vector2D&> operator[]( size_t id );

    Iterator<ComplexPolygon> begin();
    Iterator<ComplexPolygon> end();

    Iterator<const ComplexPolygon> begin() const;
    Iterator<const ComplexPolygon> end() const;
private:
    // Entities are not repeating, unless repeats are unused
    // Triangles are not intersecting, excluding points and sides
    // All triangles are counterclockwise directed
    // Lists of points and sides can contain unused entities
    // List of triangles can NOT contain unused triangles
    // Order of these entities in their containers is irrelevant
    std::vector<Vector2D> points;
    std::vector<Side> sides;
    std::vector<Triangle> triangles;

    // This value is false, when ComplexPolygon was initialized with clockwise contours
    // In that case ComplexPolygon represents inside out shape, while triangles are still stored counterclockwise
    bool counterclockwise;

    // Helpers:
    static std::array<std::pair<Vector2D, size_t>, 3> getTriangle( const ComplexPolygon &p, size_t id, size_t shift );
    static void addTriangles( std::vector<std::array<std::variant<Vector2D, size_t>, 3>>& triangles, ComplexPolygon &p );

    static void intersect( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r );
    static void unite( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r );
    static void subtract( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r );

    bool inside( const Triangle &u, const Vector2D &point ) const;
    double doubleArea( const Triangle &u ) const;

    void establishTriangleTopology( bool ccw );
    void establishQuadrangleTopology( bool ccw );
};
