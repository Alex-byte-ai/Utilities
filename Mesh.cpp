#include "Mesh.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "Exception.h"
#include "Scanner.h"
#include "Polygon.h"
#include "Basic.h"

// Line p0, p1
// Triangle v0, v1, v2
// The intersection point p
// Parameter t along line segment: p = p0 + t( p1 - p0 )
// Barycentric coordinates u, v in triangle: p = v0 + u( v1 - v0 ) + v( v2 - v0 )
// Returns true, if p lies within the triangle and line hits triangle from the front
static bool intersectSegmentTriangle(
    const Vector3D &p0, const Vector3D &p1,
    const Vector3D &v0, const Vector3D &v1, const Vector3D &v2,
    double &u, double &v, double &t
)
{
    auto dir = p1 - p0;
    auto edge1 = v1 - v0;
    auto edge2 = v2 - v0;

    if( dir * edge1.M( edge2 ) >= 0 )
        return false; // Hits a face from the back

    // Möller–Trumbore intersection

    auto pvec = dir.M( edge2 );
    double det = edge1 * pvec;
    if( Abs( det ) < Vector3D::epsilon )
        return false; // Parallel or degenerate

    double invDet = 1.0 / det;

    auto tvec = p0 - v0;
    auto qvec = tvec.M( edge1 );

    u = ( tvec * pvec ) * invDet;
    v = ( dir * qvec ) * invDet;
    t = ( edge2 * qvec ) * invDet;

    return u >= 0.0 && v >= 0.0 && u + v <= 1.0;
}

class DiscreteFunction
{
public:
    std::vector<size_t> f;

    DiscreteFunction()
    {}

    DiscreteFunction( size_t n )
    {
        f.reserve( n );
    }

    void squishEmptySpace( const Bitset &bitset )
    {
        f.clear();
        f.reserve( bitset.count() );

        auto size = bitset.size();
        for( size_t i = 0; i < size; ++i )
        {
            if( bitset.test( i ) )
                f.push_back( i );
        }
    }

    operator bool() const
    {
        return !f.empty();
    }

    size_t operator()( size_t i ) const
    {
        makeException( i < f.size() );
        return f[i];
    }

    static bool empty( size_t )
    {
        return false;
    }

    Bitset operator()( const Bitset &bitset ) const
    {
        Bitset result;
        result.resize( f.size() );
        for( size_t i = 0; i < f.size(); ++i )
            result.set( bitset.test( f[i] ) );
        return result;
    };

    static bool empty( const Bitset &bitset )
    {
        return bitset.none();
    }

    template<typename T>
    std::vector<T> operator()( const std::vector<T> &vector ) const
    {
        std::vector<T> result;
        result.reserve( f.size() );
        for( size_t i = 0; i < f.size(); ++i )
            result.emplace_back( vector[f[i]] );
        return result;
    };

    template<typename T>
    static bool empty( const std::vector<T> &vector )
    {
        return vector.empty();
    }

    template<typename Key, typename Value>
    std::map<Key, Value> operator()( const std::map<Key, Value> &map ) const
    {
        std::map<Key, Value> result;

        for( auto& [key, value] : map )
        {
            auto other = ( *this )( value );
            if( !empty( other ) )
                result.emplace( std::make_pair( key, std::move( other ) ) );
        }

        return result;
    };

    template<typename Key, typename Value>
    static bool empty( const std::map<Key, Value> &map )
    {
        return map.empty();
    }

    template<typename T>
    std::optional<T> operator()( const std::optional<T> &optional ) const
    {
        if( optional )
            return ( *this )( *optional );
        return {};
    };

    template<typename T>
    static bool empty( const std::optional<T> &optional )
    {
        return !optional || empty( *optional );
    }
};

class Sorter
{
private:
    const Mesh::Group &group;
    std::vector<size_t> faceGroup;
    DiscreteFunction faceReorder;

public:
    Sorter( const Mesh::Group &g, std::vector<Mesh::Face> &f ) : group( g ), faceGroup( f.size() ), faceReorder( f.size() )
    {
        for( size_t i = 0; i < f.size(); ++i )
        {
            size_t j = 0;
            for( auto& [name, bitset] : group )
            {
                if( bitset.test( i ) )
                    break;
                ++j;
            }
            if( j >= group.size() )
            {
                faceGroup.clear();
                faceReorder.f.clear();
                return;
            }
            faceGroup[i] = j;
            faceReorder.f.push_back( i );
        }

        std::sort( faceReorder.f.begin(), faceReorder.f.end(), *this );
    }

    bool operator()( size_t a, size_t b ) const
    {
        a = faceGroup[a];
        b = faceGroup[b];
        return a < b;
    }

    const DiscreteFunction &operator()() const
    {
        return faceReorder;
    }
};

Mesh::Groups::Groups( bool f0, bool f1, bool f2 )
{
    if( f0 ) o.emplace();
    if( f1 ) g.emplace();
    if( f2 ) m.emplace();
}

Mesh::Groups::Groups( const Mesh::Groups& other )
    : o( other.o ), g( other.g ), m( other.m )
{}

Mesh::Groups::Groups( Mesh::Groups&& other )
    : o( std::move( other.o ) ), g( std::move( other.g ) ), m( std::move( other.m ) )
{}

Mesh::Groups &Mesh::Groups::operator=( const Mesh::Groups &other )
{
    o = other.o;
    g = other.g;
    m = other.m;
    return *this;
}

Mesh::Groups &Mesh::Groups::operator=( Mesh::Groups &&other )
{
    o = std::move( other.o );
    g = std::move( other.g );
    m = std::move( other.m );
    return *this;
}

Mesh::Group *Mesh::Groups::group( int id )
{
    if( id == 0 && o )
        return &*o;
    if( id == 1 && g )
        return &*g;
    if( id == 2 && m )
        return &*m;
    return nullptr;
}

