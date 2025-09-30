#include "Vector4D.h"

#include "Basic.h"

double Vector4D::epsilon = 1e-6;

Vector4D::Vector4D(): x( 0 ), y( 0 ), z( 0 ), w( 0 ) {}

Vector4D::Vector4D( double x_, double y_, double z_, double w_ ): x( x_ ), y( y_ ), z( z_ ), w( w_ ) {}

Vector4D Vector4D::operator+() const
{
    return Vector4D( +x, +y, +z, +w );
}

Vector4D Vector4D::operator+( const Vector4D &a ) const
{
    return Vector4D( x + a.x, y + a.y, z + a.z, w + a.w );
}

Vector4D &Vector4D::operator+=( const Vector4D &a )
{
    x += a.x;
    y += a.y;
    z += a.z;
    w += a.w;
    return *this;
}

Vector4D Vector4D::operator-() const
{
    return Vector4D( -x, -y, -z, -w );
}

Vector4D Vector4D::operator-( const Vector4D &a ) const
{
    return Vector4D( x - a.x, y - a.y, z - a.z, w - a.w );
}

Vector4D &Vector4D::operator-=( const Vector4D &a )
{
    x -= a.x;
    y -= a.y;
    z -= a.z;
    w -= a.w;
    return *this;
}

Vector4D Vector4D::operator*( double k ) const
{
    return Vector4D( x * k, y * k, z * k, w * k );
}

Vector4D &Vector4D::operator*=( double k )
{
    x *= k;
    y *= k;
    z *= k;
    w *= k;
    return *this;
}

Vector4D Vector4D::operator/( double k ) const
{
    return Vector4D( x / k, y / k, z / k, w / k );
}

Vector4D &Vector4D::operator/=( double k )
{
    x /= k;
    y /= k;
    z /= k;
    w /= k;
    return *this;
}

double Vector4D::operator*( const Vector4D &a ) const
{
    return x * a.x + y * a.y + z * a.z + w * a.w;
}

bool Vector4D::operator==( const Vector4D &a ) const
{
    return ( ::Abs( x - a.x ) <= epsilon ) && ( ::Abs( y - a.y ) <= epsilon )
           && ( ::Abs( z - a.z ) <= epsilon ) && ( ::Abs( w - a.w ) <= epsilon );
}

bool Vector4D::operator!=( const Vector4D &a ) const
{
    return ( ::Abs( x - a.x ) > epsilon ) || ( ::Abs( y - a.y ) > epsilon )
           || ( ::Abs( z - a.z ) > epsilon ) || ( ::Abs( w - a.w ) > epsilon );
}

double Vector4D::Sqr() const
{
    return ( *this ) * ( *this );
}

double Vector4D::Abs() const
{
    return Sqrt( Sqr() );
}

Vector4D Vector4D::Normal() const
{
    double l = Abs();
    if( l > 0 )
        return ( *this ) / l;
    return *this;
}

double Vector4D::Ang( const Vector4D &a ) const
{
    return ArcCos( Normal() * a.Normal() );
}

Vector4D operator*( double k, const Vector4D &a )
{
    return a * k;
}
