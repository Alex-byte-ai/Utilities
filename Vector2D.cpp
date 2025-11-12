#include "Vector2D.h"

#include "Basic.h"

double Vector2D::epsilon = 1e-6;

Vector2D::Vector2D(): x( 0 ), y( 0 ) {}

Vector2D::Vector2D( double x_, double y_ ): x( x_ ), y( y_ ) {}

Vector2D Vector2D::operator+()const
{
    return Vector2D( +x, +y );
}

Vector2D Vector2D::operator+( const Vector2D &a ) const
{
    return Vector2D( x + a.x, y + a.y );
}

Vector2D &Vector2D::operator+=( const Vector2D &a )
{
    x += a.x;
    y += a.y;
    return *this;
}

Vector2D Vector2D::operator-()const
{
    return Vector2D( -x, -y );
}

Vector2D Vector2D::operator-( const Vector2D &a ) const
{
    return Vector2D( x - a.x, y - a.y );
}

Vector2D &Vector2D::operator-=( const Vector2D &a )
{
    x -= a.x;
    y -= a.y;
    return *this;
}

Vector2D Vector2D::operator*( double k ) const
{
    return Vector2D( x * k, y * k );
}

Vector2D &Vector2D::operator*=( double k )
{
    x *= k;
    y *= k;
    return *this;
}

Vector2D Vector2D::operator/( double k ) const
{
    return Vector2D( x / k, y / k );
}

Vector2D &Vector2D::operator/=( double k )
{
    x /= k;
    y /= k;
    return *this;
}

double Vector2D::operator*( const Vector2D &a ) const
{
    return x * a.x + y * a.y;
}

bool Vector2D::operator==( const Vector2D &a ) const
{
    return ( ::Abs( x - a.x ) <= epsilon ) && ( ::Abs( y - a.y ) <= epsilon );
}

bool Vector2D::operator!=( const Vector2D &a ) const
{
    return ( ::Abs( x - a.x ) > epsilon ) || ( ::Abs( y - a.y ) > epsilon );
}

double Vector2D::M( const Vector2D &a ) const
{
    return x * a.y - y * a.x;
}

Vector2D Vector2D::L() const
{
    return Vector2D( -y, x );
}

double Vector2D::Sqr()const
{
    return ( *this ) * ( *this );
}

double Vector2D::Abs() const
{
    return Sqrt( Sqr() );
}

Vector2D Vector2D::Normal() const
{
    double l = Abs();
    if( l > 0 )
        return ( *this ) / l;
    return *this;
}

double Vector2D::Ang( const Vector2D &a ) const
{
    return ArcCos( Normal() * a.Normal() );
}

std::optional<Vector2D> Vector2D::IJ( const Vector2D &i, const Vector2D &j ) const
{
    double d, da, db;
    d =  i.x * j.y - i.y * j.x;
    da =   x * j.y -   y * j.x;
    db = i.x *   y - i.y *   x;

    if( ::Abs( d ) > epsilon * epsilon )
        return Vector2D( da / d, db / d );
    return {};
}

Vector2D operator*( double k, const Vector2D &a )
{
    return a * k;
}