void Mesh::Groups::clear()
{
    bool of = o.has_value();
    bool gf = g.has_value();
    bool mf = m.has_value();

    o.reset();
    g.reset();
    m.reset();

    if( of ) o.emplace();
    if( gf ) g.emplace();
    if( mf ) m.emplace();
}

Mesh::Mesh( Groups grps )
    : groups( std::move( grps ) )
{
    groups.clear();
}

Mesh::Mesh( const Mesh &other ) :
    points( other.points ),
    normals( other.normals ),
    uv( other.uv ),
    edges( other.edges ),
    faces( other.faces ),
    groups( other.groups )
{}

Mesh::Mesh( Mesh &&other ) :
    points( std::move( other.points ) ),
    normals( std::move( other.normals ) ),
    uv( std::move( other.uv ) ),
    edges( std::move( other.edges ) ),
    faces( std::move( other.faces ) ),
    groups( std::move( other.groups ) )
{}

Mesh &Mesh::operator=( const Mesh &other )
{
    points = other.points;
    normals = other.normals;
    uv = other.uv;
    edges = other.edges;
    faces = other.faces;
    groups = other.groups;
    return*this;
}

Mesh &Mesh::operator=( Mesh &&other )
{
    points = std::move( other.points );
    normals = std::move( other.normals );
    uv = std::move( other.uv );
    edges = std::move( other.edges );
    faces = std::move( other.faces );
    groups = std::move( other.groups );
    return*this;
}

std::optional<size_t> Mesh::intersectSegment(
    const Vector3D &p0, const Vector3D &p1,
    double &u, double &v, double &t
) const
{
    double u0, v0, t0, tMin = std::numeric_limits<double>::max();
    std::optional<size_t> faceId;

    size_t i = 0;
    for( auto tri : *this )
    {
        if( intersectSegmentTriangle( p0, p1, tri.p.a, tri.p.b, tri.p.c, u0, v0, t0 ) )
        {
            if( t0 < tMin )
            {
                faceId = i;
                t = tMin = t0;
                u = u0;
                v = v0;
            }
        }
        ++i;
    }

    return faceId;
}

void Mesh::cube()
{
    clear();

    // 8 corner points of the unit cube
    points =
    {
        {0, 0, 0}, // 0
        {1, 0, 0}, // 1
        {1, 1, 0}, // 2
        {0, 1, 0}, // 3
        {0, 0, 1}, // 4
        {1, 0, 1}, // 5
        {1, 1, 1}, // 6
        {0, 1, 1}, // 7
    };

    // A single UV layout for every face:
    uv =
    {
        {0, 0, 0}, // 0
        {1, 0, 0}, // 1
        {1, 1, 0}, // 2
        {0, 1, 0}, // 3
    };

    // 6 normals for each side made of two faces
    normals =
    {
        {1, 0, 0}, // 0
        {0, 1, 0}, // 1
        {0, 0, 1}, // 2
        {-1, 0, 0}, // 3
        {0, -1, 0}, // 4
        {0, 0, -1}, // 5
    };

    // Helper to add one triangle
    auto addTri = [&]( size_t i0, size_t i1, size_t i2, size_t u0, size_t u1, size_t u2, size_t n )
    {
        size_t e0 = edges.size();
        edges.push_back( {i0, i1} );

        size_t e1 = edges.size();
        edges.push_back( {i1, i2} );

        size_t e2 = edges.size();
        edges.push_back( {i2, i0} );

        faces.push_back( Face{ Triplet{ e0, e1, e2 }, Triplet{ n, n, n }, Triplet{ u0, u1, u2 } } );
    };

    // 0, 1, 2, 3
    addTri( 0, 3, 1,  0, 3, 1,  5 );
    addTri( 2, 1, 3,  2, 1, 3,  5 );

    // 4, 5, 6, 7
    addTri( 6, 7, 5,  2, 3, 1,  2 );
    addTri( 4, 5, 7,  0, 1, 3,  2 );

    // 3, 2, 6, 7
    addTri( 3, 7, 2,  0, 3, 1,  1 );
    addTri( 6, 2, 7,  2, 1, 3,  1 );

    // 1, 0, 4, 5
    addTri( 1, 5, 0,  0, 3, 1,  4 );
    addTri( 4, 0, 5,  2, 1, 3,  4 );

    // 0, 3, 7, 4
    addTri( 0, 4, 3,  0, 3, 1,  3 );
    addTri( 7, 3, 4,  2, 1, 3,  3 );

    // 2, 1, 5, 6
    addTri( 2, 6, 1,  0, 3, 1,  0 );
    addTri( 5, 1, 6,  2, 1, 3,  0 );
}

void Mesh::plane( size_t rows, size_t columns )
{
    clear();

    for( size_t i = 0; i <= rows; ++i )
    {
        double v = double( i ) / rows;
        for( size_t j = 0; j <= columns; ++j )
        {
            double u = double( j ) / columns;
            Vector3D p( u, v, 0.0 );
            points.push_back( p );
            uv.push_back( p );
        }
    }

    normals =
    {
        {0, 0, -1},
    };

    auto idx = [columns]( size_t j, size_t i )
    {
        return i * ( columns + 1 ) + j;
    };

    // Helper to add one triangle
    auto addTri = [&]( size_t i0, size_t i1, size_t i2 )
    {
        size_t e0 = edges.size();
        edges.push_back( {i0, i1} );

        size_t e1 = edges.size();
        edges.push_back( {i1, i2} );

        size_t e2 = edges.size();
        edges.push_back( {i2, i0} );

        // UV indexes are same as point indexes
        faces.push_back( Face{ Triplet{ e0, e1, e2 }, Triplet{ 0, 0, 0 }, Triplet{ i0, i1, i2 } } );
    };

    // For each cell, make two CCW triangles
    for( size_t i = 0; i < rows; ++i )
    {
        for( size_t j = 0; j < columns; ++j )
        {
            size_t p0 = idx( j, i );
            size_t p1 = idx( j + 1, i );
            size_t p2 = idx( j + 1, i + 1 );
            size_t p3 = idx( j, i + 1 );

            addTri( p0, p3, p1 );
            addTri( p2, p1, p3 );
        }
    }
}

