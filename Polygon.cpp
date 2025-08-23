#include "Polygon.h"

#include <algorithm>

#include "BasicMemory.h"
#include "Exception.h"
#include "BasicMath.h"

// Contours

using Contour = std::vector<Vector2D>;
using Contours = std::vector<Contour>;

using IndexContour = std::vector<size_t>;
using IndexContours = std::vector<IndexContour>;

// Segment

class Segment
{
public:
    const Vector2D &s, &f;

    inline Segment( const Vector2D &start, const Vector2D &finish ) : s( start ), f( finish )
    {}
};

// Helper functions for side checks

inline static double leftSideValue( const Segment &line, const Vector2D &point )
{
    return ( point - line.s ).M( line.f - line.s );
}

inline static bool leftSide( const Segment &line, const Vector2D &point )
{
    return leftSideValue( line, point ) >= 0;
}

inline static bool rightSide( const Segment &line, const Vector2D &point )
{
    return !leftSide( line, point );
}

inline static double insideValue( const Segment &line, const Vector2D &point )
{
    return leftSideValue( line, point );
}

inline static bool inside( const Segment &line, const Vector2D &point )
{
    return leftSide( line, point );
}

inline static bool outside( const Segment &line, const Vector2D &point )
{
    return !inside( line, point );
}

inline static bool inside( const Segment &a, const Segment &b, const Vector2D &point )
{
    if( ( a.f - a.s ).M( b.f - b.s ) < 0 )
        return inside( a, point ) && inside( b, point );
    return !( !inside( a, point ) && !inside( b, point ) );
}

inline static bool insideConvex( const Contour &contour, const Vector2D &point )
{
    auto size = contour.size();
    for( size_t i = 0; i < size; ++i )
    {
        if( !inside( Segment( contour[i], contour[( i + 1 ) % size] ), point ) )
            return false;
    }
    return true;
}

// Helper functions for intersection calculations

// Arguments can't be swapped, b is considered as an infinite line and a is finite segment.
// Direction of intersection of b by a is stored in fromWithin
inline static bool intersectParam( const Segment &a, const Segment &b, double &t, bool &fromWithin )
{
    auto da = a.f - a.s;
    auto db = b.f - b.s;

    auto m = da.M( db );
    auto l = ( b.s - a.s ).M( db );

    // l = -value from leftSide
    fromWithin = l <= 0;

    if( m < 0 )
    {
        m = -m;
        l = -l;
    }

    if( l < 0 )
        return false;

    if( l >= m )
    {
        if( l > 0 )
            return false;
        t = 0;
        fromWithin = false;
        return true;
    }

    t =  l / m;
    return true;
}

// Arguments can't be swapped.
inline static bool intersect( const Segment &a, const Segment &b, Vector2D &common, bool &fromWithin )
{
    double u;
    if( !intersectParam( a, b, u, fromWithin ) )
        return false;

    auto da = a.f - a.s;
    common = a.s + da * u;
    return true;
}

// Arguments can't be swapped.
// Returns intersection parameters.
inline static std::optional<Vector2D> intersect( const Segment &a, const Segment &b )
{
    double u, v;
    bool fromWithin;

    if( !intersectParam( a, b, u, fromWithin ) )
        return {};
    if( !intersectParam( b, a, v, fromWithin ) )
        return {};

    return Vector2D( u, v );
}

// Boolean operations for triangles

