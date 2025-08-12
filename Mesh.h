#pragma once

#include <filesystem>
#include <functional>
#include <vector>
#include <map>

#include "Vector3D.h"
#include "Affine3D.h"
#include "Bitset.h"

class Mesh
{
public:
    class Edge
    {
    public:
        // Positions of edge's ends (start, finish) in points
        // Order matters, edges, that only differ in order, can both be stored in `edges`
        size_t s, f;

        Edge( size_t start, size_t finish ) : s( start ), f( finish )
        {}
    };

    class Triplet
    {
    public:
        // Indexes
        size_t a, b, c;

        Triplet( size_t a_, size_t b_, size_t c_ ) : a( a_ ), b( b_ ), c( c_ )
        {}
    };

    class Face
    {
    public:
        // Positions of face's edges in `edges`, texture coordinates in `uv`, normals in `normals`
        // Next edge always have common point with previous, start of an edge is finish of a previous edge

        Triplet p, n, uv;

        Face( const Triplet &points, const Triplet &normals, const Triplet &texture ) : p( points ), n( normals ), uv( texture )
        {}
    };

    class Triangle
    {
    public:
        Vector3D &a, &b, &c;

        Triangle( Vector3D &a_, Vector3D &b_, Vector3D &c_ ) : a( a_ ), b( b_ ), c( c_ )
        {}

        Vector3D operator()( double u, double v ) const
        {
            return a * ( 1.0 - u - v ) + b * u + c * v;
        }
    };

    class Data
    {
    public:
        Triangle p, n, uv;

        Data( const Triangle &points, const Triangle &normals, const Triangle &texture ) : p( points ), n( normals ), uv( texture )
        {}
    };

    class ConstTriangle
    {
    public:
        const Vector3D &a, &b, &c;

        ConstTriangle( const Vector3D &a_, const Vector3D &b_, const Vector3D &c_ ) : a( a_ ), b( b_ ), c( c_ )
        {}

        Vector3D operator()( double u, double v ) const
        {
            return a * ( 1.0 - u - v ) + b * u + c * v;
        }
    };

    class ConstData
    {
    public:
        ConstTriangle p, n, uv;

        ConstData( const ConstTriangle &points, const ConstTriangle &normals, const ConstTriangle &texture ) : p( points ), n( normals ), uv( texture )
        {}
    };

    // Iterates through all faces, provides access

    class Iterator
    {
    private:
        Mesh &mesh;
        size_t id;

        Iterator( Mesh &mesh, size_t id );
    public:
        Iterator &operator++();

        bool operator==( const Iterator &other ) const;
        bool operator!=( const Iterator &other ) const;

        Data operator*() const;

        friend Mesh;
    };

    class ConstIterator
    {
    private:
        const Mesh &mesh;
        size_t id;

        ConstIterator( const Mesh &mesh, size_t id );
    public:
        ConstIterator &operator++();

        bool operator==( const ConstIterator &other ) const;
        bool operator!=( const ConstIterator &other ) const;

        ConstData operator*() const;

        friend Mesh;
    };

    using Group = std::map<std::wstring, Bitset>;

    class Groups
    {
    public:
        std::optional<Group> o, g, m;

        Groups( bool f0, bool f1, bool f2 );
        Groups( const Groups& other );
        Groups( Groups&& other );

        Groups &operator=( const Groups &other );
        Groups &operator=( Groups &&other );

        Group *group( int id );
    };

    Mesh( Groups grps = Groups( false, false, false ) );
    Mesh( const Mesh &other );
    Mesh( Mesh &&other );

    Mesh &operator=( const Mesh &other );
    Mesh &operator=( Mesh &&other );

    std::optional<size_t> intersectSegment( const Vector3D &p0, const Vector3D &p1, double &u, double &v, double &t ) const;

    Data operator[]( size_t faceId );
    ConstData operator[]( size_t faceId ) const;

    Iterator begin();
    Iterator end();

    ConstIterator begin() const;
    ConstIterator end() const;

    void cube(); // Sets mesh to be a unit cube
    void plane( size_t rows, size_t columns ); // Sets mesh to be a subdivided unit plane

    Mesh extract( const Bitset &faceSet ) const;

    void remakeNormals( bool faceNormals );
    void normalize();
    void optimize();
    bool sortFacesByGroup( int id );

    // Applies transformation to all the points of the mesh
    void transform( const Affine3D &f );
    void transform( const std::function<void( Vector3D & )> &f );

    void clear();

    const std::vector<Vector3D> &getPoints() const;
    const std::vector<Vector3D> &getNormals() const;
    const std::vector<Vector3D> &getUVs() const;
    const std::vector<Edge> &getEdges() const;
    const std::vector<Face> &getFaces() const;
    const Groups &getGroups() const;

    bool input( const std::filesystem::path &path, std::filesystem::path *materials = nullptr );
    bool output( const std::filesystem::path &path, std::filesystem::path *materials = nullptr ) const;

private:
    // All faces are counterclockwise directed
    // Lists of points, normals, uv and edges can contain unused entities
    // List of faces can NOT contain unused faces
    // Order of these entities in their containers is irrelevant
    std::vector<Vector3D> points, normals, uv;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    Groups groups;
};

class Surface
{
public:
    struct Options
    {
        struct Mm
        {
            double brightness = 0, contrast = 1;
        };

        bool blendu = true, blendv = true, clamp = false;
        Vector3D o{0, 0, 0}, s{1, 1, 1}, t{0, 0, 0};
        std::wstring imfchan, type;
        double boost = -1, bm = 1;
        long long int texres = -1;
        Mm mm;
    };

    struct Texture
    {
        std::filesystem::path texture;
        Options options;
    };

    struct Material
    {
        std::optional<Texture> map_ns, map_ka, map_kd, map_ks, map_ke, map_d, bump, disp, decal, refl;
        Vector3D ka{ 1.0, 1.0, 1.0 }, kd{ 1.0, 1.0, 1.0 }, ks{ 1.0, 1.0, 1.0 }, ke{ 0.0, 0.0, 0.0 };
        double ns = 30.0, ni = 1, tr = 1.0;
        unsigned illum = 2;
    };

    std::map<std::wstring, Material> materials;

    bool input( const std::filesystem::path &path );
    bool output( const std::filesystem::path &path ) const;
};