void Mesh::prism( const std::vector<Vector2D>& base )
{
    clear();

    Interval<double> width, height;

    for( auto& p : base )
    {
        points.emplace_back( p.x, p.y, 0.0 );
        width.add( p.x );
        height.add( p.y );
    }

    uv =
    {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
    };

    for( auto& p : base )
    {
        points.emplace_back( p.x, p.y, 1.0 );
        uv.emplace_back( width.normalize( p.x ), width.normalize( p.y ), 0 );
    }

    normals =
    {
        {0, 0, -1},
        {0, 0, 1},
    };

    auto size = base.size();

    for( size_t i = 0; i < size; ++i )
    {
        auto& point = base[i];
        auto& next = base[( i + 1 ) % size];

        auto d = ( next - point ).Normal();
        normals.emplace_back( -d.y, d.x, 0.0 );
    }

    // Helper to add one triangle of a side of the prism
    auto addSideTri = [&]( size_t i0, size_t i1, size_t i2, size_t u0, size_t u1, size_t u2, size_t n )
    {
        size_t e0 = edges.size();
        edges.push_back( {i0, i1} );

        size_t e1 = edges.size();
        edges.push_back( {i1, i2} );

        size_t e2 = edges.size();
        edges.push_back( {i2, i0} );

        faces.push_back( Face{ Triplet{ e0, e1, e2 }, Triplet{ n, n, n }, Triplet{ u0, u1, u2 } } );
    };

    // Helper to add one triangle of a base of the prism
    auto addBaseTri = [&]( size_t i0, size_t i1, size_t i2 )
    {
        size_t e0 = edges.size();
        edges.push_back( {i0, i1} );

        size_t e1 = edges.size();
        edges.push_back( {i1, i2} );

        size_t e2 = edges.size();
        edges.push_back( {i2, i0} );

        size_t t0 = i0, t1 = i1, t2 = i2, n = 0;
        if( t0 >= base.size() )
        {
            t0 -= base.size();
            t1 -= base.size();
            t2 -= base.size();
            n = 1;
        }

        faces.push_back( Face{ Triplet{ e0, e1, e2 }, Triplet{ n, n, n }, Triplet{ t0, t1, t2 } } );
    };

    // For each side, make two CCW triangles
    for( size_t i = 0; i < size; ++i )
    {
        size_t p0 = i;
        size_t p1 = ( i + 1 ) % size;
        size_t p2 = p1 + size;
        size_t p3 = p0 + size;

        addSideTri( p0, p3, p1, 0, 3, 1, i + 2 );
        addSideTri( p2, p1, p3, 2, 1, 3, i + 2 );
    }

    // Triangulate base and add triangles

    ComplexPolygon triangles( base );

    for( auto t : triangles )
        addBaseTri( t.s.a.s, t.s.b.s, t.s.c.s );

    for( auto t : triangles )
        addBaseTri( t.s.a.s + size, t.s.b.s + size, t.s.c.s + size );
}

Mesh Mesh::extract( const Bitset &faceSet ) const
{
    DiscreteFunction f;
    f.squishEmptySpace( faceSet );

    Mesh result;
    result.points = points;
    result.normals = normals;
    result.uv = uv;
    result.edges = edges;
    result.faces = f( faces );
    result.groups.o = f( groups.o );
    result.groups.g = f( groups.g );
    result.groups.m = f( groups.m );
    return result;
}

void Mesh::remakeNormals( bool faceNormals )
{
    normals.clear();

    for( auto &face : faces )
    {
        auto &edgeA = edges[face.p.a];
        auto &edgeB = edges[face.p.b];
        auto &edgeC = edges[face.p.c];

        auto &pointA = points[edgeA.s];
        auto &pointB = points[edgeB.s];
        auto &pointC = points[edgeC.s];

        face.n.a = face.n.b = face.n.c = normals.size();
        normals.push_back( ( pointB - pointA ).M( pointC - pointB ).Normal() );
    }

    if( !faceNormals )
    {
        std::vector<std::set<std::pair<size_t, double>>> pointTriangles( points.size() );

        for( size_t f = 0; f < faces.size(); ++f )
        {
            auto &face = faces[f];

            auto &edgeA = edges[face.p.a];
            auto &edgeB = edges[face.p.b];
            auto &edgeC = edges[face.p.c];

            auto pointIdA = edgeA.s;
            auto pointIdB = edgeB.s;
            auto pointIdC = edgeC.s;

            auto &pointA = points[pointIdA];
            auto &pointB = points[pointIdB];
            auto &pointC = points[pointIdC];

            pointTriangles[pointIdA].insert( {f, ( pointB - pointA ).Ang( pointC - pointA )} );
            pointTriangles[pointIdB].insert( {f, ( pointA - pointB ).Ang( pointC - pointB )} );
            pointTriangles[pointIdC].insert( {f, ( pointA - pointC ).Ang( pointB - pointC )} );
        }

        auto normalsf = std::move( normals );
        normals.resize( points.size() );

        for( size_t p = 0; p < points.size(); ++p )
        {
            for( auto [f, k] : pointTriangles[p] )
            {
                normals[p] += normalsf[f] * k;
            }
            normals[p] = normals[p].Normal();
        }

        for( auto &face : faces )
        {
            auto &edgeA = edges[face.p.a];
            auto &edgeB = edges[face.p.b];
            auto &edgeC = edges[face.p.c];

            auto pointA = edgeA.s;
            auto pointB = edgeB.s;
            auto pointC = edgeC.s;

            face.n.a = pointA;
            face.n.b = pointB;
            face.n.c = pointC;
        }
    }
}

