#include "Matrix2D.h"

#include "Exception.h"
#include "Basic.h"

double Matrix2D::epsilon = 1e-6;

Matrix2D::Matrix2D()
{
    a00 = a01 = a10 = a11 = 0;
}

Matrix2D::Matrix2D( double a00_, double a01_, double a10_, double a11_ ): a00( a00_ ), a01( a01_ ), a10( a10_ ), a11( a11_ ) {}

Matrix2D Matrix2D::operator*( const Matrix2D &a ) const
{
    return Matrix2D( a00 * a.a00 + a01 * a.a10, a00 * a.a01 + a01 * a.a11, a10 * a.a00 + a11 * a.a10, a10 * a.a01 + a11 * a.a11 );
}

Matrix2D &Matrix2D::operator*=( const Matrix2D &a )
{
    Matrix2D r = *this * a;
    *this = r;
    return *this;
}

Matrix2D Matrix2D::operator*( double k ) const
{
    Matrix2D r;
    r.a00 = a00 * k;
    r.a01 = a01 * k;
    r.a10 = a10 * k;
    r.a11 = a11 * k;
    return r;
}

Matrix2D &Matrix2D::operator*=( double k )
{
    a00 *= k;
    a01 *= k;
    a10 *= k;
    a11 *= k;
    return *this;
}

Matrix2D Matrix2D::operator/( double k ) const
{
    Matrix2D r;
    r.a00 = a00 / k;
    r.a01 = a01 / k;
    r.a10 = a10 / k;
    r.a11 = a11 / k;
    return r;
}

Matrix2D &Matrix2D::operator/=( double k )
{
    a00 /= k;
    a01 /= k;
    a10 /= k;
    a11 /= k;
    return *this;
}

Matrix2D Matrix2D::operator+()const
{
    Matrix2D r;
    r.a00 = +a00;
    r.a01 = +a01;
    r.a10 = +a10;
    r.a11 = +a11;
    return r;
}

Matrix2D Matrix2D::operator+( const Matrix2D &a ) const
{
    Matrix2D r;
    r.a00 = a00 + a.a00;
    r.a01 = a01 + a.a01;
    r.a10 = a10 + a.a10;
    r.a11 = a11 + a.a11;
    return r;
}

Matrix2D &Matrix2D::operator+=( const Matrix2D &a )
{
    a00 += a.a00;
    a01 += a.a01;
    a10 += a.a10;
    a11 += a.a11;
    return *this;
}

Matrix2D Matrix2D::operator-()const
{
    Matrix2D r;
    r.a00 = -a00;
    r.a01 = -a01;
    r.a10 = -a10;
    r.a11 = -a11;
    return r;
}

Matrix2D Matrix2D::operator-( const Matrix2D &a ) const
{
    Matrix2D r;
    r.a00 = a00 - a.a00;
    r.a01 = a01 - a.a01;
    r.a10 = a10 - a.a10;
    r.a11 = a11 - a.a11;
    return r;
}

Matrix2D &Matrix2D::operator-=( const Matrix2D &a )
{
    a00 -= a.a00;
    a01 -= a.a01;
    a10 -= a.a10;
    a11 -= a.a11;
    return *this;
}

bool Matrix2D::operator==( const Matrix2D &a ) const
{
    return
        ( Abs( a00 - a.a00 ) <= epsilon ) &&
        ( Abs( a01 - a.a01 ) <= epsilon ) &&
        ( Abs( a10 - a.a10 ) <= epsilon ) &&
        ( Abs( a11 - a.a11 ) <= epsilon );
}

bool Matrix2D::operator!=( const Matrix2D &a ) const
{
    return
        ( Abs( a00 - a.a00 ) > epsilon ) ||
        ( Abs( a01 - a.a01 ) > epsilon ) ||
        ( Abs( a10 - a.a10 ) > epsilon ) ||
        ( Abs( a11 - a.a11 ) > epsilon );
}

double Matrix2D::det() const
{
    return a00 * a11 - a10 * a01;
}

Matrix2D Matrix2D::transpose() const
{
    return Matrix2D( a00, a10, a01, a11 );
}

Matrix2D Matrix2D::cofactor() const
{
    return Matrix2D( a11, -a10, -a01,  a00 );
}

Matrix2D Matrix2D::adjugate() const
{
    return cofactor().transpose();
}

Matrix2D Matrix2D::inv() const
{
    double d = det();
    makeException( Abs( d ) > epsilon );
    return adjugate() / d;
}

Matrix2D Matrix2D::Zero()
{
    return Matrix2D();
}

Matrix2D Matrix2D::Identity()
{
    return Matrix2D( 1, 0, 0, 1 );
}

Matrix2D Matrix2D::Scale( double s )
{
    return Matrix2D( s, 0, 0, s );
}

Matrix2D Matrix2D::Scale( double xs, double ys )
{
    return Matrix2D( xs, 0, 0, ys );
}

Matrix2D Matrix2D::Rotation( double angle )
{
    double c = Cos( angle );
    double s = Sin( angle );
    return Matrix2D( c, -s, s, c );
}

Matrix2D operator*( double k, const Matrix2D &a )
{
    return a * k;
}
