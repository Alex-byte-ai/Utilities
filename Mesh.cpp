#include "Mesh.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "Exception.h"
#include "Scanner.h"
#include "Polygon.h"
#include "Basic.h"

// ---------------- Helpers ----------------

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

// ---------------- Material ----------------

static void getValue( Scanner &s, Information::Wrapper& value, bool numeric )
{
    s.token.error( Scanner::Name );
    auto key = ( std::wstring )s.token.s;
    s.getToken();

    Information::Array v;
    while( true )
    {
        Information::Item item;
        if( s.token.t == Scanner::Int )
        {
            item = s.token.n;
            v.push( item );
            s.getToken();
        }
        else if( s.token.t == Scanner::Real )
        {
            item = s.token.x;
            v.push( item );
            s.getToken();
        }
        else if( !numeric && s.token.t == Scanner::Name )
        {
            if( s.token.s == L"on" )
            {
                item = true;
                v.push( item );
                s.getToken();
            }
            else if( s.token.s == L"off" )
            {
                item = false;
                v.push( item );
                s.getToken();
            }
            else
            {
                item = ( std::wstring )s.token.s;
                v.push( item );
                s.getToken();
            }
        }
        else
            break;
    }

    if( v.size() > 1 )
    {
        value( key ) = std::move( v );
    }
    else if( v.size() == 1 )
    {
        value( key ) = std::move( v[0] );
    }
}

static void getOptions( Scanner &s, Information::Wrapper& texture, Unicode::String &filePathSufix, std::optional<std::wstring>& type )
{
    type.reset();

    s.getToken();

    while( true )
    {
        filePathSufix.Clear();
        filePathSufix << s.token.s;
        // Options start with '-'
        // If no '-', then nothing to parse
        if( s.token.t != Scanner::Minus )
            return;

        filePathSufix.Clear();

        // Consume '-'
        s.getToken();

        s.token.error( Scanner::Name );

        bool isType = s.token.s == L"type";

        getValue( s, texture, false );

        if( isType )
        {
            type = ( std::wstring )texture( L"type" ).as<Information::String>();
            texture( L"type" ) = Information::Null();
        }
    }

    makeException( false );
}

static void getMap( const std::filesystem::path &root, std::wstring name, Scanner &s, Information::Wrapper& material )
{
    Information::Item textureItem;
    Information::Wrapper texture( textureItem );

    Unicode::String string;
    std::optional<std::wstring> type;
    getOptions( s, texture, string, type );

    if( type )
    {
        makeException( name == L"refl" );
        name += *type;
    }

    s.getLine();
    string << s.token.s;
    std::filesystem::path path( ( std::wstring )string );

    auto map = path.is_absolute() ? path : root / path;
    texture( L"map" ) = map.wstring();
    material( name ) = std::move( textureItem );
    s.getToken();
}

static void getMaterials( const std::filesystem::path &path, Information::Wrapper& materials )
{
    static const std::set<std::wstring> mapNames{L"map_Ns", L"map_Ka", L"map_Kd", L"map_Ks", L"map_Ke", L"map_D", L"map_d", L"bump", L"map_bump", L"disp", L"decal", L"refl"};

    std::ifstream file;
    file.open( path, std::ios::binary );

    Scanner s( file, path.generic_wstring() );

    auto root = path.parent_path();

    materials = Information::Array();

    while( s.token.t != Scanner::Nil )
    {
        s.token.error( Scanner::Name );
        makeException( s.token.s == "newmtl" );

        s.getLine();
        s.getToken();

        Information::Item item;
        item = Information::Object();
        item( L"name" ) = ( std::wstring )s.token.s;
        Information::Wrapper material( item( L"material" ) = Information::Object() );

        while( s.token.t != Scanner::Nil )
        {
            s.token.error( Scanner::Name );

            if( s.token.s == "newmtl" )
                break;

            auto key = ( std::wstring )s.token.s;
            if( mapNames.count( key ) )
            {
                getMap( root, std::move( key ), s, material );
            }
            else
            {
                getValue( s, material, true );
            }
        }

        materials.as<Information::Array>().push( std::move( item ) );
    }

    static unsigned id = 0;
    materials.output( L"output/" + std::to_wstring( id++ ) + L".txt" );
}

