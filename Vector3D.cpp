#include "Vector3D.h"

#include "Basic.h"

double Vector3D::epsilon = 1e-6;

Vector3D::Vector3D(): x( 0 ), y( 0 ) {}

Vector3D::Vector3D( double x_, double y_, double z_ ): x( x_ ), y( y_ ), z( z_ ) {}

Vector3D Vector3D::operator+()const
{
    return Vector3D( +x, +y, +z );
}

Vector3D Vector3D::operator+( const Vector3D &a ) const
{
    return Vector3D( x + a.x, y + a.y, z + a.z );
}

Vector3D &Vector3D::operator+=( const Vector3D &a )
{
    x += a.x;
    y += a.y;
    z += a.z;
    return *this;
}

Vector3D Vector3D::operator-()const
{
    return Vector3D( -x, -y, -z );
}

Vector3D Vector3D::operator-( const Vector3D &a ) const
{
    return Vector3D( x - a.x, y - a.y, z - a.z );
}

Vector3D &Vector3D::operator-=( const Vector3D &a )
{
    x -= a.x;
    y -= a.y;
    z -= a.z;
    return *this;
}

Vector3D Vector3D::operator*( double k ) const
{
    return Vector3D( x * k, y * k, z * k );
}

Vector3D &Vector3D::operator*=( double k )
{
    x *= k;
    y *= k;
    z *= k;
    return *this;
}

Vector3D Vector3D::operator/( double k ) const
{
    return Vector3D( x / k, y / k, z / k );
}

Vector3D &Vector3D::operator/=( double k )
{
    x /= k;
    y /= k;
    z /= k;
    return *this;
}

double Vector3D::operator*( const Vector3D &a ) const
{
    return x * a.x + y * a.y + z * a.z;
}

bool Vector3D::operator==( const Vector3D &a ) const
{
    return ( ::Abs( x - a.x ) <= epsilon ) && ( ::Abs( y - a.y ) <= epsilon ) && ( ::Abs( z - a.z ) <= epsilon );
}

bool Vector3D::operator!=( const Vector3D &a ) const
{
    return ( ::Abs( x - a.x ) > epsilon ) || ( ::Abs( y - a.y ) > epsilon ) || ( ::Abs( z - a.z ) > epsilon );
}

Vector3D Vector3D::M( const Vector3D &a ) const
{
    Vector3D r;
    r.x = y * a.z - a.y * z;
    r.y = z * a.x - a.z * x;
    r.z = x * a.y - a.x * y;
    return r;
}

double Vector3D::Sqr()const
{
    return ( *this ) * ( *this );
}

double Vector3D::Abs() const
{
    return Sqrt( Sqr() );
}

Vector3D Vector3D::Normal() const
{
    double l = Abs();
    if( l > 0 )
        return ( *this ) / l;
    return *this;
}

double Vector3D::Ang( const Vector3D &a ) const
{
    return ArcCos( Normal() * a.Normal() );
}

std::optional<Vector3D> Vector3D::IJK( const Vector3D &i, const Vector3D &j, const Vector3D &k ) const
{
    double d =
        i.x * ( j.y * k.z - j.z * k.y )
        - i.y * ( j.x * k.z - j.z * k.x )
        + i.z * ( j.x * k.y - j.y * k.x );

    double da =
        x   * ( j.y * k.z - j.z * k.y )
        - y   * ( j.x * k.z - j.z * k.x )
        + z   * ( j.x * k.y - j.y * k.x );

    double db =
        i.x * ( y * k.z -   z * k.y )
        - i.y * ( x * k.z -   z * k.x )
        + i.z * ( x * k.y -   y * k.x );

    double dc =
        i.x * ( j.y *    z - j.z *    y )
        - i.y * ( j.x *    z - j.z *    x )
        + i.z * ( j.x *    y - j.y *    x );

    if( ::Abs( d ) > epsilon * epsilon * epsilon )
        return Vector3D( da / d, db / d, dc / d );

    return {};
}

Vector3D operator*( double k, const Vector3D &a )
{
    return a * k;
}
