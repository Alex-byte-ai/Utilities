#include "Affine3D.h"

Affine3D::Affine3D() {}

Affine3D::Affine3D( const Affine3D &other )
{
    s = other.s;
    t = other.t;
}

Affine3D::Affine3D( const Vector3D &shift ): t( Matrix3D::Identity() ), s( shift ) {}

Affine3D::Affine3D( const Matrix3D &transformation ): t( transformation ), s() {}

Affine3D::Affine3D( const Matrix3D &transformation, const Vector3D &shift ): t( transformation ), s( shift ) {}

Affine3D &Affine3D::operator=( const Affine3D &other )
{
    s = other.s;
    t = other.t;
    return *this;
}

Affine3D Affine3D::operator*( const Affine3D &a ) const
{
    Affine3D r( *this );
    r *= a;
    return r;
}



Affine3D &Affine3D::operator*=( const Affine3D &a )
{
    s += t * a.s;
    t *= a.t;
    return *this;
}

Vector3D Affine3D::operator()( const Vector3D &a ) const
{
    return t * a + s;
}

bool Affine3D::operator==( const Affine3D &a ) const
{
    return ( s == a.s ) && ( t == a.t );
}

bool Affine3D::operator!=( const Affine3D &a ) const
{
    return ( s != a.s ) || ( t != a.t );
}

Affine3D Affine3D::inv() const
{
    auto tInv = t.inv();
    return Affine3D( tInv, -tInv * s );
}
