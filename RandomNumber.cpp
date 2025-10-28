#include "RandomNumber.h"

void RandomNumber::init()
{
    a = 16807;
    m = 2147483647;
    q = m / a;
    r = m % a;
}

void RandomNumber::next()
{
    long long int gamma = a * ( z % q ) - r * ( z / q );
    if( gamma > 0 )
    {
        z = gamma;
    }
    else
    {
        z = gamma + m;
    }
}

RandomNumber::RandomNumber() : RandomNumber( 314159 )
{}

RandomNumber::RandomNumber( long long int seed )
{
    init();
    setSeed( seed );
}

void RandomNumber::setSeed( long long int seed )
{
    z = seed;
}

long long int RandomNumber::getInteger( long long int start, long long int finish )
{
    // There are finish - start + 1 numbers between start and finish.

    long long int result = z % ( finish - start + 1 ) + start; // Can be not uniform, because interval in which z can end up is large, but not might not be divisible by b - a + 1
    next();
    return result;
}

double RandomNumber::getReal( double start, double finish )
{
    double result = z * ( finish - start ) / m + start; // Might not include a or b, if z never reaches 0 or m
    next();
    return result;
}

long long int RandomNumber::getInteger( const IntegerInterval &i )
{
    return getInteger( i.a, i.b );
}

double RandomNumber::getReal( const RealInterval &i )
{
    return getReal( i.a, i.b );
}