void Mesh::normalize()
{
    for( auto &n : normals )
        n = n.Normal();
}

void Mesh::optimize()
{
    Bitset usedEdges, usedNormals, usedUV, usedPoints;

    usedEdges.resize( edges.size() );
    usedNormals.resize( normals.size() );
    usedUV.resize( uv.size() );
    usedPoints.resize( points.size() );

    for( const auto &f : faces )
    {
        usedEdges.set( f.p.a );
        usedEdges.set( f.p.b );
        usedEdges.set( f.p.c );

        usedNormals.set( f.n.a );
        usedNormals.set( f.n.b );
        usedNormals.set( f.n.c );

        usedUV.set( f.uv.a );
        usedUV.set( f.uv.b );
        usedUV.set( f.uv.c );
    }

    for( const auto &e : edges )
    {
        usedPoints.set( e.s );
        usedPoints.set( e.f );
    }

    DiscreteFunction pointRemap;
    pointRemap.squishEmptySpace( usedPoints );

    DiscreteFunction normalRemap;
    normalRemap.squishEmptySpace( usedNormals );

    DiscreteFunction uvRemap;
    uvRemap.squishEmptySpace( usedUV );

    DiscreteFunction edgeRemap;
    edgeRemap.squishEmptySpace( usedEdges );

    points = pointRemap( points );
    normals = normalRemap( normals );
    uv = uvRemap( uv );
    edges = edgeRemap( edges );

    for( auto &f : faces )
    {
        f.p.a = edgeRemap( f.p.a );
        f.p.b = edgeRemap( f.p.b );
        f.p.c = edgeRemap( f.p.c );

        f.n.a = normalRemap( f.n.a );
        f.n.b = normalRemap( f.n.b );
        f.n.c = normalRemap( f.n.c );

        f.uv.a = uvRemap( f.uv.a );
        f.uv.b = uvRemap( f.uv.b );
        f.uv.c = uvRemap( f.uv.c );
    }

    for( auto &e : edges )
    {
        e.s = pointRemap( e.s );
        e.f = pointRemap( e.f );
    }
}

bool Mesh::sortFacesByGroup( int id )
{
    auto s = groups.group( id );
    if( !s )
        return false;

    Sorter sorter( *s, faces );
    auto& f = sorter();

    if( !f )
        return false;

    if( groups.o )
    {
        for( auto& [name, bitset] : *groups.o )
            bitset = f( bitset );
    }

    if( groups.g )
    {
        for( auto& [name, bitset] : *groups.g )
            bitset = f( bitset );
    }

    if( groups.m )
    {
        for( auto& [name, bitset] : *groups.m )
            bitset = f( bitset );
    }

    faces = f( faces );
    return true;
}

void Mesh::transform( const Affine3D &f )
{
    for( auto &p : points )
        p = f( p );
}

void Mesh::transform( const std::function<void( Vector3D & )> &f )
{
    for( auto &p : points )
        f( p );
}

void Mesh::clear()
{
    points.clear();
    normals.clear();
    uv.clear();
    edges.clear();
    faces.clear();
    groups.clear();
}

const std::vector<Vector3D> &Mesh::getPoints() const
{
    return points;
}

const std::vector<Vector3D> &Mesh::getNormals() const
{
    return normals;
}

const std::vector<Vector3D> &Mesh::getUVs() const
{
    return uv;
}

const std::vector<Mesh::Edge> &Mesh::getEdges() const
{
    return edges;
}

const std::vector<Mesh::Face> &Mesh::getFaces() const
{
    return faces;
}

const Mesh::Groups &Mesh::getGroups() const
{
    return groups;
}

// https://en.wikipedia.org/wiki/Wavefront_.obj_file

