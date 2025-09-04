#include "Matrix3D.h"

#include "Exception.h"
#include "Basic.h"

double Matrix3D::epsilon = 1e-6;

Matrix3D::Matrix3D()
{
    a00 = a01 = a02 = a10 = a11 = a12 = a20 = a21 = a22 = 0;
}

Matrix3D::Matrix3D( double a00_, double a01_, double a02_, double a10_, double a11_, double a12_, double a20_, double a21_, double a22_ )
    : a00( a00_ ), a01( a01_ ), a02( a02_ ), a10( a10_ ), a11( a11_ ), a12( a12_ ), a20( a20_ ), a21( a21_ ), a22( a22_ )
{}

Matrix3D Matrix3D::operator*( const Matrix3D &a ) const
{
    return Matrix3D( a00 * a.a00 + a01 * a.a10 + a02 * a.a20,
                     a00 * a.a01 + a01 * a.a11 + a02 * a.a21,
                     a00 * a.a02 + a01 * a.a12 + a02 * a.a22,

                     a10 * a.a00 + a11 * a.a10 + a12 * a.a20,
                     a10 * a.a01 + a11 * a.a11 + a12 * a.a21,
                     a10 * a.a02 + a11 * a.a12 + a12 * a.a22,

                     a20 * a.a00 + a21 * a.a10 + a22 * a.a20,
                     a20 * a.a01 + a21 * a.a11 + a22 * a.a21,
                     a20 * a.a02 + a21 * a.a12 + a22 * a.a22 );
}

Matrix3D &Matrix3D::operator*=( const Matrix3D &a )
{
    Matrix3D r = *this * a;
    *this = r;
    return *this;
}

Matrix3D Matrix3D::operator*( double k ) const
{
    Matrix3D r;
    r.a00 = a00 * k;
    r.a01 = a01 * k;
    r.a02 = a02 * k;
    r.a10 = a10 * k;
    r.a11 = a11 * k;
    r.a12 = a12 * k;
    r.a20 = a20 * k;
    r.a21 = a21 * k;
    r.a22 = a22 * k;
    return r;
}

Matrix3D &Matrix3D::operator*=( double k )
{
    a00 *= k;
    a01 *= k;
    a02 *= k;
    a10 *= k;
    a11 *= k;
    a12 *= k;
    a20 *= k;
    a21 *= k;
    a22 *= k;
    return *this;
}

Vector3D Matrix3D::operator*( const Vector3D& v ) const
{
    return Vector3D( a00 * v.x + a01 * v.y + a02 * v.z, a10 * v.x + a11 * v.y + a12 * v.z, a20 * v.x + a21 * v.y + a22 * v.z );
}

Matrix3D Matrix3D::operator/( double k ) const
{
    Matrix3D r;
    r.a00 = a00 / k;
    r.a01 = a01 / k;
    r.a02 = a02 / k;
    r.a10 = a10 / k;
    r.a11 = a11 / k;
    r.a12 = a12 / k;
    r.a20 = a20 / k;
    r.a21 = a21 / k;
    r.a22 = a22 / k;
    return r;
}

Matrix3D &Matrix3D::operator/=( double k )
{
    a00 /= k;
    a01 /= k;
    a02 /= k;
    a10 /= k;
    a11 /= k;
    a12 /= k;
    a20 /= k;
    a21 /= k;
    a22 /= k;
    return *this;
}

Matrix3D Matrix3D::operator+()const
{
    Matrix3D r;
    r.a00 = +a00;
    r.a01 = +a01;
    r.a02 = +a02;
    r.a10 = +a10;
    r.a11 = +a11;
    r.a12 = +a12;
    r.a20 = +a20;
    r.a21 = +a21;
    r.a22 = +a22;
    return r;
}

Matrix3D Matrix3D::operator+( const Matrix3D &a ) const
{
    Matrix3D r;
    r.a00 = a00 + a.a00;
    r.a01 = a01 + a.a01;
    r.a02 = a02 + a.a02;
    r.a10 = a10 + a.a10;
    r.a11 = a11 + a.a11;
    r.a12 = a12 + a.a12;
    r.a20 = a20 + a.a20;
    r.a21 = a21 + a.a21;
    r.a22 = a22 + a.a22;
    return r;
}

Matrix3D &Matrix3D::operator+=( const Matrix3D &a )
{
    a00 += a.a00;
    a01 += a.a01;
    a02 += a.a02;
    a10 += a.a10;
    a11 += a.a11;
    a12 += a.a12;
    a20 += a.a20;
    a21 += a.a21;
    a22 += a.a22;
    return *this;
}

Matrix3D Matrix3D::operator-()const
{
    Matrix3D r;
    r.a00 = -a00;
    r.a01 = -a01;
    r.a02 = -a02;
    r.a10 = -a10;
    r.a11 = -a11;
    r.a12 = -a12;
    r.a20 = -a20;
    r.a21 = -a21;
    r.a22 = -a22;
    return r;
}

