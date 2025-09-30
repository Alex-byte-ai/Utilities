#pragma once

#include <filesystem>
#include <functional>
#include <vector>
#include <map>

#include "Vector2D.h"
#include "Vector3D.h"
#include "Affine3D.h"
#include "Bitset.h"

class Mesh
{
public:
    struct Edge
    {
        // Positions of edge's ends (start, finish) in points
        // Order matters, edges, that only differ in order, can both be stored in `edges`
        size_t s, f;
    };

    struct Triplet
    {
        // Indexes
        size_t a, b, c;
    };

    struct Face
    {
        // Positions of face's edges in `edges`, texture coordinates in `uv`, normals in `normals`
        // Next edge always have common point with previous, start of an edge is finish of a previous edge

        Triplet p, n, uv;
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

        void clear();
    };

    Mesh( Groups grps = Groups( false, false, false ) );
    Mesh( const Mesh &other );
    Mesh( Mesh &&other );

    Mesh &operator=( const Mesh &other );
    Mesh &operator=( Mesh &&other );

    std::optional<size_t> intersectSegment( const Vector3D &p0, const Vector3D &p1, double &u, double &v, double &t ) const;

    void cube(); // Sets mesh to be a unit cube
    void plane( size_t rows, size_t columns ); // Sets mesh to be a subdivided unit plane
    void prism( const std::vector<Vector2D>& base ); // Generates a prism of height one

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

    // Access
    template<typename V>
    struct V3
    {
        V a, b, c;
    };

    template<typename V>
    struct Va3
    {
        V a, b, c;

        std::remove_reference_t<V> operator()( double u, double v ) const
        {
            return a * ( 1.0 - u - v ) + b * u + c * v;
        }
    };

    template<typename V>
    struct Data
    {
    public:
        using Type = V;

        Face f;
        V3<Edge> e;
        Va3<V> p, n, uv;
    };

    template<typename M>
    class Iterator
    {
    private:
        M &mesh;
        size_t id;
    public:
        Iterator( M &m, size_t i ) : mesh( m ), id( i )
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

        Data<copyConst<M, Vector3D&>> operator*() const
        {
            return mesh[id];
        }
    };

    Data<Vector3D> operator[]( size_t id ) const;
    Data<Vector3D&> operator[]( size_t id );

    Iterator<Mesh> begin();
    Iterator<Mesh> end();

    Iterator<const Mesh> begin() const;
    Iterator<const Mesh> end() const;
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
            double brightness, contrast;
        };

        std::wstring imfchan, type;
        bool blendu, blendv, clamp;
        long long int texres;
        double boost, bm;
        Vector3D o, s, t;
        Mm mm;

        Options();
        void clear();
    };

    struct Texture
    {
        std::filesystem::path texture;
        Options options;

        Texture();
        void clear();
    };

    struct Material
    {
        std::optional<Texture> map_ns, map_ka, map_kd, map_ks, map_ke, map_d, bump, disp, decal, refl;
        Vector3D ka, kd, ks, ke;
        double ns, ni, d;
        unsigned illum;

        Material();
        void clear();

        const Texture *get( int i ) const;
    };

    Surface();
    ~Surface();

    void clear();

    std::map<std::wstring, Material> materials;

    bool input( const std::filesystem::path &path );
    bool output( const std::filesystem::path &path ) const;
};
