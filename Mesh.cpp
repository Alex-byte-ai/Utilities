#include "Mesh.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "Exception.h"
#include "BasicMath.h"
#include "Scanner.h"

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
        return f[i];
    }

    Bitset operator()( const Bitset &bitset ) const
    {
        Bitset result;
        result.resize( f.size() );
        for( size_t i = 0; i < f.size(); ++i )
            result.set( bitset.test( f[i] ) );
        return result;
    };

    template<typename T>
    std::vector<T> operator()( const std::vector<T> &vector ) const
    {
        std::vector<T> result;
        result.reserve( f.size() );
        for( size_t i = 0; i < f.size(); ++i )
            result.emplace_back( vector[f[i]] );
        return result;
    };
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

Mesh::Iterator::Iterator( Mesh &mesh_, size_t id_ ) : mesh( mesh_ ), id( id_ )
{}

Mesh::Iterator &Mesh::Iterator::operator++()
{
    ++id;
    return *this;
}

bool Mesh::Iterator::operator==( const Iterator &other ) const
{
    return id == other.id;
}

bool Mesh::Iterator::operator!=( const Iterator &other ) const
{
    return id != other.id;
}

Mesh::Data Mesh::Iterator::operator*() const
{
    return mesh[id];
}

Mesh::ConstIterator::ConstIterator( const Mesh &mesh_, size_t id_ ) : mesh( mesh_ ), id( id_ )
{}

Mesh::ConstIterator &Mesh::ConstIterator::operator++()
{
    ++id;
    return *this;
}

bool Mesh::ConstIterator::operator==( const ConstIterator &other ) const
{
    return id == other.id;
}

bool Mesh::ConstIterator::operator!=( const ConstIterator &other ) const
{
    return id != other.id;
}

Mesh::ConstData Mesh::ConstIterator::operator*() const
{
    return mesh[id];
}

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

Mesh::Mesh( Groups grps )
    : groups( std::move( grps ) )
{}

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
    groups = other.groups;
    points = other.points;
    normals = other.normals;
    uv = other.uv;
    edges = other.edges;
    faces = other.faces;
    return*this;
}