namespace Boolean
{
using Vertex = std::variant<Vector2D, size_t>;
using TrianglePolygon = std::array<std::pair<Vector2D, size_t>, 3>;
using Triangle = std::array<Vertex, 3>;
using Triangles = std::vector<Triangle>;

// Rotate three variables in a cycle: ( a, b, c ) -> ( b, c, a )
template<typename T>
void shuffle( T &a, T &b, T &c )
{
    T tmp = std::move( a );
    a = std::move( b );
    b = std::move( c );
    c = std::move( tmp );
}

// Input: triangles p and q with indexed points (indexes can have any values, but none repeats for both triangles)
// Output: list of triangles r. For an input point the stored index is used, for a computed intersection only its coordinate is used
// All triangle vertices in input and output are ordered counterclockwise.
inline static void intersect( TrianglePolygon p, TrianglePolygon q, Triangles &r )
{
    auto sq0 = q[1].first - q[0].first;
    auto sq1 = q[2].first - q[1].first;
    auto sq2 = q[0].first - q[2].first;

    auto sp0 = p[1].first - p[0].first;
    auto sp1 = p[2].first - p[1].first;
    auto sp2 = p[0].first - p[2].first;

    double p0q0 = sq0.M( q[0].first - p[0].first );
    double p1q0 = sq0.M( q[0].first - p[1].first );
    double p2q0 = sq0.M( q[0].first - p[2].first );

    double p0q1 = sq1.M( q[1].first - p[0].first );
    double p1q1 = sq1.M( q[1].first - p[1].first );
    double p2q1 = sq1.M( q[1].first - p[2].first );

    double p0q2 = sq2.M( q[2].first - p[0].first );
    double p1q2 = sq2.M( q[2].first - p[1].first );
    double p2q2 = sq2.M( q[2].first - p[2].first );

    // These boolean variables mostly determine pattern of resulting calculation
    bool fp0q0 = p0q0 >= 0;
    bool fp0q1 = p0q1 >= 0;
    bool fp0q2 = p0q2 >= 0;

    bool fp1q0 = p1q0 >= 0;
    bool fp1q1 = p1q1 >= 0;
    bool fp1q2 = p1q2 >= 0;

    bool fp2q0 = p2q0 >= 0;
    bool fp2q1 = p2q1 >= 0;
    bool fp2q2 = p2q2 >= 0;

    // Determine “inside” status for each vertex of p.
    // A vertex is inside q if it passes all three tests.
    bool in0 = fp0q0 && fp0q1 && fp0q2;
    bool in1 = fp1q0 && fp1q1 && fp1q2;
    bool in2 = fp2q0 && fp2q1 && fp2q2;
    int countInside = ( in0 ? 1 : 0 ) + ( in1 ? 1 : 0 ) + ( in2 ? 1 : 0 );

    // Shuffle the vertices of p (and all associated values) so that:
    // * If exactly one vertex is inside, it becomes p[0].
    // * If exactly two are inside, they become p[0] and p[1].

    auto shuffleP = [&]()
    {
        shuffle( p[0], p[1], p[2] );
        shuffle( sp0, sp1, sp2 );
        shuffle( p0q0, p1q0, p2q0 );
        shuffle( p0q1, p1q1, p2q1 );
        shuffle( p0q2, p1q2, p2q2 );
        shuffle( fp0q0, fp1q0, fp2q0 );
        shuffle( fp0q1, fp1q1, fp2q1 );
        shuffle( fp0q2, fp1q2, fp2q2 );
    };

    auto shuffleQ = [&]()
    {
        shuffle( q[0], q[1], q[2] );
        shuffle( sq0, sq1, sq2 );
        shuffle( p0q0, p0q1, p0q2 );
        shuffle( p1q0, p1q1, p1q2 );
        shuffle( p2q0, p2q1, p2q2 );
        shuffle( fp0q0, fp0q1, fp0q2 );
        shuffle( fp1q0, fp1q1, fp1q2 );
        shuffle( fp2q0, fp2q1, fp2q2 );
    };

    ( void )shuffleQ;

    // Not fully implemented
    makeException( false );

    // Set index of unique inside vertex to 0
    if( countInside == 1 )
    {
        if( in1 )
        {
            shuffleP();
        }
        else if( in2 )
        {
            shuffleP();
            shuffleP();
        }
    }
    else if( countInside == 2 )
    {
        // Ensure that an inside vertexes collected in the beginning
        if( !in0 )
        {
            if( in1 )
            {
                shuffleP();
            }
            else if( in2 )
            {
                shuffleP();
                shuffleP();
            }
        }
        // If p[1] is not inside, swap p[1] and p[2].
        if( !( fp1q0 && fp1q1 && fp1q2 ) )
        {
            std::swap( p[1], p[2] );
            std::swap( sp1, sp2 );
            std::swap( p1q0, p2q0 );
            std::swap( p1q1, p2q1 );
            std::swap( p1q2, p2q2 );
            std::swap( fp1q0, fp2q0 );
            std::swap( fp1q1, fp2q1 );
            std::swap( fp1q2, fp2q2 );
        }
    }

    // Case 1. Triangle p is completely inside q.
    if(
        fp0q0 && fp0q1 && fp0q2 &&
        fp1q0 && fp1q1 && fp1q2 &&
        fp2q0 && fp2q1 && fp2q2 )
    {
        Triangle triangle
        {
            p[0].second,
            p[1].second,
            p[2].second
        };
        r.push_back( triangle );
        return;
    }

    // Case 2. One vertex of p is inside q, the other two are outside (assumed to fail the q–edge 2 test).
    if(
        fp0q0 && fp0q1 && fp0q2 &&
        fp1q0 && fp1q1 && !fp1q2 &&
        fp2q0 && fp2q1 && !fp2q2 )
    {
        Triangle triangle
        {
            p[0].second,
            p[0].first + ( p0q2 / ( p0q2 - p1q2 ) ) *sp0, // p[0] - p[1]
            p[2].first + ( p2q2 / ( p2q2 - p0q2 ) ) *sp2 // p[2] - p[0]
        };
        r.push_back( triangle );
        return;
    }

    // Case 3. Two vertices of p are inside q, one is outside.
    if(
        fp0q0 && fp0q1 && fp0q2 &&
        fp1q0 && fp1q1 && fp1q2 &&
        fp2q0 && fp2q1 && !fp2q2 )
    {
        auto r0 = p[0].second;
        auto r1 = p[1].second;
        auto r2 = p[1].first + ( p1q2 / ( p1q2 - p2q2 ) ) * sp1; // p[1] - p[2]
        auto r3 = p[2].first + ( p2q2 / ( p2q2 - p0q2 ) ) * sp2; // p[2] - p[0]

        Triangle triangle0
        {
            r0, r1, r2
        };
        r.push_back( triangle0 );

        Triangle triangle1
        {
            r0, r2, r3
        };
        r.push_back( triangle1 );
        return;
    }

    // All valid cases should be processed above
    // makeException( false );
}

inline static void unite( const TrianglePolygon &p, const TrianglePolygon &q, Triangles &r )
{
    ( void )p;
    ( void )q;
    ( void )r;

    // Not implemented
    makeException( false );
}

inline static void subtract( const TrianglePolygon &p, const TrianglePolygon &q, Triangles &r )
{
    ( void )p;
    ( void )q;
    ( void )r;

    // Not implemented
    makeException( false );
}
}