bool Mesh::input( const std::filesystem::path &path, std::filesystem::path *materials )
{
    try
    {
        std::ifstream file( path, std::ios::binary );
        Scanner s( file, path.generic_wstring() );

        clear();

        if( materials )
            materials->clear();

        Bitset *o = nullptr, *g = nullptr, *m = nullptr;

        auto get = [&s]( Bitset *&bitset, std::optional<std::map<std::wstring, Bitset>> &map )
        {
            if( !map )
                return;
            bitset = &map->emplace( ( std::wstring )s.token.s, Bitset() ).first->second;
        };

        while( s.token.t != Scanner::Nil )
        {
            s.token.error( Scanner::Name );

            if( s.token.s == "mtllib" )
            {
                s.getLine();

                if( materials )
                {
                    std::wstring string;
                    if( !s.token.s.EncodeW( string ) )
                        return false;

                    std::filesystem::path secondary = string;
                    *materials = secondary.is_absolute() ? secondary : path.parent_path() / secondary;
                }

                s.getToken();

                continue;
            }

            if( s.token.s == "o" )
            {
                s.getLine();
                get( o, groups.o );
                s.getToken();

                continue;
            }

            if( s.token.s == "g" )
            {
                s.getLine();
                get( g, groups.g );
                s.getToken();

                continue;
            }

            if( s.token.s == "usemtl" )
            {
                s.getToken();
                get( m, groups.m );
                s.getToken();

                continue;
            }

            if( s.token.s == "s" )
            {
                s.getToken();
                // Smooth shading: on / off / 0 / 1
                s.getToken();

                continue;
            }

            auto getVector = [&]( Vector3D & v )
            {
                s.getToken();
                v.x = s.token.x;
                s.token.error( Scanner::Real );

                s.getToken();
                v.y = s.token.x;
                s.token.error( Scanner::Real );

                s.getToken();
                if( s.token.t == Scanner::Real )
                {
                    v.z = s.token.x;
                    s.getToken();
                }
                else
                {
                    v.z = 0;
                }

                return true;
            };

            if( s.token.s == "v" )
            {
                Vector3D v;
                if( !getVector( v ) )
                    return false;
                points.push_back( v );

                continue;
            }

            if( s.token.s == "vt" )
            {
                Vector3D vt;
                if( !getVector( vt ) )
                    return false;
                uv.push_back( vt );

                continue;
            }

            if( s.token.s == "vn" )
            {
                Vector3D vn;
                if( !getVector( vn ) )
                    return false;
                normals.push_back( vn );

                continue;
            }

            if( s.token.s == "vp" )
            {
                Vector3D vp;
                if( !getVector( vp ) )
                    return false;

                continue;
            }

            if( s.token.s == "l" )
            {
                s.getToken();
                // Polyline
                while( s.token.t == Scanner::Int )
                {
                    s.getToken();
                }

                continue;
            }

            if( s.token.s == "f" )
            {
                std::vector<std::tuple<size_t, std::optional<size_t>, std::optional<size_t>>> vertices;

                s.getToken();

                while( s.token.t == Scanner::Int )
                {
                    std::optional<size_t> normal, texture;
                    bool tex = false, norm = false;

                    size_t point = s.token.n - 1;
                    s.getToken();

                    if( s.token.t == Scanner::Slash )
                    {
                        s.getToken();

                        tex = s.token.t != Scanner::Slash;
                        norm = !tex;

                        if( tex )
                        {
                            s.token.error( Scanner::Int );

                            texture = s.token.n - 1;
                            s.getToken();

                            norm = s.token.t == Scanner::Slash;
                            if( norm )
                            {
                                s.getToken();
                            }
                        }
                        else
                        {
                            s.getToken();
                        }

                        if( norm )
                        {
                            s.token.error( Scanner::Int );

                            normal = s.token.n - 1;
                            s.getToken();
                        }
                    }

                    vertices.emplace_back( point, normal, texture );
                }

                if( vertices.size() < 3 )
                    return false;

                bool tex = true;

                auto getTexture = [this, &tex]( const auto & vertex )
                {
                    if( std::get<2>( vertex ).has_value() && *std::get<2>( vertex ) < uv.size() )
                        return *std::get<2>( vertex );

                    if( tex )
                    {
                        uv.push_back( Vector3D( 0, 0, 0 ) );
                        tex = false;
                    }

                    return uv.size() - 1;
                };

                auto getNormal = [this]( const auto & vertex, size_t e0, size_t e1 )
                {
                    if( std::get<1>( vertex ).has_value() && *std::get<1>( vertex ) < normals.size() )
                        return *std::get<1>( vertex );

                    const auto &edge0 = edges[e0];
                    const auto &edge1 = edges[e1];

                    const auto &p0 = points[edge0.s];
                    const auto &p1 = points[edge0.f];
                    const auto &p2 = points[edge1.f];

                    auto n = normals.size();
                    normals.push_back( ( p1 - p0 ).M( p2 - p1 ).Normal() );
                    return n;
                };

                for( size_t i = 1; i < vertices.size() - 1; ++i )
                {
                    const auto &v0 = vertices[0];
                    const auto &v1 = vertices[i];
                    const auto &v2 = vertices[( i + 1 ) % vertices.size()];
                    if( std::get<0>( v0 ) >= points.size() || std::get<0>( v1 ) >= points.size() || std::get<0>( v2 ) >= points.size() )
                        return false;

                    auto e0 = edges.size();
                    edges.push_back( Edge{ std::get<0>( v0 ), std::get<0>( v1 ) } );

                    auto e1 = edges.size();
                    edges.push_back( Edge{ std::get<0>( v1 ), std::get<0>( v2 ) } );

                    auto e2 = edges.size();
                    edges.push_back( Edge{ std::get<0>( v2 ), std::get<0>( v0 ) } );

                    if( o )
                        o->set( faces.size() );
                    if( g )
                        g->set( faces.size() );
                    if( m )
                        m->set( faces.size() );
                    faces.push_back( Face{ Triplet{ e0, e1, e2 },
                                           Triplet{ getNormal( v0, e0, e1 ), getNormal( v1, e1, e2 ), getNormal( v2, e2, e0 ) },
                                           Triplet{ getTexture( v0 ), getTexture( v1 ), getTexture( v2 ) } } );
                }

                continue;
            }

            s.token.error( L"Unknown command." );
        }
    }
    catch( ... )
    {
        return false;
    }

    return true;
}

bool Mesh::output( const std::filesystem::path &path, std::filesystem::path *materials ) const
{
    String data;

    if( materials )
        data << materials->wstring() << L"\n";

    data << L"o Mesh\n";

    for( auto &v : points )
        data << L"v " << v.x << L" " << v.y << L" " << v.z << L"\n";

    for( auto &v : normals )
        data << L"vn " << v.x << L" " << v.y << L" " << v.z << L"\n";

    for( auto &v : uv )
        data << L"vt " << v.x << L" " << v.y << L" " << v.z << L"\n";

    for( auto &face : faces )
    {
        auto &edgeA = edges[face.p.a];
        auto &edgeB = edges[face.p.b];
        auto &edgeC = edges[face.p.c];

        auto pointA = edgeA.s + 1;
        auto pointB = edgeB.s + 1;
        auto pointC = edgeC.s + 1;

        auto uvA = face.uv.a + 1;
        auto uvB = face.uv.b + 1;
        auto uvC = face.uv.c + 1;

        auto normalA = face.n.a + 1;
        auto normalB = face.n.b + 1;
        auto normalC = face.n.c + 1;

        data << L"f ";
        data << pointA << L"/" << uvA << L"/" << normalA << L" ";
        data << pointB << L"/" << uvB << L"/" << normalB << L" ";
        data << pointC << L"/" << uvC << L"/" << normalC << L"\n";
    }

    size_t pos = 0;
    std::vector<uint8_t> fileData;
    if( !data.EncodeUtf8( fileData, pos, true ) )
        return false;

    std::ofstream file( path, std::ios::binary );
    if( !file )
        return false;

    file.write( ( const char * )fileData.data(), fileData.size() );
    return true;
}