Matrix3D Matrix3D::operator-( const Matrix3D &a ) const
{
    Matrix3D r;
    r.a00 = a00 - a.a00;
    r.a01 = a01 - a.a01;
    r.a02 = a02 - a.a02;
    r.a10 = a10 - a.a10;
    r.a11 = a11 - a.a11;
    r.a12 = a12 - a.a12;
    r.a20 = a20 - a.a20;
    r.a21 = a21 - a.a21;
    r.a22 = a22 - a.a22;
    return r;
}

Matrix3D &Matrix3D::operator-=( const Matrix3D &a )
{
    a00 -= a.a00;
    a01 -= a.a01;
    a02 -= a.a02;
    a10 -= a.a10;
    a11 -= a.a11;
    a12 -= a.a12;
    a20 -= a.a20;
    a21 -= a.a21;
    a22 -= a.a22;
    return *this;
}

bool Matrix3D::operator==( const Matrix3D &a ) const
{
    return
        ( Abs( a00 - a.a00 ) <= epsilon ) &&
        ( Abs( a01 - a.a01 ) <= epsilon ) &&
        ( Abs( a02 - a.a02 ) <= epsilon ) &&

        ( Abs( a10 - a.a10 ) <= epsilon ) &&
        ( Abs( a11 - a.a11 ) <= epsilon ) &&
        ( Abs( a12 - a.a12 ) <= epsilon ) &&

        ( Abs( a20 - a.a20 ) <= epsilon ) &&
        ( Abs( a21 - a.a21 ) <= epsilon ) &&
        ( Abs( a22 - a.a22 ) <= epsilon );
}

bool Matrix3D::operator!=( const Matrix3D &a ) const
{
    return
        ( Abs( a00 - a.a00 ) > epsilon ) ||
        ( Abs( a01 - a.a01 ) > epsilon ) ||
        ( Abs( a02 - a.a02 ) > epsilon ) ||

        ( Abs( a10 - a.a10 ) > epsilon ) ||
        ( Abs( a11 - a.a11 ) > epsilon ) ||
        ( Abs( a12 - a.a12 ) > epsilon ) ||

        ( Abs( a20 - a.a20 ) > epsilon ) ||
        ( Abs( a21 - a.a21 ) > epsilon ) ||
        ( Abs( a22 - a.a22 ) > epsilon );
}

double Matrix3D::det() const
{
    return
        a00 * ( a11 * a22 - a12 * a21 )
        - a01 * ( a10 * a22 - a12 * a20 )
        + a02 * ( a10 * a21 - a11 * a20 );
}

Matrix3D Matrix3D::transpose() const
{
    return Matrix3D( a00, a10, a20,
                     a01, a11, a21,
                     a02, a12, a22 );
}

Matrix3D Matrix3D::cofactor() const
{
    return Matrix3D(
               +( a11 * a22 - a12 * a21 ), -( a10 * a22 - a12 * a20 ), +( a10 * a21 - a11 * a20 ),
               -( a01 * a22 - a02 * a21 ), +( a00 * a22 - a02 * a20 ), -( a00 * a21 - a01 * a20 ),
               +( a01 * a12 - a02 * a11 ), -( a00 * a12 - a02 * a10 ), +( a00 * a11 - a01 * a10 )
           );
}

Matrix3D Matrix3D::adjugate() const
{
    return cofactor().transpose();
}

Matrix3D Matrix3D::inv() const
{
    double d = det();
    makeException( Abs( d ) > epsilon );
    return adjugate() / d;
}

Matrix3D Matrix3D::Zero()
{
    return Matrix3D();
}

Matrix3D Matrix3D::Identity()
{
    return Matrix3D( 1, 0, 0, 0, 1, 0, 0, 0, 1 );
}

Matrix3D Matrix3D::Scale( double s )
{
    return Matrix3D( s, 0, 0, 0, s, 0, 0, 0, s );
}

Matrix3D Matrix3D::Scale( double xs, double ys, double zs )
{
    return Matrix3D( xs, 0, 0, 0, ys, 0, 0, 0, zs );
}

Matrix3D Matrix3D::Rotation( const Vector3D &axis, double angle )
{
    double c = Cos( angle );
    double s = Sin( angle );
    auto u = axis.Normal();
    return Matrix3D( u.x * u.x * ( 1 - c ) + c, u.x * u.y * ( 1 - c ) - u.z * s, u.x * u.z * ( 1 - c ) + u.y * s,
                     u.x * u.y * ( 1 - c ) + u.z * s, u.y * u.y * ( 1 - c ) + c, u.y * u.z * ( 1 - c ) - u.x * s,
                     u.x * u.z * ( 1 - c ) - u.y * s, u.y * u.z * ( 1 - c ) + u.x * s, u.z * u.z * ( 1 - c ) + c );
}

Matrix3D operator*( double k, const Matrix3D &a )
{
    return a * k;
}
