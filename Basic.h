#pragma once

#include <type_traits>
#include <limits>
#include <cmath>

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
    static_assert( std::is_arithmetic_v<A> && std::is_arithmetic_v<B> );
    using R = std::common_type_t<A, B>;

    if constexpr( std::is_integral_v<A> && std::is_integral_v<B> )
    {
        auto result = a % b;
        if( result >= 0 )
            return result;
        if( b > 0 )
            return result + b;
        return result - b;
    }
    else
    {
        R result = std::fmod( a, b );
        if( result >= 0 )
            return result;
        if( b > 0 )
            return result + b;
        return result - b;
    }
}

template<typename A, typename B>
inline auto Div( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> && std::is_arithmetic_v<B> );
    using R = std::common_type_t<A, B>;

    if constexpr( std::is_integral_v<A> && std::is_integral_v<B> )
    {
        auto result = a / b;
        if( a % b >= 0 )
            return result;
        if( b > 0 )
            return result - 1;
        return result + 1;
    }
    else
    {
        R q = std::floor( static_cast<R>( a ) / static_cast<R>( b ) );
        R r = std::fmod( static_cast<R>( a ), static_cast<R>( b ) );
        if( r >= 0 )
            return q;
        if( b > 0 )
            return q - 1;
        return q + 1;
    }
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

template<typename A>
A Sqrt( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::sqrt( a );
}

template<typename A>
A Sin( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::sin( a );
}

template<typename A>
A Cos( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::cos( a );
}

template<typename A>
A Tan( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::tan( a );
}

template<typename A, typename B>
auto ArcTan2( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    return std::atan2( a, b );
}

template<typename A>
A ArcSin( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::asin( a );
}

template<typename A>
A ArcCos( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::acos( a );
}

template<typename A>
A Exp( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::exp( a );
}

template<typename A, typename B>
auto Pow( A a, B b )
{
    static_assert( std::is_arithmetic_v<A> &&std::is_arithmetic_v<B> );
    return std::pow( a, b );
}

template<typename A = double>
constexpr A Pi()
{
    static_assert( std::is_floating_point_v<A> );
    return ArcTan2( A( 0 ), A( -1 ) );
}

template<typename A>
A RoundDown( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::floor( a );
}

template<typename A>
A RoundUp( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::ceil( a );
}

template<typename A>
A Round( A a )
{
    static_assert( std::is_arithmetic_v<A> );
    return std::nearbyint( a );
}

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

    void add( T x )
    {
        if( x > b ) b = x;
        if( x < a ) a = x;
    };

    T leftBorder() const
    {
        return a;
    };

    T rightBorder() const
    {
        return b;
    };

    T length() const
    {
        return Max( b - a, 0 );
    };

    bool test( T x ) const
    {
        return a <= x && x <= b;
    };

    long double normalize( T x ) const
    {
        if( a < b )
            return ( ( long double )( x - a ) ) / ( ( long double )( b - a ) );
        return x;
    };

    long double interpolate( long double x ) const
    {
        if( a < b )
            return a + ( b - a ) * x;
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

void copy( void *destination, const void *source, unsigned bytes );
void move( void *destination, const void *source, unsigned bytes );
void clear( void *destination, unsigned bytes );
void clear( void *destination, unsigned char sample, unsigned bytes );
bool compare( const void *source0, const void *source1, unsigned bytes );
bool compare( const wchar_t *string0, const wchar_t *string1 );
unsigned stringLength( const char *string );