Mesh::Data<Vector3D> Mesh::operator[]( size_t id ) const
{
    makeException( id < faces.size() );

    auto &f = faces[id];
    V3<Edge> e{ edges[f.p.a], edges[f.p.b], edges[f.p.c] };
    Va3<Vector3D> p{ points[e.a.s], points[e.b.s], points[e.c.s] };
    Va3<Vector3D> t{ points[f.uv.a], points[f.uv.b], points[f.uv.c] };
    Va3<Vector3D> n{ points[f.n.a], points[f.n.b], points[f.n.c] };
    return { f, e, p, n, t };
}

Mesh::Data<Vector3D&> Mesh::operator[]( size_t id )
{
    makeException( id < faces.size() );

    auto &f = faces[id];
    V3<Edge> e{ edges[f.p.a], edges[f.p.b], edges[f.p.c] };
    Va3<Vector3D&> p{ points[e.a.s], points[e.b.s], points[e.c.s] };
    Va3<Vector3D&> t{ points[f.uv.a], points[f.uv.b], points[f.uv.c] };
    Va3<Vector3D&> n{ points[f.n.a], points[f.n.b], points[f.n.c] };
    return { f, e, p, n, t };
}

Mesh::Iterator<Mesh> Mesh::begin()
{
    return Iterator<Mesh>( *this, 0 );
}

Mesh::Iterator<Mesh> Mesh::end()
{
    return Iterator<Mesh>( *this, faces.size() );
}

Mesh::Iterator<const Mesh> Mesh::begin() const
{
    return Iterator<const Mesh>( *this, 0 );
}

Mesh::Iterator<const Mesh> Mesh::end() const
{
    return Iterator<const Mesh>( *this, faces.size() );
}

static bool getOptions( Scanner &s, Surface::Options &options, String &filePathSufix )
{
    auto getBool = [&]( bool & value )
    {
        s.getToken();
        if( s.token.t == Scanner::Name )
        {
            if( s.token.s == "on" )
            {
                value = true;
                s.getToken();
                return true;
            }
            if( s.token.s == "off" )
            {
                value = false;
                s.getToken();
                return true;
            }
            return false;
        }
        if( s.token.t == Scanner::Int )
        {
            if( s.token.n == 1 )
            {
                value = true;
                s.getToken();
                return true;
            }
            if( s.token.n == 0 )
            {
                value = false;
                s.getToken();
                return true;
            }
            return false;
        }
        return false;
    };

    auto getTriplet = [&]( Vector3D & value )
    {
        // u [v [w]]
        s.getToken();
        if( s.token.t != Scanner::Int && s.token.t != Scanner::Real )
            return false;

        value.x = s.token.x;

        s.getToken();
        if( s.token.t != Scanner::Int && s.token.t != Scanner::Real )
            return true;

        value.y = s.token.x;

        s.getToken();
        if( s.token.t != Scanner::Int && s.token.t != Scanner::Real )
            return true;

        value.z = s.token.x;

        s.getToken();
        return true;
    };

    options.clear();

    s.getToken();

    while( true )
    {
        filePathSufix.Clear();
        filePathSufix << s.token.s;
        // Options start with '-'
        // If no '-', then nothing to parse
        if( s.token.t != Scanner::Minus )
            return true;

        filePathSufix.Clear();

        // consume '-'
        s.getToken();

        if( s.token.t != Scanner::Name )
            return false;

        // parse option name

        if( s.token.s == "blendu" )
        {
            // set horizontal texture blending
            if( getBool( options.blendu ) )
                continue;
            break;
        }

        if( s.token.s == "blendv" )
        {
            // set vertical texture blending
            if( getBool( options.blendv ) )
                continue;
            break;
        }

        if( s.token.s == "boost" )
        {
            // boost mip-map sharpness

            s.getToken();
            if( s.token.t != Scanner::Int && s.token.t != Scanner::Real )
                break;

            options.boost = s.token.x;

            if( options.boost < 0 )
                break;

            s.getToken();
            continue;
        }

        if( s.token.s == "mm" )
        {
            // Modify texture map values

            s.getToken();
            if( s.token.t != Scanner::Int && s.token.t != Scanner::Real )
                break;

            options.mm.brightness = s.token.x;

            s.getToken();
            if( s.token.t != Scanner::Int && s.token.t != Scanner::Real )
                break;

            options.mm.contrast = s.token.x;

            s.getToken();
            continue;
        }

        if( s.token.s == "o" )
        {
            // Origin offset
            if( getTriplet( options.o ) )
                continue;
            break;
        }

        if( s.token.s == "s" )
        {
            // Scale
            if( getTriplet( options.s ) )
                continue;
            break;
        }

        if( s.token.s == "t" )
        {
            // Turbulence
            if( getTriplet( options.t ) )
                continue;
            break;
        }

        if( s.token.s == "texres" )
        {
            // Texture resolution to create

            s.getToken();
            if( s.token.t != Scanner::Int || s.token.n < 1 )
                break;

            options.texres = s.token.n;

            if( options.texres < 1 )
                break;

            s.getToken();
            continue;
        }

        if( s.token.s == "clamp" )
        {
            // Only render texels in the clamped 0-1 range
            // when unclamped, textures are repeated across a surface,
            // when clamped, only texels which fall within the 0-1 range are rendered

            if( getBool( options.clamp ) )
                continue;
            break;
        }

        if( s.token.s == "bm" )
        {
            // Bump multiplier (for bump maps only)

            s.getToken();
            if( s.token.t != Scanner::Int && s.token.t != Scanner::Real )
                break;

            options.bm = s.token.x;

            s.getToken();
            continue;
        }

        if( s.token.s == "imfchan" )
        {
            // Specifies which channel of the file is used to create a scalar or bump texture
            // (the default for bump is 'l' and for decal is 'm')
            // r:red | g:green | b:blue | m:matte | l:luminance | z:z-depth

            s.getToken();
            if( s.token.t != Scanner::Name )
                break;

            std::wstring value;
            if( !s.token.s.EncodeW( value ) )
                break;

            options.imfchan = value;

            s.getToken();
            continue;
        }

        if( s.token.s == "type" )
        {
            // Specifies a type for a reflection map
            // when using a cube map, the texture file for each side of the cube is specified separately
            // sphere | cube_top | cube_bottom | cube_front  | cube_back | cube_left | cube_right

            s.getToken();
            if( s.token.t != Scanner::Name )
                break;

            std::wstring value;
            if( !s.token.s.EncodeW( value ) )
                break;

            options.type = value;

            s.getToken();
            continue;
        }

        // If we get here, the option name was not recognized
        return false;
    }

    return false;
}