// Area

inline static double calculateArea( const Contour &contour )
{
    double s = 0;
    auto size = contour.size();
    for( size_t i = 0; i < size; ++i )
        s += contour[i].M( contour[( i + 1 ) % size] );

    s = s * 0.5;
    return s;
}

// Polygon clipping

inline static void clip( Contour &polygon, const Segment &side )
{
    static Contour result;
    bool fromWithin, intersects;
    Vector2D common;

    result.clear();

    auto size = polygon.size();
    for( size_t i = 0; i < size; ++i )
    {
        auto &point = polygon[i];
        auto &next = polygon[( i + 1 ) % size];

        intersects = intersect( Segment( point, next ), side, common, fromWithin );
        if( !intersects && fromWithin )
        {
            result.push_back( point );
        }
        else if( intersects && fromWithin )
        {
            result.push_back( point );
            result.push_back( common );
        }
        else if( intersects && !fromWithin )
        {
            result.push_back( common );
        }
    }

    polygon = result;
}

inline static void clipByConvex( Contour &shape, const Contour &cutter )
{
    auto size = cutter.size();
    for( size_t i = 0; i < size; ++i )
        clip( shape, Segment( cutter[i], cutter[( i + 1 ) % size] ) );
}

// Check convexity and contour direction

inline static bool isConvex( const Contour &contour, bool &direction )
{
    auto size = contour.size();
    if( size < 3 )
    {
        direction = true;
        return true;
    }

    short sign = 0;
    for( size_t i = 0; i < size; ++i )
    {
        auto &p0 = contour[i];
        auto &p1 = contour[( i + 1 ) % size];
        auto &p2 = contour[( i + 2 ) % size];

        double cross = ( p1 - p0 ).M( p2 - p1 );
        if( cross < 0 )
        {
            if( sign > 0 )
                return false;
            sign = -1;
        }
        else if( cross > 0 )
        {
            if( sign < 0 )
                return false;
            sign = 1;
        }
    }

    direction = sign <= 0;
    return true;
}

// Normalization (Makes contours, that represent same shapes, same)

inline bool order( const Vector2D &a, const Vector2D &b )
{
    if( a.x > b.x )
        return false;

    if( a.x < b.x )
        return true;

    if( a.y < b.y )
        return true;

    return false;
}

inline static size_t pivot( const Contour &contour )
{
    size_t connerId = 0;
    auto size = contour.size();
    for( size_t i = 0; i < size; ++i )
    {
        auto &p = contour[i];
        auto &conner = contour[connerId];
        if( order( p, conner ) )
            connerId = i;
    }
    return connerId;
}