static bool setMaterials( const std::filesystem::path &path, const Information::Item& materials )
{
    Unicode::String data;

    for( const auto &sample : materials.as<Information::Array>() )
    {
        const auto &name = sample( L"name" ).as<Information::String>();
        const auto &mat = sample( L"material" ).as<Information::Object>();

        auto writeTexture = [&]( const wchar_t *prefix, const std::optional<std::wstring> &type = {} )
        {
            if( !mat.exists( prefix ) )
                return;

            const auto &t = mat( prefix ).as<Information::Object>();

            auto putBool = [&]( const std::wstring & key )
            {
                if( t.exists( key ) )
                {
                    const auto& value = t( key );
                    data << L"-" << key << L" " << ( value.as<bool>() ? L"on" : L"off" ) << L" ";
                }
            };

            auto putValue = [&]( const std::wstring & key )
            {
                if( t.exists( key ) )
                {
                    const auto& value = t( key );
                    data << L"-" << key << L" " << value.as<long double>() << L" ";
                }
            };

            auto putIndex = [&]( const std::wstring & key )
            {
                if( t.exists( key ) )
                {
                    const auto& value = t( key );
                    data << L"-" << key << L" " << value.as<long long int>() << L" ";
                }
            };

            auto putVector = [&]( const std::wstring & key )
            {
                if( t.exists( key ) )
                {
                    const auto& vector = t( key );
                    data << L"-" <<  key << L" " << vector( L"x" ).as<long double>() << L" " << vector( L"y" ).as<long double>() << L" " << vector( L"z" ).as<long double>() << L"\n";
                }
            };

            auto putString = [&]( const std::wstring & key )
            {
                if( t.exists( key ) )
                {
                    const auto& string = t( key );
                    data << L"-" <<  key << L" " << string.as<Information::String>() << L"\n";
                }
            };

            auto putMm = [&]( const std::wstring & key )
            {
                if( t.exists( key ) )
                {
                    const auto& mm = t( key );
                    data << L"-" <<  key << L" " << mm( L"brightness" ).as<Information::String>() << L" " << mm( L"contrast" ).as<Information::String>() << L"\n";
                }
            };

            data << prefix << L" ";

            putBool( L"blendu" );
            putBool( L"blendv" );
            putBool( L"clamp" );
            putValue( L"boost" );

            if( std::wstring( L"bump" ) == prefix && mat.exists( L"bm" ) )
                data << L"-bm " << mat( L"bm" ).as<long double>() << L" ";

            putIndex( L"texres" );
            putString( L"imfchan" );

            if( type )
                data << L"-type " << *type << L" ";

            putMm( L"mm" );
            putVector( L"o" );
            putVector( L"s" );
            putVector( L"t" );

            data << t( L"map" ).as<Information::String>() << L"\n";
        };

        auto putColor = [&]( const std::wstring & key )
        {
            if( mat.exists( key ) )
            {
                const auto& color = mat( key );
                data << key << L" " << color( L"x" ).as<long double>() << L" " << color( L"y" ).as<long double>() << L" " << color( L"z" ).as<long double>() << L"\n";
            }
        };

        auto putValue = [&]( const std::wstring & key )
        {
            if( mat.exists( key ) )
            {
                const auto& value = mat( key );
                data << key << L" " << value.as<long double>() << L"\n";
            }
        };

        auto putIndex = [&]( const std::wstring & key )
        {
            if( mat.exists( key ) )
            {
                const auto& value = mat( key );
                data << key << L" " << value.as<long long int>() << L"\n";
            }
        };

        data << L"newmtl " << name << L"\n";

        putColor( L"Ka" );
        putColor( L"Kd" );
        putColor( L"Ks" );
        putColor( L"Ke" );
        putValue( L"Ns" );
        putValue( L"Ni" );
        putValue( L"Tr" );
        putIndex( L"illum" );

        writeTexture( L"map_Ka" );
        writeTexture( L"map_Kd" );
        writeTexture( L"map_Ks" );
        writeTexture( L"map_Ke" );
        writeTexture( L"map_d" );
        writeTexture( L"map_Ns" );
        writeTexture( L"bump", {} );
        writeTexture( L"disp" );
        writeTexture( L"decal" );
        writeTexture( L"refl", L"sphere" );
        writeTexture( L"refl", L"cube_top" );
        writeTexture( L"refl", L"cube_bottom" );
        writeTexture( L"refl", L"cube_front" );
        writeTexture( L"refl", L"cube_back" );
        writeTexture( L"refl", L"cube_left" );
        writeTexture( L"refl", L"cube_right" );

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

// ---------------- Mesh ----------------

// https://en.wikipedia.org/wiki/Wavefront_.obj_file

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
    materialsFile( other.materialsFile ),
    points( other.points ),
    normals( other.normals ),
    texturing( other.texturing ),
    edges( other.edges ),
    faces( other.faces ),
    groups( other.groups )
{}

Mesh::Mesh( Mesh &&other ) :
    materialsFile( std::move( other.materialsFile ) ),
    points( std::move( other.points ) ),
    normals( std::move( other.normals ) ),
    texturing( std::move( other.texturing ) ),
    edges( std::move( other.edges ) ),
    faces( std::move( other.faces ) ),
    groups( std::move( other.groups ) )
{}

Mesh &Mesh::operator=( const Mesh &other )
{
    materialsFile = other.materialsFile;
    points = other.points;
    normals = other.normals;
    texturing = other.texturing;
    edges = other.edges;
    faces = other.faces;
    groups = other.groups;
    return*this;
}

Mesh &Mesh::operator=( Mesh &&other )
{
    materialsFile = std::move( other.materialsFile );
    points = std::move( other.points );
    normals = std::move( other.normals );
    texturing = std::move( other.texturing );
    edges = std::move( other.edges );
    faces = std::move( other.faces );
    groups = std::move( other.groups );
    return*this;
}

std::optional<size_t> Mesh::intersectSegment( const Vector3D &p0, const Vector3D &p1, double &u, double &v, double &t ) const
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

    // A single texturing layout for every face:
    texturing =
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
            texturing.push_back( p );
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

    texturing =
    {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
    };

    for( auto& p : base )
    {
        points.emplace_back( p.x, p.y, 1.0 );
        texturing.emplace_back( width.normalize( p.x ), width.normalize( p.y ), 0 );
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
    result.texturing = texturing;
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
    Bitset usedEdges, usedNormals, usedTexturing, usedPoints;

    usedEdges.resize( edges.size() );
    usedNormals.resize( normals.size() );
    usedTexturing.resize( texturing.size() );
    usedPoints.resize( points.size() );

    for( const auto &f : faces )
    {
        usedEdges.set( f.p.a );
        usedEdges.set( f.p.b );
        usedEdges.set( f.p.c );

        usedNormals.set( f.n.a );
        usedNormals.set( f.n.b );
        usedNormals.set( f.n.c );

        usedTexturing.set( f.t.a );
        usedTexturing.set( f.t.b );
        usedTexturing.set( f.t.c );
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

    DiscreteFunction texturingRemap;
    texturingRemap.squishEmptySpace( usedTexturing );

    DiscreteFunction edgeRemap;
    edgeRemap.squishEmptySpace( usedEdges );

    points = pointRemap( points );
    normals = normalRemap( normals );
    texturing = texturingRemap( texturing );
    edges = edgeRemap( edges );

    for( auto &f : faces )
    {
        f.p.a = edgeRemap( f.p.a );
        f.p.b = edgeRemap( f.p.b );
        f.p.c = edgeRemap( f.p.c );

        f.n.a = normalRemap( f.n.a );
        f.n.b = normalRemap( f.n.b );
        f.n.c = normalRemap( f.n.c );

        f.t.a = texturingRemap( f.t.a );
        f.t.b = texturingRemap( f.t.b );
        f.t.c = texturingRemap( f.t.c );
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
    texturing.clear();
    edges.clear();
    faces.clear();
    groups.clear();
    materialsFile.reset();
}

std::optional<std::filesystem::path>& Mesh::getMaterialsFile()
{
    return materialsFile;
}

std::optional<Information::Item>& Mesh::getMaterials()
{
    if( materials )
        return materials;

    if( materialsFile )
    {
        materials.emplace();
        Information::Wrapper m( *materials );
        try
        {
            ::getMaterials( *materialsFile, m );
        }
        catch( ... )
        {
            materials.reset();
        }
    }

    return materials;
}

const std::optional<std::filesystem::path>& Mesh::getMaterialsFile() const
{
    return materialsFile;
}

const std::optional<Information::Item>& Mesh::getMaterials() const
{
    return materials;
}

const std::vector<Vector3D> &Mesh::getPoints() const
{
    return points;
}

const std::vector<Vector3D> &Mesh::getNormals() const
{
    return normals;
}

const std::vector<Vector3D> &Mesh::getTexturing() const
{
    return texturing;
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

bool Mesh::input( const std::filesystem::path &path )
{
    try
    {
        std::ifstream file( path, std::ios::binary );
        Scanner s( file, path.generic_wstring() );

        clear();

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

                if( materialsFile )
                    return false;

                std::wstring string;
                if( !s.token.s.EncodeW( string ) )
                    return false;

                std::filesystem::path secondary = string;
                materialsFile = secondary.is_absolute() ? secondary : path.parent_path() / secondary;

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
                texturing.push_back( vt );

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
                    if( std::get<2>( vertex ).has_value() && *std::get<2>( vertex ) < texturing.size() )
                        return *std::get<2>( vertex );

                    if( tex )
                    {
                        texturing.push_back( Vector3D( 0, 0, 0 ) );
                        tex = false;
                    }

                    return texturing.size() - 1;
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

bool Mesh::output( const std::filesystem::path &path ) const
{
    Unicode::String data;

    if( materialsFile )
    {
        if( materials )
        {
            ::setMaterials( *materialsFile, *materials );
        }
        else
        {
            Information::Item temporary;
            Information::Wrapper t( temporary );
            ::getMaterials( *materialsFile, t );
            ::setMaterials( *materialsFile, temporary );
        }
        data << materialsFile->wstring() << L"\n";
    }

    data << L"o Mesh\n";

    for( auto &v : points )
        data << L"v " << v.x << L" " << v.y << L" " << v.z << L"\n";

    for( auto &v : normals )
        data << L"vn " << v.x << L" " << v.y << L" " << v.z << L"\n";

    for( auto &v : texturing )
        data << L"vt " << v.x << L" " << v.y << L" " << v.z << L"\n";

    for( auto &face : faces )
    {
        auto &edgeA = edges[face.p.a];
        auto &edgeB = edges[face.p.b];
        auto &edgeC = edges[face.p.c];

        auto pointA = edgeA.s + 1;
        auto pointB = edgeB.s + 1;
        auto pointC = edgeC.s + 1;

        auto texturingA = face.t.a + 1;
        auto texturingB = face.t.b + 1;
        auto texturingC = face.t.c + 1;

        auto normalA = face.n.a + 1;
        auto normalB = face.n.b + 1;
        auto normalC = face.n.c + 1;

        data << L"f ";
        data << pointA << L"/" << texturingA << L"/" << normalA << L" ";
        data << pointB << L"/" << texturingB << L"/" << normalB << L" ";
        data << pointC << L"/" << texturingC << L"/" << normalC << L"\n";
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
    Va3<Vector3D> t{ points[f.t.a], points[f.t.b], points[f.t.c] };
    Va3<Vector3D> n{ points[f.n.a], points[f.n.b], points[f.n.c] };
    return { f, e, p, n, t };
}

Mesh::Data<Vector3D&> Mesh::operator[]( size_t id )
{
    makeException( id < faces.size() );

    auto &f = faces[id];
    V3<Edge> e{ edges[f.p.a], edges[f.p.b], edges[f.p.c] };
    Va3<Vector3D&> p{ points[e.a.s], points[e.b.s], points[e.c.s] };
    Va3<Vector3D&> t{ points[f.t.a], points[f.t.b], points[f.t.c] };
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
