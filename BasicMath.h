#pragma once

#undef min
#undef max

#include <type_traits>
#include <limits>

#include "Exception.h"

template<typename A, typename B>
inline decltype( A() + B() ) Min( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    static_assert( std::is_unsigned_v<A> == std::is_unsigned_v<B> );
    if( a < b )
        return a;
    return b;
}

template<typename A, typename B>
inline decltype( A() + B() ) Max( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    static_assert( std::is_unsigned_v<A> == std::is_unsigned_v<B> );
    if( a > b )
        return a;
    return b;
}

template<typename A>
inline A Sqr( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return a * a;
}

template<typename A>
inline short Sign( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    if( a < A( 0 ) )
        return -1;
    if( a > A( 0 ) )
        return 1;
    return 0;
}

template<typename A>
inline A Abs( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    if( a >= A( 0 ) )
        return a;
    return -a;
}

template<typename A, typename B>
inline bool SameSign( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    return ( ( a >= A( 0 ) ) && ( b >= B( 0 ) ) ) || ( ( a <= A( 0 ) ) && ( b <= B( 0 ) ) );
}

template<typename A, typename B>
inline bool SameSignStrict( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    return ( ( a > A( 0 ) ) && ( b > B( 0 ) ) ) || ( ( a < A( 0 ) ) && ( b < B( 0 ) ) ) || ( ( a == A( 0 ) ) && ( b == B( 0 ) ) );
}

// Mod and Div are mathematically correct.
// https://en.wikipedia.org/wiki/Euclidean_division

template<typename A, typename B>
inline auto Mod( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    static_assert( std::is_integral_v<A> &&std::is_integral_v<B> );
    auto result = a % b;
    if( result >= 0 )
        return result;
    if( b > 0 )
        return result + b;
    return result - b;
}

template<typename A, typename B>
inline auto Div( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    static_assert( std::is_integral_v<A> &&std::is_integral_v<B> );
    auto result = a / b;
    if( a % b >= 0 )
        return result;
    if( b > 0 )
        return result - 1;
    return result + 1;
}

template<typename A>
bool isNumber( A a )
{
    return a <= a;
}

template<typename A>
bool isInfinite( A a )
{
    return a >= A( 1 ) / A( 0 ) || a <= A( -1 ) / A( 0 );
}

float ModF( float a, float b );
double ModF( double a, double b );
long double ModF( long double a, long double b );
double ModF( int a, double b );

float Sqrt( float a );
double Sqrt( double a );
long double Sqrt( long double a );
double Sqrt( int a );

float Sin( float a );
double Sin( double a );
long double Sin( long double a );
double Sin( int a );

float Cos( float a );
double Cos( double a );
long double Cos( long double a );
double Cos( int a );

float ArcTan2( float y, float x );
double ArcTan2( double y, double x );
long double ArcTan2( long double y, long double x );
double ArcTan2( int y, int x );

float ArcSin( float a );
double ArcSin( double a );
long double ArcSin( long double a );
double ArcSin( int a );

float ArcCos( float a );
double ArcCos( double a );
long double ArcCos( long double a );
double ArcCos( int a );

float Exp( float a );
double Exp( double a );
long double Exp( long double a );
double Exp( int a );

float Pow( float a, float b );
double Pow( double a, double b );
long double Pow( long double a, long double b );
double Pow( int a, double b );
int Pow( int a, int b );

template<typename A = double>
constexpr A Pi()
{
    static_assert( std::is_floating_point_v<A> );
    return ArcTan2( A( 0 ), A( -1 ) );
}

long long RoundDown( float a );
long long RoundDown( double a );
long long RoundDown( long double a );

long long RoundUp( float a );
long long RoundUp( double a );
long long RoundUp( long double a );

long long Round( float a );
long long Round( double a );
long long Round( long double a );

template<typename T>
class Interval
{
private:
    T a, b;
public:
    // Add union???

    Interval()
    {
        a = std::numeric_limits<T>::max();
        b = std::numeric_limits<T>::lowest();
    };

    Interval( const Interval &other )
    {
        a = other.a;
        b = other.b;
    };

    Interval &operator=( const Interval &other )
    {
        a = other.a;
        b = other.b;
        return *this;
    };

    T leftBorder() const
    {
        return a;
    };

    T rightBorder() const
    {
        return b;
    };

    bool test( T x )
    {
        return ( a <= x ) && ( x <= b );
    };

    void add( T x )
    {
        if( x > b ) b = x;
        if( x < a ) a = x;
    };

    long double normalize( T x ) const
    {
        if( a < b )
            return ( ( long double )( x - a ) ) / ( ( long double )( b - a ) );
        return x;
    };

    bool empty() const
    {
        return a > b;
    };

    Interval intersection( const Interval &other ) const
    {
        Interval result;
        result.a = Max( a, other.a );
        result.b = Min( b, other.b );
        return result;
    };
};