inline static void normalize( Contour &contour )
{
    if( contour.empty() )
        return;

    auto exactVector = []( const Vector2D & a, const Vector2D & b )
    {
        return compare( &a, &b, sizeof( a ) );
    };
    auto i = std::unique( contour.begin(), contour.end(), exactVector );
    contour.erase( i, contour.end() );

    auto size = contour.size();
    auto shift = ( size - pivot( contour ) ) % size;
    if( shift == 0 )
        return;

    std::rotate( contour.rbegin(), contour.rbegin() + shift, contour.rend() );
}

// Ear clipping triangulation

inline static bool isEar( const Contour &polygon, size_t i )
{
    size_t prev = ( i + polygon.size() - 1 ) % polygon.size();
    size_t next = ( i + 1 ) % polygon.size();

    auto v1 = polygon[next] - polygon[i];
    auto v2 = polygon[prev] - polygon[i];

    if( v1.M( v2 ) >= 0 )
        return false;

    for( size_t j = 0; j < polygon.size(); ++j )
    {
        if( j == i || j == prev || j == next )
            continue;

        if( inside( Segment( polygon[prev], polygon[next] ), polygon[j] ) )
            return false;
    }

    return true;
}

inline static Contours triangulateEarClipping( Contour polygon )
{
    Contours triangles;

    while( polygon.size() > 3 )
    {
        for( size_t i = 0; i < polygon.size(); ++i )
        {
            if( isEar( polygon, i ) )
            {
                size_t prev = ( i + polygon.size() - 1 ) % polygon.size();
                size_t next = ( i + 1 ) % polygon.size();

                triangles.push_back( { polygon[prev], polygon[i], polygon[next] } );
                polygon.erase( polygon.begin() + i );
                break;
            }
        }
    }

    if( polygon.size() == 3 )
        triangles.push_back( polygon );

    return triangles;
}

// ConvexPolygon

ConvexPolygon::ConvexPolygon() : direction( true )
{}

ConvexPolygon::ConvexPolygon( ConvexPolygon &&other ) : contour( std::move( other.contour ) ), direction( other.direction )
{}

ConvexPolygon::ConvexPolygon( const ConvexPolygon &other ) : contour( other.contour ), direction( other.direction )
{}

ConvexPolygon::ConvexPolygon( Contour c ) : contour( std::move( c ) )
{
    makeException( isConvex( contour, direction ) );
    if( !direction )
        std::reverse( contour.begin(), contour.end() );
}

ConvexPolygon::ConvexPolygon( Contour c, bool d ) : contour( std::move( c ) ), direction( d )
{}

ConvexPolygon &ConvexPolygon::operator=( ConvexPolygon &&other )
{
    contour = std::move( other.contour );
    direction = other.direction;
    return *this;
}

ConvexPolygon &ConvexPolygon::operator=( const ConvexPolygon &other )
{
    contour = other.contour;
    direction = other.direction;
    return *this;
}

ConvexPolygon ConvexPolygon::intersect( const ConvexPolygon &other ) const
{
    static ConvexPolygon result;
    result = *this;
    clipByConvex( result.contour, other.contour );
    result.direction = direction == other.direction;
    return result;
}

ConvexPolygon ConvexPolygon::inverse() const
{
    static ConvexPolygon result;
    result = *this;
    result.direction = !result.direction;
    return result;
}

bool ConvexPolygon::inside( const Vector2D &point ) const
{
    return insideConvex( contour, point );
}

double ConvexPolygon::area() const
{
    auto s = calculateArea( contour );
    if( !direction )
        s = -s;
    return s;
}

// ComplexPolygon

ComplexPolygon::ComplexPolygon()
{
    counterclockwise = true;
}

ComplexPolygon::ComplexPolygon( ComplexPolygon &&other ) :
    points( std::move( other.points ) ),
    sides( std::move( other.sides ) ),
    triangles( std::move( other.triangles ) ),
    counterclockwise( other.counterclockwise )
{}

ComplexPolygon::ComplexPolygon( const ComplexPolygon &other ) :
    points( other.points ),
    sides( other.sides ),
    triangles( other.triangles ),
    counterclockwise( other.counterclockwise )
{}

ComplexPolygon::ComplexPolygon( const Contours &contours )
{
    ( void )contours;

    // Not implemented
    makeException( false );
}

ComplexPolygon::ComplexPolygon( const Vector2D &p0, const Vector2D &p1 )
{
    points = {p0, {p0.x, p1.y}, p1, {p1.x, p0.y}};
    counterclockwise = SameSign( p1.x - p0.x, p1.y - p0.y );
    establishQuadrangleTopology( counterclockwise );
}

