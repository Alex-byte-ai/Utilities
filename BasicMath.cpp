#include "BasicMath.h"

#include <math.h>
#include <cstdint>

#include "Exception.h"

float ModF( float a, float b )
{
    return fmod( a, b );
}

double ModF( double a, double b )
{
    return fmod( a, b );
}

long double ModF( long double a, long double b )
{
    return fmod( a, b );
}

double ModF( int a, double b )
{
    return fmod( a, b );
}

float Sqrt( float a )
{
    return sqrt( a );
}

double Sqrt( double a )
{
    return sqrt( a );
}

long double Sqrt( long double a )
{
    return sqrt( a );
}

double Sqrt( int a )
{
    return sqrt( ( double )a );
}

float Sin( float a )
{
    return sin( a );
}

double Sin( double a )
{
    return sin( a );
}

long double Sin( long double a )
{
    return sin( a );
}

double Sin( int a )
{
    return sin( ( double )a );
}

float Cos( float a )
{
    return cos( a );
}

double Cos( double a )
{
    return cos( a );
}

long double Cos( long double a )
{
    return cos( a );
}

double Cos( int a )
{
    return cos( ( double )a );
}

float ArcTan2( float y, float x )
{
    return atan2( y, x );
}

double ArcTan2( double y, double x )
{
    return atan2( y, x );
}

long double ArcTan2( long double y, long double x )
{
    return atan2( y, x );
}

double ArcTan2( int y, int x )
{
    return atan2( ( double )y, ( double )x );
}

float ArcSin( float a )
{
    return asin( a );
}

double ArcSin( double a )
{
    return asin( a );
}

long double ArcSin( long double a )
{
    return asin( a );
}

double ArcSin( int a )
{
    return asin( ( double )a );
}

float ArcCos( float a )
{
    return acos( a );
}

double ArcCos( double a )
{
    return acos( a );
}

long double ArcCos( long double a )
{
    return acos( a );
}

double ArcCos( int a )
{
    return acos( ( double )a );
}

float Exp( float a )
{
    return exp( a );
}

double Exp( double a )
{
    return exp( a );
}

long double Exp( long double a )
{
    return exp( a );
}

double Exp( int a )
{
    return exp( ( double )a );
}

float Pow( float a, float b )
{
    return pow( a, b );
}

double Pow( double a, double b )
{
    return pow( a, b );
}

long double Pow( long double a, long double b )
{
    return pow( a, b );
}

double Pow( int a, double b )
{
    return pow( ( double )a, b );
}

int Pow( int a, int b )
{
    return pow( a, b );
}

// In these procedures, it's must do
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
template <typename Int, typename Float>
Int RoundDownT( Float value )
{
    auto minimum = std::numeric_limits<Int>::lowest();
    auto maximum = std::numeric_limits<Int>::max();

    makeException( minimum <= value && value - 1 < maximum );

    Int truncated = value;
    if( truncated == value )
        return truncated;

    if( value < 0 )
    {
        makeException( truncated > minimum );
        truncated -= 1;
    }

    return truncated;
}

template <typename Int, typename Float>
Int RoundUpT( Float value )
{
    auto minimum = std::numeric_limits<Int>::lowest();
    auto maximum = std::numeric_limits<Int>::max();

    makeException( minimum < value + 1 && value <= maximum );

    Int truncated = value;
    if( truncated == value )
        return truncated;

    if( value > 0 )
    {
        makeException( truncated < maximum );
        truncated += 1;
    }
    return truncated;
}

template <typename Int, typename Float>
Int RoundT( Float value )
{
    auto minimum = std::numeric_limits<Int>::lowest();
    auto maximum = std::numeric_limits<Int>::max();

    makeException( minimum % 2 == 0 ? minimum <= value + 0.5 : minimum < value + 0.5 );
    makeException( maximum % 2 == 0 ? value - 0.5 <= maximum : value - 0.5 < maximum );

    Int truncated = value;
    if( truncated == value )
        return truncated;

    Float frac = value - truncated;
    Float absFrac = Abs( frac );

    if( absFrac < Float( 0.5 ) )
        return truncated;

    if( absFrac > Float( 0.5 ) )
        return ( value > 0 ) ? truncated + 1 : truncated - 1;

    // Exactly 0.5, round to the nearest even integer.
    if( truncated % 2 != 0 )
        return ( value > 0 ) ? truncated + 1 : truncated - 1;

    return truncated;
}
#pragma GCC diagnostic pop

long long RoundDown( float a )
{
    return RoundDownT<long long, float>( a );
}

long long RoundDown( double a )
{
    return RoundDownT<long long, double>( a );
}

long long RoundDown( long double a )
{
    return RoundDownT<long long, long double>( a );
}

long long RoundUp( float a )
{
    return RoundUpT<long long, float>( a );
}

long long RoundUp( double a )
{
    return RoundUpT<long long, double>( a );
}

long long RoundUp( long double a )
{
    return RoundUpT<long long, long double>( a );
}

long long Round( float a )
{
    return RoundT<long long, float>( a );
}

long long Round( double a )
{
    return RoundT<long long, double>( a );
}

long long Round( long double a )
{
    return RoundT<long long, long double>( a );
}
