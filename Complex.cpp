#include "Complex.h"

#include "Basic.h"

double Complex::epsilon = 1e-6;

Complex::Complex(): a( 0 ), b( 0 ) {}

Complex::Complex( double a_, double b_ ): a( a_ ), b( b_ ) {}

Complex Complex::operator+()const
{
    return Complex( +a, +b );
}

Complex Complex::operator+( const Complex &x ) const
{
    return Complex( a + x.a, b + x.b );
}

Complex &Complex::operator+=( const Complex &x )
{
    a += x.a;
    b += x.b;
    return *this;
}

Complex Complex::operator-()const
{
    return Complex( -a, -b );
}

Complex Complex::operator-( const Complex &x ) const
{
    return Complex( a - x.a, b - x.b );
}

Complex &Complex::operator-=( const Complex &x )
{
    a -= x.a;
    b -= x.b;
    return *this;
}

Complex Complex::operator*( double k ) const
{
    return Complex( a * k, b * k );
}

Complex &Complex::operator*=( double k )
{
    a *= k;
    b *= k;
    return *this;
}

Complex Complex::operator/( double k ) const
{
    return Complex( a / k, b / k );
}

Complex &Complex::operator/=( double k )
{
    a /= k;
    b /= k;
    return *this;
}

Complex Complex::operator*( const Complex &x ) const
{
    return Complex( a * x.a - b * x.b, a * x.b + b * x.a );
}

bool Complex::operator==( const Complex &x ) const
{
    return ( ::Abs( a - x.a ) <= epsilon ) && ( ::Abs( b - x.b ) <= epsilon );
}

bool Complex::operator!=( const Complex &x ) const
{
    return ( ::Abs( a - x.a ) > epsilon ) && ( ::Abs( b - x.b ) > epsilon );
}

double Complex::Abs() const
{
    return Sqrt( a * a + b * b );
}

double Complex::Arg() const
{
    return ArcTan2( b, a );
}

Complex Complex::Normal() const
{
    double l = Abs();
    if( l > 0 )
        return ( *this ) / l;
    return *this;
}

Complex operator*( double k, const Complex &a )
{
    return a * k;
}
