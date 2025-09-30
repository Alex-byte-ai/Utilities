#include "Matrix4D.h"

#include "Exception.h"
#include "Basic.h"

double Matrix4D::epsilon = 1e-6;

Matrix4D::Matrix4D()
{
    a00 = a01 = a02 = a03 = a10 = a11 = a12 = a13 = a20 = a21 = a22 = a23 = a30 = a31 = a32 = a33 = 0;
}

Matrix4D::Matrix4D( double a00_, double a01_, double a02_, double a03_,
                    double a10_, double a11_, double a12_, double a13_,
                    double a20_, double a21_, double a22_, double a23_,
                    double a30_, double a31_, double a32_, double a33_ )
    : a00( a00_ ), a01( a01_ ), a02( a02_ ), a03( a03_ ),
      a10( a10_ ), a11( a11_ ), a12( a12_ ), a13( a13_ ),
      a20( a20_ ), a21( a21_ ), a22( a22_ ), a23( a23_ ),
      a30( a30_ ), a31( a31_ ), a32( a32_ ), a33( a33_ )
{}

Matrix4D Matrix4D::operator*( const Matrix4D &a ) const
{
    Matrix4D r;
    r.a00 = a00 * a.a00 + a01 * a.a10 + a02 * a.a20 + a03 * a.a30;
    r.a01 = a00 * a.a01 + a01 * a.a11 + a02 * a.a21 + a03 * a.a31;
    r.a02 = a00 * a.a02 + a01 * a.a12 + a02 * a.a22 + a03 * a.a32;
    r.a03 = a00 * a.a03 + a01 * a.a13 + a02 * a.a23 + a03 * a.a33;

    r.a10 = a10 * a.a00 + a11 * a.a10 + a12 * a.a20 + a13 * a.a30;
    r.a11 = a10 * a.a01 + a11 * a.a11 + a12 * a.a21 + a13 * a.a31;
    r.a12 = a10 * a.a02 + a11 * a.a12 + a12 * a.a22 + a13 * a.a32;
    r.a13 = a10 * a.a03 + a11 * a.a13 + a12 * a.a23 + a13 * a.a33;

    r.a20 = a20 * a.a00 + a21 * a.a10 + a22 * a.a20 + a23 * a.a30;
    r.a21 = a20 * a.a01 + a21 * a.a11 + a22 * a.a21 + a23 * a.a31;
    r.a22 = a20 * a.a02 + a21 * a.a12 + a22 * a.a22 + a23 * a.a32;
    r.a23 = a20 * a.a03 + a21 * a.a13 + a22 * a.a23 + a23 * a.a33;

    r.a30 = a30 * a.a00 + a31 * a.a10 + a32 * a.a20 + a33 * a.a30;
    r.a31 = a30 * a.a01 + a31 * a.a11 + a32 * a.a21 + a33 * a.a31;
    r.a32 = a30 * a.a02 + a31 * a.a12 + a32 * a.a22 + a33 * a.a32;
    r.a33 = a30 * a.a03 + a31 * a.a13 + a32 * a.a23 + a33 * a.a33;
    return r;
}

Matrix4D &Matrix4D::operator*=( const Matrix4D &a )
{
    *this = *this * a;
    return *this;
}

Matrix4D Matrix4D::operator*( double k ) const
{
    Matrix4D r;
    r.a00 = a00 * k;
    r.a01 = a01 * k;
    r.a02 = a02 * k;
    r.a03 = a03 * k;
    r.a10 = a10 * k;
    r.a11 = a11 * k;
    r.a12 = a12 * k;
    r.a13 = a13 * k;
    r.a20 = a20 * k;
    r.a21 = a21 * k;
    r.a22 = a22 * k;
    r.a23 = a23 * k;
    r.a30 = a30 * k;
    r.a31 = a31 * k;
    r.a32 = a32 * k;
    r.a33 = a33 * k;
    return r;
}

Matrix4D &Matrix4D::operator*=( double k )
{
    a00 *= k;
    a01 *= k;
    a02 *= k;
    a03 *= k;
    a10 *= k;
    a11 *= k;
    a12 *= k;
    a13 *= k;
    a20 *= k;
    a21 *= k;
    a22 *= k;
    a23 *= k;
    a30 *= k;
    a31 *= k;
    a32 *= k;
    a33 *= k;
    return *this;
}

Vector4D Matrix4D::operator*( const Vector4D& v ) const
{
    return Vector4D( a00 * v.x + a01 * v.y + a02 * v.z + a03 * v.w,
                     a10 * v.x + a11 * v.y + a12 * v.z + a13 * v.w,
                     a20 * v.x + a21 * v.y + a22 * v.z + a23 * v.w,
                     a30 * v.x + a31 * v.y + a32 * v.z + a33 * v.w );
}