Mesh &Mesh::operator=( Mesh &&other )
{
    groups = std::move( other.groups );
    points = std::move( other.points );
    normals = std::move( other.normals );
    uv = std::move( other.uv );
    edges = std::move( other.edges );
    faces = std::move( other.faces );
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

Mesh::Data Mesh::operator[]( size_t faceId )
{
    makeException( faceId < faces.size() );

    auto &face = faces[faceId];

    auto &edgeA = edges[face.p.a];
    auto &edgeB = edges[face.p.b];
    auto &edgeC = edges[face.p.c];

    auto &pointA = points[edgeA.s];
    auto &pointB = points[edgeB.s];
    auto &pointC = points[edgeC.s];

    auto &uvA = uv[face.uv.a];
    auto &uvB = uv[face.uv.b];
    auto &uvC = uv[face.uv.c];

    auto &normalA = normals[face.n.a];
    auto &normalB = normals[face.n.b];
    auto &normalC = normals[face.n.c];

    return Data( Triangle( pointA, pointB, pointC ), Triangle( normalA, normalB, normalC ), Triangle( uvA, uvB, uvC ) );
}

Mesh::ConstData Mesh::operator[]( size_t faceId ) const
{
    makeException( faceId < faces.size() );

    auto &face = faces[faceId];

    auto &edgeA = edges[face.p.a];
    auto &edgeB = edges[face.p.b];
    auto &edgeC = edges[face.p.c];

    auto &pointA = points[edgeA.s];
    auto &pointB = points[edgeB.s];
    auto &pointC = points[edgeC.s];

    auto &uvA = uv[face.uv.a];
    auto &uvB = uv[face.uv.b];
    auto &uvC = uv[face.uv.c];

    auto &normalA = normals[face.n.a];
    auto &normalB = normals[face.n.b];
    auto &normalC = normals[face.n.c];

    return ConstData( ConstTriangle( pointA, pointB, pointC ), ConstTriangle( normalA, normalB, normalC ), ConstTriangle( uvA, uvB, uvC ) );
}

Mesh::Iterator Mesh::begin()
{
    return Iterator( *this, 0 );
}

Mesh::Iterator Mesh::end()
{
    return Iterator( *this, faces.size() );
}

Mesh::ConstIterator Mesh::begin() const
{
    return ConstIterator( *this, 0 );
}

Mesh::ConstIterator Mesh::end() const
{
    return ConstIterator( *this, faces.size() );
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

    // Helper to add one triangle and its UVs
    auto addTri = [&]( size_t i0, size_t i1, size_t i2, size_t u0, size_t u1, size_t u2, size_t n )
    {
        size_t e0 = edges.size();
        edges.push_back( {i0, i1} );

        size_t e1 = edges.size();
        edges.push_back( {i1, i2} );

        size_t e2 = edges.size();
        edges.push_back( {i2, i0} );

        faces.push_back( Face( Triplet( e0, e1, e2 ), Triplet( n, n, n ), Triplet( u0, u1, u2 ) ) );
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

    // Build points and UVs
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

    auto addTri = [&]( size_t i0, size_t i1, size_t i2 )
    {
        size_t e0 = edges.size();
        edges.push_back( {i0, i1} );

        size_t e1 = edges.size();
        edges.push_back( {i1, i2} );

        size_t e2 = edges.size();
        edges.push_back( {i2, i0} );

        // UV indices are same as point indices
        faces.push_back( Face( Triplet( e0, e1, e2 ), Triplet( 0, 0, 0 ), Triplet( i0, i1, i2 ) ) );
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

Mesh Mesh::extract( const Bitset &faceSet ) const
{
    Mesh result;
    result.points = points;
    result.normals = normals;
    result.uv = uv;

    DiscreteFunction f;
    f.squishEmptySpace( faceSet );
    result.faces = f( faces );
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
}

bool Mesh::sortFacesByGroup( int id )
{
    auto s = groups.group( id );
    if( !s )
        return false;

    Sorter sorter( *s, faces );
    if( !sorter() )
        return false;

    auto newFaces = sorter()( faces );

    if( groups.o )
    {
        for( auto& [name, bitset] : *groups.o )
            bitset = sorter()( bitset );
    }

    if( groups.g )
    {
        for( auto& [name, bitset] : *groups.g )
            bitset = sorter()( bitset );
    }

    if( groups.m )
    {
        for( auto& [name, bitset] : *groups.m )
            bitset = sorter()( bitset );
    }

    faces = std::move( newFaces );
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
    uv.clear();
    edges.clear();
    faces.clear();
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
    std::ifstream file( path, std::ios::binary );
    Scanner s( file, path.generic_wstring() );
    String e;

    clear();

    if( materials )
        materials->clear();

    Bitset *o = nullptr, *g = nullptr, *m = nullptr;

    auto get = [&s]( Bitset *&bitset, std::optional<std::map<std::wstring, Bitset>> &map )
    {
        if( !map )
            return true;
        std::wstring name;
        if( !s.token.s.EncodeW( name ) )
            return false;
        bitset = &map->emplace( name, Bitset() ).first->second;
        return true;
    };

    while( s.token.t != Scanner::Nil )
    {
        if( s.token.error( e ) )
            return false;

        if( s.token.error( e, Scanner::Name ) )
            return false;

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
            if( !get( o, groups.o ) )
                return false;
            s.getToken();

            continue;
        }

        if( s.token.s == "g" )
        {
            s.getLine();
            if( !get( g, groups.g ) )
                return false;
            s.getToken();

            continue;
        }

        if( s.token.s == "usemtl" )
        {
            s.getToken();
            if( !get( m, groups.m ) )
                return false;
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
            if( s.token.error( e, Scanner::Real ) )
                return false;

            s.getToken();
            v.y = s.token.x;
            if( s.token.error( e, Scanner::Real ) )
                return false;

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
                        if( s.token.error( e, Scanner::Int ) )
                            return false;

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
                        if( s.token.error( e, Scanner::Int ) )
                            return false;

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
                edges.push_back( Edge( std::get<0>( v0 ), std::get<0>( v1 ) ) );

                auto e1 = edges.size();
                edges.push_back( Edge( std::get<0>( v1 ), std::get<0>( v2 ) ) );

                auto e2 = edges.size();
                edges.push_back( Edge( std::get<0>( v2 ), std::get<0>( v0 ) ) );

                if( o )
                    o->set( faces.size() );
                if( g )
                    g->set( faces.size() );
                if( m )
                    m->set( faces.size() );
                faces.push_back( Face(
                                     Triplet( e0, e1, e2 ),
                                     Triplet( getNormal( v0, e0, e1 ), getNormal( v1, e1, e2 ), getNormal( v2, e2, e0 ) ),
                                     Triplet( getTexture( v0 ), getTexture( v1 ), getTexture( v2 ) )
                                 ) );
            }

            continue;
        }

        if( s.token.error( e, L"Unknown command." ) )
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
        if( s.token.t != Scanner::Int || s.token.t != Scanner::Real )
            return false;

        value.x = s.token.x;

        s.getToken();
        if( s.token.t != Scanner::Int || s.token.t != Scanner::Real )
            return true;

        value.y = s.token.x;

        s.getToken();
        if( s.token.t != Scanner::Int || s.token.t != Scanner::Real )
            return true;

        value.z = s.token.x;

        s.getToken();
        return true;
    };

    s.getToken();

    while( true )
    {
        filePathSufix.Clear();
        filePathSufix << s.token.s;
        if( s.token.t != Scanner::Minus )
            return true;

        filePathSufix.Clear();

        s.getToken();
        if( s.token.t != Scanner::Name )
            return false;

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
            if( s.token.t != Scanner::Int || s.token.t != Scanner::Real || s.token.x < 0 )
                break;

            options.boost = s.token.x;

            s.getToken();
            continue;
        }

        if( s.token.s == "mm" )
        {
            // modify texture map values

            s.getToken();
            if( s.token.t != Scanner::Int || s.token.t != Scanner::Real )
                break;

            options.mm.brightness = s.token.x;

            s.getToken();
            if( s.token.t != Scanner::Int || s.token.t != Scanner::Real )
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
            // texture resolution to create

            s.getToken();
            if( s.token.t != Scanner::Int || s.token.n < 1 )
                break;

            options.texres = s.token.n;

            s.getToken();
            continue;
        }

        if( s.token.s == "clamp" )
        {
            // only render texels in the clamped 0-1 range (default off)
            // when unclamped, textures are repeated across a surface,
            // when clamped, only texels which fall within the 0-1
            // range are rendered.

            if( getBool( options.clamp ) )
                continue;
            break;
        }

        if( s.token.s == "bm" )
        {
            // bump multiplier (for bump maps only)

            s.getToken();
            if( s.token.t != Scanner::Int || s.token.t != Scanner::Real )
                break;

            options.bm = s.token.x;

            s.getToken();
            continue;
        }

        if( s.token.s == "imfchan" )
        {
            // specifies which channel of the file is used to
            // create a scalar or bump texture. r:red, g:green,
            // b:blue, m:matte, l:luminance, z:z-depth..
            // (the default for bump is 'l' and for decal is 'm')
            // r | g | b | m | l | z

            s.getToken();
            if( s.token.t != Scanner::Name )
                break;

            std::wstring value;
            if( !s.token.s.EncodeW( value ) )
                return false;

            options.imfchan = value;

            s.getToken();
            continue;
        }

        if( s.token.s == "type" )
        {
            // specifies a type for a "refl" reflection map
            // when using a cube map, the texture file for each
            // side of the cube is specified separately
            // sphere | cube_top | cube_bottom | cube_front  | cube_back | cube_left | cube_right

            s.getToken();
            if( s.token.t != Scanner::Name )
                break;

            std::wstring value;
            if( !s.token.s.EncodeW( value ) )
                return false;

            options.type = value;

            s.getToken();
            continue;
        }
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

static bool getScalar( const wchar_t *name, Scanner &s, bool &pass, double &scalar, const wchar_t *altName = nullptr )
{
    if( pass )
        return true;

    bool f = !altName || s.token.s != altName;

    if( s.token.s != name && f )
    {
        pass = false;
        return true;
    }

    String e;

    s.getToken();
    if( s.token.error( e, Scanner::Real ) )
        return false;

    // Only altName used for scalar is Tr (for d)
    scalar = altName || f ? s.token.x : 1 - s.token.x;
    s.getToken();

    pass = true;
    return true;
}

static bool getIndex( const wchar_t *name, Scanner &s, bool &pass, unsigned &index, const wchar_t *altName = nullptr )
{
    if( pass )
        return true;

    if( s.token.s != name && ( !altName || s.token.s != altName ) )
    {
        pass = false;
        return true;
    }

    String e;

    s.getToken();
    if( s.token.error( e, Scanner::Int ) )
        return false;

    index = s.token.n;
    s.getToken();

    pass = true;
    return true;
}

static bool getVector( const wchar_t *name, Scanner &s, bool &pass, Vector3D &vector, const wchar_t *altName = nullptr )
{
    if( pass )
        return true;

    if( s.token.s != name && ( !altName || s.token.s != altName ) )
    {
        pass = false;
        return true;
    }

    String e;

    s.getToken();
    vector.x = s.token.x;
    if( s.token.error( e, Scanner::Real ) )
        return false;

    s.getToken();
    vector.y = s.token.x;
    if( s.token.error( e, Scanner::Real ) )
        return false;

    s.getToken();
    vector.z = s.token.x;
    if( s.token.error( e, Scanner::Real ) )
        return false;

    s.getToken();

    pass = true;
    return true;
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

bool Surface::input( const std::filesystem::path &path )
{
    std::ifstream file;
    file.open( path, std::ios::binary );

    Scanner s( file, path.generic_wstring() );
    String e;

    auto root = path.parent_path();

    materials.clear();

    while( s.token.t != Scanner::Nil )
    {
        if( s.token.error( e ) )
            return false;

        if( s.token.error( e, Scanner::Name ) )
            return false;

        if( s.token.s != "newmtl" )
            return false;

        s.getLine();

        std::wstring mtl;
        if( !s.token.s.EncodeW( mtl ) )
            return false;

        s.getToken();

        bool pass = false;
        auto& material = materials.emplace( mtl, Surface::Material() ).first->second;

        while( s.token.t != Scanner::Nil )
        {
            if( s.token.error( e, Scanner::Name ) )
                return false;

            if( !getScalar( L"Ns", s, pass, material.ns ) ) // Specular exponent (Shininess)
                return false;
            if( !getScalar( L"Ni", s, pass, material.ni ) ) // Refractive index
                return false;
            if( !getIndex( L"illum", s, pass, material.illum ) ) // Illumination model's index
                return false;
            if( !getVector( L"Ka", s, pass, material.ka ) ) // Color of material for ambient lighting
                return false;
            if( !getVector( L"Kd", s, pass, material.kd ) ) // Color of material for diffuse reflection
                return false;
            if( !getVector( L"Ks", s, pass, material.ks ) ) // Color of material for specular reflection
                return false;
            if( !getVector( L"Ke", s, pass, material.ke ) ) // Color of material for emission
                return false;
            if( !getScalar( L"d", s, pass, material.tr, L"Tr" ) ) // Opaqueness
                return false;
            if( !getMap( root, L"map_Ns", s, pass, material.map_ns ) ) // Specular exponent texture
                return false;
            if( !getMap( root, L"map_Ka", s, pass, material.map_ka ) ) // Texture of material for ambient lighting
                return false;
            if( !getMap( root, L"map_Kd", s, pass, material.map_kd ) ) // Texture of material for diffuse reflection
                return false;
            if( !getMap( root, L"map_Ks", s, pass, material.map_ks ) ) // Texture of material for specular reflection
                return false;
            if( !getMap( root, L"map_Ke", s, pass, material.map_ke ) ) // Texture of material for emission
                return false;
            if( !getMap( root, L"map_D", s, pass, material.map_d, L"map_d" ) ) // Opaqueness texture
                return false;
            if( !getMap( root, L"bump", s, pass, material.bump, L"map_bump" ) ) // Effect is like embossing the surface with the texture
                return false;
            if( !getMap( root, L"disp", s, pass, material.disp ) ) // Same as bump, but it modifies actual geometry
                return false;
            if( !getMap( root, L"decal", s, pass, material.decal ) ) // Layered on top of main texture to create stickers/markings/logos/labels
                return false;
            if( !getMap( root, L"refl", s, pass, material.refl ) ) // A reflection of environment in a material
                return false;
            if( pass )
            {
                pass = false;
                continue;
            }
            break;
        }
    }
    return true;
}

bool Surface::output( const std::filesystem::path & ) const
{
    return false;
}