static bool getMap( const std::filesystem::path &root, const wchar_t *name, Scanner &s, bool &pass, std::optional<Surface::Texture> &map, const wchar_t *altName = nullptr )
{
    if( pass )
        return true;

    if( s.token.s != name && ( !altName || s.token.s != altName ) )
    {
        pass = false;
        return true;
    }

    map.emplace();

    String string;
    if( !getOptions( s, map->options, string ) )
        return false;

    s.getLine();
    string << s.token.s;

    std::wstring wstring;
    if( !string.EncodeW( wstring ) )
        return false;

    std::filesystem::path path( wstring );

    map->texture = path.is_absolute() ? path : root / path;
    s.getToken();

    pass = true;
    return true;
}

static void getScalar( const wchar_t *name, Scanner &s, bool &pass, double &scalar, const wchar_t *altName = nullptr )
{
    if( pass )
        return;

    bool f = !altName || s.token.s != altName;

    if( s.token.s != name && f )
    {
        pass = false;
        return;
    }

    s.getToken();
    s.token.error( Scanner::Real );

    // Only altName used for scalar is Tr (for d)
    scalar = f ? s.token.x : 1 - s.token.x;
    s.getToken();

    pass = true;
}

static void getIndex( const wchar_t *name, Scanner &s, bool &pass, unsigned &index, const wchar_t *altName = nullptr )
{
    if( pass )
        return;

    if( s.token.s != name && ( !altName || s.token.s != altName ) )
    {
        pass = false;
        return;
    }

    s.getToken();
    s.token.error( Scanner::Int );

    index = s.token.n;
    s.getToken();

    pass = true;
}

static void getVector( const wchar_t *name, Scanner &s, bool &pass, Vector3D &vector, const wchar_t *altName = nullptr )
{
    if( pass )
        return;

    if( s.token.s != name && ( !altName || s.token.s != altName ) )
    {
        pass = false;
        return;
    }

    s.getToken();
    vector.x = s.token.x;
    s.token.error( Scanner::Real );

    s.getToken();
    vector.y = s.token.x;
    s.token.error( Scanner::Real );

    s.getToken();
    vector.z = s.token.x;
    s.token.error( Scanner::Real );

    s.getToken();

    makeException( vector.x < 0 || vector.x > 1 || vector.y < 0 || vector.y > 1 || vector.z < 0 || vector.z > 1 );

    pass = true;
}

// Illumination model's index:
// 0: Color on and Ambient off
// 1: Color on and Ambient on
// 2: Highlight on
// 3: Reflection on and Ray trace on
// 4: Transparency: Glass on, Reflection: Ray trace on
// 5: Reflection: Fresnel on and Ray trace on
// 6: Transparency: Refraction on, Reflection: Fresnel off and Ray trace on
// 7: Transparency: Refraction on, Reflection: Fresnel on and Ray trace on
// 8: Reflection on and Ray trace off
// 9: Transparency: Glass on, Reflection: Ray trace off
// 10: Casts shadows onto invisible surfaces

Surface::Options::Options()
{
    clear();
}

void Surface::Options::clear()
{
    mm.brightness = 0.0;
    mm.contrast = 1.0;

    blendu = true;
    blendv = true;
    clamp = false;

    o = Vector3D( 0.0, 0.0, 0.0 );
    s = Vector3D( 1.0, 1.0, 1.0 );
    t = Vector3D( 0.0, 0.0, 0.0 );

    imfchan = L"";
    type = L"";

    boost = -1.0;
    bm = 1.0;

    texres = -1;
}

Surface::Texture::Texture()
{
    clear();
}

void Surface::Texture::clear()
{
    texture.clear();
    options.clear();
}

Surface::Material::Material()
{
    clear();
}

void Surface::Material::clear()
{
    map_ns.reset();
    map_ka.reset();
    map_kd.reset();
    map_ks.reset();
    map_ke.reset();
    map_d.reset();
    bump.reset();
    disp.reset();
    decal.reset();
    refl.reset();

    ka = Vector3D( 0.02, 0.02, 0.02 );
    kd = Vector3D( 0.60, 0.60, 0.60 );
    ks = Vector3D( 0.80, 0.80, 0.80 );
    ke = Vector3D( 0.01, 0.01, 0.01 );

    ns = 30.0;
    ni = 1.0;
    d = 1.0;

    illum = 2;
}

const Surface::Texture *Surface::Material::get( int i ) const
{
    if( i == 0 && map_ns )
        return &*map_ns;
    else if( i == 1 && map_ka )
        return &*map_ka;
    else if( i == 2 && map_kd )
        return &*map_kd;
    else if( i == 3 && map_ks )
        return &*map_ks;
    else if( i == 4 && map_ke )
        return &*map_ke;
    else if( i == 5 && map_d )
        return &*map_d;
    else if( i == 6 && bump )
        return &*bump;
    else if( i == 7 && disp )
        return &*disp;
    else if( i == 8 && decal )
        return &*decal;
    else if( i == 9 && refl )
        return &*refl;
    return nullptr;
}