Matrix4D Matrix4D::operator/( double k ) const
{
    Matrix4D r = *this;
    return r *= ( 1.0 / k );
}

Matrix4D &Matrix4D::operator/=( double k )
{
    return *this *= ( 1.0 / k );
}

Matrix4D Matrix4D::operator+() const
{
    return *this;
}

Matrix4D Matrix4D::operator+( const Matrix4D &a ) const
{
    Matrix4D r;
#define ADD(i,j) r.a##i##j = a##i##j + a.a##i##j;
    ADD( 0, 0 ) ADD( 0, 1 ) ADD( 0, 2 ) ADD( 0, 3 )
    ADD( 1, 0 ) ADD( 1, 1 ) ADD( 1, 2 ) ADD( 1, 3 )
    ADD( 2, 0 ) ADD( 2, 1 ) ADD( 2, 2 ) ADD( 2, 3 )
    ADD( 3, 0 ) ADD( 3, 1 ) ADD( 3, 2 ) ADD( 3, 3 )
#undef ADD
    return r;
}

Matrix4D &Matrix4D::operator+=( const Matrix4D &a )
{
#define ADD(i,j) a##i##j += a.a##i##j;
    ADD( 0, 0 ) ADD( 0, 1 ) ADD( 0, 2 ) ADD( 0, 3 )
    ADD( 1, 0 ) ADD( 1, 1 ) ADD( 1, 2 ) ADD( 1, 3 )
    ADD( 2, 0 ) ADD( 2, 1 ) ADD( 2, 2 ) ADD( 2, 3 )
    ADD( 3, 0 ) ADD( 3, 1 ) ADD( 3, 2 ) ADD( 3, 3 )
#undef ADD
    return *this;
}

Matrix4D Matrix4D::operator-() const
{
    return *this * -1;
}

Matrix4D Matrix4D::operator-( const Matrix4D &a ) const
{
    return *this + ( -a );
}

Matrix4D &Matrix4D::operator-=( const Matrix4D &a )
{
    return *this += ( -a );
}

bool Matrix4D::operator==( const Matrix4D &a ) const
{
#define CMP(i,j) ( Abs( a##i##j - a.a##i##j ) <= epsilon )
    return CMP( 0, 0 ) && CMP( 0, 1 ) && CMP( 0, 2 ) && CMP( 0, 3 ) &&
           CMP( 1, 0 ) && CMP( 1, 1 ) && CMP( 1, 2 ) && CMP( 1, 3 ) &&
           CMP( 2, 0 ) && CMP( 2, 1 ) && CMP( 2, 2 ) && CMP( 2, 3 ) &&
           CMP( 3, 0 ) && CMP( 3, 1 ) && CMP( 3, 2 ) && CMP( 3, 3 );
#undef CMP
}

bool Matrix4D::operator!=( const Matrix4D &a ) const
{
    return !( *this == a );
}

// Determinant, cofactor, adjugate, and inverse functions for 4x4 can be implemented if needed.

Matrix4D Matrix4D::transpose() const
{
    return Matrix4D(
               a00, a10, a20, a30,
               a01, a11, a21, a31,
               a02, a12, a22, a32,
               a03, a13, a23, a33 );
}

Matrix4D Matrix4D::Zero()
{
    return Matrix4D();
}

Matrix4D Matrix4D::Identity()
{
    return Matrix4D(
               1, 0, 0, 0,
               0, 1, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1 );
}

Matrix4D Matrix4D::Perspective( double fovY, double aspect, double zNear, double zFar )
{
    double f = 1.0 / Tan( fovY * 0.5 );
    double A = ( zFar + zNear ) / ( zNear - zFar );
    double B = ( 2.0 * zFar * zNear ) / ( zNear - zFar );

    return Matrix4D(
               f / aspect, 0.0,  0.0,  0.0,
               0.0,        f,    0.0,  0.0,
               0.0,        0.0,   A,    B,
               0.0,        0.0,  -1.0,  0.0
           );
}

Matrix4D Matrix4D::Orthographic( double left, double right, double bottom, double top, double zNear, double zFar )
{
    // scale terms
    double sx =  2.0 / ( right - left );
    double sy =  2.0 / ( top   - bottom );
    double sz = -2.0 / ( zFar  - zNear );

    // translation terms
    double tx = -( right + left )   / ( right - left );
    double ty = -( top   + bottom ) / ( top   - bottom );
    double tz = -( zFar  + zNear )  / ( zFar  - zNear );

    return Matrix4D(
               sx,   0.0, 0.0, tx,
               0.0,  sy,  0.0, ty,
               0.0,  0.0, sz,  tz,
               0.0,  0.0, 0.0, 1.0
           );
}

Matrix4D operator*( double k, const Matrix4D &a )
{
    return a * k;
}