ComplexPolygon::ComplexPolygon( const Affine2D &transform )
{
    points = {transform( {0, 0} ), transform( {0, 1} ), transform( {1, 1} ), transform( {1, 0} )};
    counterclockwise = transform.t.det() >= 0;
    establishQuadrangleTopology( counterclockwise );
}

ComplexPolygon::ComplexPolygon( const Vector2D &p0, const Vector2D &p1, const Vector2D &p2 )
{
    points = {p0, p1, p2};
    counterclockwise = ( p1 - p0 ).M( p2 - p1 ) <= 0;
    establishTriangleTopology( counterclockwise );
}

ComplexPolygon &ComplexPolygon::operator=( ComplexPolygon &&other )
{
    points = std::move( other.points );
    sides = std::move( other.sides );
    triangles = std::move( other.triangles );
    counterclockwise = other.counterclockwise;
    return *this;
}

ComplexPolygon &ComplexPolygon::operator=( const ComplexPolygon &other )
{
    points = other.points;
    sides = other.sides;
    triangles = other.triangles;
    counterclockwise = other.counterclockwise;
    return *this;
}

ComplexPolygon ComplexPolygon::operator&&( const ComplexPolygon &other ) const
{
    ComplexPolygon r;
    intersect( other, r );
    return r;
}

ComplexPolygon ComplexPolygon::operator||( const ComplexPolygon &other ) const
{
    ComplexPolygon r;
    unite( other, r );
    return r;
}

ComplexPolygon ComplexPolygon::operator!() const
{
    auto result = *this;
    result.counterclockwise = !counterclockwise;
    return result;
}

bool ComplexPolygon::inside( const Vector2D &point ) const
{
    if( counterclockwise )
    {
        for( auto &t : triangles )
        {
            if( inside( t, point ) )
                return true;
        }
        return false;
    }
    for( auto &t : triangles )
    {
        if( !inside( t, point ) )
            return false;
    }
    return true;
}

double ComplexPolygon::area() const
{
    double s = 0;
    for( auto &t : triangles )
        s += doubleArea( t );
    return s * 0.5;
}

bool ComplexPolygon::carcass( const Vector2D &point ) const
{
    for( auto &t : triangles )
    {
        auto p0 = points[sides[t.a].s];
        auto p1 = points[sides[t.b].s];
        auto p2 = points[sides[t.c].s];
        if( !counterclockwise )
            std::swap( p0, p1 );

        auto p = ( p0 + p1 + p2 ) / 3;

        auto bp0 = ( p0 - p ) * 1.02 + p;
        auto bp1 = ( p1 - p ) * 1.02 + p;
        auto bp2 = ( p2 - p ) * 1.02 + p;

        auto sp0 = ( p0 - p ) * 0.98 + p;
        auto sp1 = ( p1 - p ) * 0.98 + p;
        auto sp2 = ( p2 - p ) * 0.98 + p;

        bool bi = ::inside( Segment( bp0, bp1 ), point ) && ::inside( Segment( bp1, bp2 ), point ) && ::inside( Segment( bp2, bp0 ), point );
        bool so = !::inside( Segment( sp0, sp1 ), point ) || !::inside( Segment( sp1, sp2 ), point ) || !::inside( Segment( sp2, sp0 ), point );

        if( bi && so )
            return true;
    }
    return false;
}

// INTERSECTION (p ∩ q):
void ComplexPolygon::intersect( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r )
{
    Boolean::TrianglePolygon pTriangle
    {
        std::make_pair( p.points[p.sides[p.triangles[pId].a].s], p.sides[p.triangles[pId].a].s ),
        std::make_pair( p.points[p.sides[p.triangles[pId].b].s], p.sides[p.triangles[pId].b].s ),
        std::make_pair( p.points[p.sides[p.triangles[pId].c].s], p.sides[p.triangles[pId].c].s )
    };

    Boolean::TrianglePolygon qTriangle =
    {
        std::make_pair( q.points[q.sides[q.triangles[qId].a].s], q.sides[q.triangles[qId].a].s + p.points.size() ),
        std::make_pair( q.points[q.sides[q.triangles[qId].b].s], q.sides[q.triangles[qId].b].s + p.points.size() ),
        std::make_pair( q.points[q.sides[q.triangles[qId].c].s], q.sides[q.triangles[qId].c].s + p.points.size() )
    };

    Boolean::Triangles rTriangles;
    Boolean::intersect( pTriangle, qTriangle, rTriangles );
    for( auto &rTriangle : rTriangles )
    {
        for( auto &vertex : rTriangle )
        {
            if( std::holds_alternative<Vector2D>( vertex ) )
            {
                auto point = std::get<Vector2D>( vertex );
                vertex = r.points.size();
                r.points.push_back( point );
            }
        }

        Triangle triangle;

        triangle.a = r.sides.size();
        r.sides.push_back( {std::get<size_t>( rTriangle[0] ), std::get<size_t>( rTriangle[1] )} );

        triangle.b = r.sides.size();
        r.sides.push_back( {std::get<size_t>( rTriangle[1] ), std::get<size_t>( rTriangle[2] )} );

        triangle.c = r.sides.size();
        r.sides.push_back( {std::get<size_t>( rTriangle[2] ), std::get<size_t>( rTriangle[0] )} );

        r.triangles.push_back( triangle );
    }
}