Surface::Surface()
{}

Surface::~Surface()
{}

void Surface::clear()
{
    materials.clear();
}

bool Surface::input( const std::filesystem::path &path )
{
    try
    {
        std::ifstream file;
        file.open( path, std::ios::binary );

        Scanner s( file, path.generic_wstring() );

        auto root = path.parent_path();

        clear();

        while( s.token.t != Scanner::Nil )
        {
            s.token.error( Scanner::Name );

            if( s.token.s != "newmtl" )
                return false;

            s.getLine();

            std::wstring mtl;
            if( !s.token.s.EncodeW( mtl ) )
                return false;

            s.getToken();

            auto& material = materials.emplace( mtl, Surface::Material() ).first->second;

            bool pass;
            while( s.token.t != Scanner::Nil )
            {
                pass = false;

                s.token.error( Scanner::Name );

                getScalar( L"Ns", s, pass, material.ns ); // Specular exponent (Shininess)
                getScalar( L"Ni", s, pass, material.ni ); // Refractive index
                getIndex( L"illum", s, pass, material.illum ); // Illumination model's index
                getVector( L"Ka", s, pass, material.ka ); // Color of material for ambient lighting
                getVector( L"Kd", s, pass, material.kd ); // Color of material for diffuse reflection
                getVector( L"Ks", s, pass, material.ks ); // Color of material for specular reflection
                getVector( L"Ke", s, pass, material.ke ); // Color of material for emission
                getScalar( L"d", s, pass, material.d, L"Tr" ); // Opaqueness
                getMap( root, L"map_Ns", s, pass, material.map_ns ); // Specular exponent texture
                getMap( root, L"map_Ka", s, pass, material.map_ka ); // Texture of material for ambient lighting
                getMap( root, L"map_Kd", s, pass, material.map_kd ); // Texture of material for diffuse reflection
                getMap( root, L"map_Ks", s, pass, material.map_ks ); // Texture of material for specular reflection
                getMap( root, L"map_Ke", s, pass, material.map_ke ); // Texture of material for emission
                getMap( root, L"map_D", s, pass, material.map_d, L"map_d" ); // Opaqueness texture
                getMap( root, L"bump", s, pass, material.bump, L"map_bump" ); // Effect is like embossing the surface with the texture
                getMap( root, L"disp", s, pass, material.disp ); // Same as bump, but it modifies actual geometry
                getMap( root, L"decal", s, pass, material.decal ); // Layered on top of main texture to create stickers/markings/logos/labels
                getMap( root, L"refl", s, pass, material.refl ); // A reflection of environment in a material
                if( pass )
                    continue;
                break;
            }

            if( 0 > material.ns || material.ns > 1000 )
                return false;

            if( 0 >= material.ni || material.ni > 10 )
                return false;

            if( 0 > material.d || material.d > 1 )
                return false;

            if( material.illum > 10 )
                return false;
        }
    }
    catch( ... )
    {
        return false;
    }

    return true;
}

bool Surface::output( const std::filesystem::path & path ) const
{
    String data;

    for( const auto &matPair : materials )
    {
        const auto &name = matPair.first;
        const auto &mat = matPair.second;

        data << "newmtl " << name << "\n";
        data << "Ka " << mat.ka.x << " " << mat.ka.y << " " << mat.ka.z << "\n";
        data << "Kd " << mat.kd.x << " " << mat.kd.y << " " << mat.kd.z << "\n";
        data << "Ks " << mat.ks.x << " " << mat.ks.y << " " << mat.ks.z << "\n";
        data << "Ke " << mat.ke.x << " " << mat.ke.y << " " << mat.ke.z << "\n";
        data << "Ns " << mat.ns << "\n";
        data << "Ni " << mat.ni << "\n";
        data << "Tr " << mat.d << "\n";
        data << "illum " << mat.illum << "\n";

        auto writeTexture = [&]( const std::string & prefix, const std::optional<Texture> &texture )
        {
            if( !texture )
                return;

            const auto &t = *texture;
            const auto &opt = t.options;

            data << prefix << " ";

            data << "-blendu " << ( opt.blendu ? "on " : "off " );
            data << "-blendv " << ( opt.blendv ? "on " : "off " );
            data << "-clamp " << ( opt.clamp ? "on " : "off " );
            data << "-boost " << opt.boost << " ";
            data << "-bm " << opt.bm << " ";
            data << "-texres " << opt.texres << " ";

            if( !opt.imfchan.empty() )
                data << "-imfchan " << opt.imfchan << " ";

            if( !opt.type.empty() )
                data << "-type " << opt.type << " ";

            data << "-mm " << opt.mm.brightness << " " << opt.mm.contrast << " ";
            data << "-o " << opt.o.x << " " << opt.o.y << " " << opt.o.z << " ";
            data << "-s " << opt.s.x << " " << opt.s.y << " " << opt.s.z << " ";
            data << "-t " << opt.t.x << " " << opt.t.y << " " << opt.t.z << " ";

            data << t.texture.wstring() << "\n";
        };

        writeTexture( "map_Ka", mat.map_ka );
        writeTexture( "map_Kd", mat.map_kd );
        writeTexture( "map_Ks", mat.map_ks );
        writeTexture( "map_Ke", mat.map_ke );
        writeTexture( "map_d", mat.map_d );
        writeTexture( "map_Ns", mat.map_ns );
        writeTexture( "bump", mat.bump );
        writeTexture( "disp", mat.disp );
        writeTexture( "decal", mat.decal );
        writeTexture( "refl", mat.refl );

        data << "\n";
    }

    size_t pos = 0;
    std::vector<uint8_t> fileData;
    if( !data.EncodeUtf8( fileData, pos, true ) )
        return false;

    std::ofstream file( path, std::ios::binary );
    if( !file )
        return false;

    file.write( ( const char * )fileData.data(), fileData.size() );
    return true;
}
