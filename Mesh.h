#pragma once

#include <filesystem>
#include <functional>
#include <vector>
#include <map>

#include "Information.h"
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
        bool operator<( const Edge& other ) const
        {
            return s < other.s || ( s == other.s && f < other.f );
        };
    };

    struct Triplet
    {
        // Indexes
        size_t a, b, c;
    };

    struct Face
    {
        // Positions of faces' edges in `edges`, normals in `normals`, texture coordinates in `texturing`
        // Next edge always have common point with previous, start of an edge is finish of a previous edge
        Triplet p, n, t;
    };

    using Group = std::map<std::wstring, Bitset>;

    struct Groups
    {
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

    void cube(); // Unit cube
    void plane( size_t rows, size_t columns ); // Subdivided unit plane
    void prism( const std::vector<Vector2D>& base ); // Prism of height one
    void torus( double a, double b, size_t n, size_t m ); // Torus of radius `a` (Circle is approximated by `n` sided polygon) with section of radius 'b' (Circle is approximated by `m` sided polygon)
    void sphereUV( size_t n, size_t m ); // Unit UV sphere
    void sphereIco( size_t n ); // Unit ico sphere

    Mesh extract( const Bitset &faceSet ) const;

    void remakeNormals( bool faceNormals );
    void normalize();
    void optimize();
    void invert();
    bool sortFacesByGroup( int id );

    // Applies transformation to the mesh
    void transform( const Affine3D &f );
    void setPoints( const std::function<void( Vector3D & point, const std::vector<size_t>& faces )>& f );
    void setNormals( const std::function<void( Vector3D & normal, const std::vector<size_t>& faces )>& f );
    void setTexturing( const std::function<void( Vector3D & texture, const std::vector<size_t>& faces )>& f );
    void setPoints( const std::function<void( Vector3D & point )>& f );
    void setNormals( const std::function<void( Vector3D & normal )>& f );
    void setTexturing( const std::function<void( Vector3D & texture )>& f );

    void clear();

    std::optional<std::filesystem::path>& getMaterialsFile();
    std::optional<Information::Item>& getMaterials();

    const std::optional<std::filesystem::path>& getMaterialsFile() const;
    const std::optional<Information::Item>& getMaterials() const;

    const std::vector<Vector3D> &getPoints() const;
    const std::vector<Vector3D> &getNormals() const;
    const std::vector<Vector3D> &getTexturing() const;
    const std::vector<Edge> &getEdges() const;
    const std::vector<Face> &getFaces() const;
    const Groups &getGroups() const;

    bool input( const std::filesystem::path &path );
    bool output( const std::filesystem::path &path ) const;

    // Access
    template<typename V>
    struct V3
    {
        V a, b, c;
    };

    template<typename V>
    struct Va3 : public V3<V>
    {
        std::remove_reference_t<V> operator()( double u, double v ) const
        {
            return V3<V>::a * ( 1.0 - u - v ) + V3<V>::b * u + V3<V>::c * v;
        }
    };

    template<typename V>
    struct Data
    {
    public:
        using Type = V;

        Face f;
        V3<Edge> e;
        Va3<V> p, n, t;
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
    // Lists of points, normals, texturing and edges can contain unused entities
    // List of faces can NOT contain unused faces
    // Order of these entities in their containers is irrelevant

    std::optional<std::filesystem::path> materialsFile;
    std::optional<Information::Item> materials;

    std::vector<Vector3D> points, normals, texturing;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    Groups groups;
};
