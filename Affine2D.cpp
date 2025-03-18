#include "Affine2D.h"

Affine2D::Affine2D() {}

Affine2D::Affine2D( const Affine2D &a )
{
    s = a.s;
    t = a.t;
}

Affine2D::Affine2D( const Vector2D &shift ): t( Matrix2D::Identity() ), s( shift ) {}

Affine2D::Affine2D( const Matrix2D &transformation ): t( transformation ), s() {}

Affine2D::Affine2D( const Matrix2D &transformation, const Vector2D &shift ): t( transformation ), s( shift ) {}

Affine2D Affine2D::operator*( const Affine2D &a ) const
{
    Affine2D r( *this );
    r *= a;
    return r;
}

Affine2D &Affine2D::operator*=( const Affine2D &a )
{
    s += t * a.s;
    t *= a.t;
    return *this;
}

Vector2D Affine2D::operator()( const Vector2D &a ) const
{
    return t * a + s;
}

bool Affine2D::operator==( const Affine2D &a ) const
{
    return ( s == a.s ) && ( t == a.t );
}

bool Affine2D::operator!=( const Affine2D &a ) const
{
    return ( s != a.s ) && ( t != a.t );
}

Vector2D operator*( const Matrix2D &a, const Vector2D &b )
{
    return Vector2D( a.a00 * b.x + a.a01 * b.y, a.a10 * b.x + a.a11 * b.y );
}