// UNION (p ∪ q):
void ComplexPolygon::unite( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r )
{
    ( void )p;
    ( void )pId;
    ( void )q;
    ( void )qId;
    ( void )r;

    // Not implemented
    makeException( false );
}

// SUBTRACTION (p - q):
void ComplexPolygon::subtract( const ComplexPolygon &p, size_t pId, const ComplexPolygon &q, size_t qId, ComplexPolygon &r )
{
    ( void )p;
    ( void )pId;
    ( void )q;
    ( void )qId;
    ( void )r;

    // Not implemented
    makeException( false );
}

void ComplexPolygon::intersect( const ComplexPolygon &other, ComplexPolygon &r ) const
{
    r.points = points;
    r.points.insert( r.points.end(), other.points.begin(), other.points.end() );

    for( size_t i = 0; i < triangles.size(); ++i )
    {
        for( size_t j = 0; j < other.triangles.size(); ++j )
        {
            intersect( *this, i, other, j, r );
        }
    }
}

void ComplexPolygon::unite( const ComplexPolygon &other, ComplexPolygon &r ) const
{
    // Not implemented
    makeException( false );

    ComplexPolygon trimmed = *this;
    r = other;
    r.points.insert( r.points.end(), points.begin(), points.end() );

    for( size_t i = 0; i < triangles.size(); ++i )
    {
        for( size_t j = 0; j < other.triangles.size(); ++j )
        {
            subtract( other, j, *this, i, r );
        }
    }

    r = std::move( trimmed );

    for( size_t i = 0; i < triangles.size(); ++i )
    {}
}

void ComplexPolygon::subtract( const ComplexPolygon &other, ComplexPolygon &r ) const
{
    ( void )other;
    ( void )r;

    // Not implemented
    makeException( false );
}

bool ComplexPolygon::inside( const Triangle &u, const Vector2D &point ) const
{
    auto &a = sides[u.a];
    auto &b = sides[u.b];
    auto &c = sides[u.c];

    auto p0 = points[a.s];
    auto p1 = points[b.s];
    auto p2 = points[c.s];

    if( counterclockwise )
        return ::inside( Segment( p0, p1 ), point ) && ::inside( Segment( p1, p2 ), point ) && ::inside( Segment( p2, p0 ), point );
    return ::inside( Segment( p0, p1 ), point ) || ::inside( Segment( p1, p2 ), point ) || ::inside( Segment( p2, p0 ), point );
}

double ComplexPolygon::doubleArea( const Triangle &u ) const
{
    auto &a = sides[u.a];
    auto &b = sides[u.b];
    return ( points[b.f] - points[b.s] ).M( points[a.s] - points[a.f] );
}

void ComplexPolygon::establishTriangleTopology( bool ccw )
{
    if( ccw )
    {
        sides = {{0, 1}, {1, 2}, {2, 0}};
        triangles = {{0, 1, 2}};
    }
    else
    {
        sides = {{0, 2}, {2, 1}, {1, 0}};
        triangles = {{0, 1, 2}};
    }
}

void ComplexPolygon::establishQuadrangleTopology( bool ccw )
{
    if( ccw )
    {
        sides = {{0, 1}, {1, 2}, {2, 0}, {0, 2}, {2, 3}, {3, 0}};
        triangles = {{0, 1, 2}, {3, 4, 5}};
    }
    else
    {
        sides = {{0, 2}, {2, 1}, {1, 0}, {0, 3}, {3, 2}, {2, 0}};
        triangles = {{0, 1, 2}, {3, 4, 5}};

    }
}
